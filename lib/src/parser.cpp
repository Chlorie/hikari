#include "hikari/api.h"

#include <unordered_map>
#include <deque>
#include <algorithm>
#include <ranges>
#include <tuple>
#include <bit>

#include <fmt/format.h>
#include <clu/indices.h>
#include <clu/parse.h>

namespace hkr
{
    namespace
    {
        class ParseError final : public std::runtime_error
        {
        public:
            using std::runtime_error::runtime_error;
        };

        constexpr auto npos = std::string_view::npos;

        struct TextPositionMap;

        class TextPosition
        {
        public:
            TextPosition() noexcept = default;

            explicit TextPosition(TextPositionMap& map_entry, const std::size_t offset):
                data_(reinterpret_cast<std::uintptr_t>(&map_entry)), offset_(offset)
            {
            }

            explicit TextPosition(const std::size_t line, const std::size_t column)
            {
                set_line(line);
                set_column(column);
            }

            bool is_map_entry() const noexcept { return (data_ & 1) == 0; }

            // NOLINTNEXTLINE(performance-no-int-to-ptr)
            TextPositionMap& map_entry() const noexcept { return *reinterpret_cast<TextPositionMap*>(data_); }
            std::size_t offset() const noexcept { return offset_; }

            std::size_t line() const noexcept { return data_ >> 1; }
            std::size_t column() const noexcept { return offset_; }

            void set_line(const std::size_t value)
            {
                if (constexpr auto max = ~std::uintptr_t{} >> 1; value > max)
                    throw ParseError(fmt::format("Line number must be less than {}", max));
                data_ = (value << 1) | 1;
            }

            void set_column(const std::size_t value) noexcept { offset_ = value; }

            std::string to_string() const;

        private:
            std::uintptr_t data_ = 0;
            std::size_t offset_ = 0;
        };

        struct TextPositionMap
        {
            std::string name;
            std::string content;
            TextPosition definition_position;
            std::vector<TextPosition> positions;
        };
        static_assert(alignof(TextPositionMap) >= 2);

        std::string TextPosition::to_string() const
        {
            if (is_map_entry())
            {
                auto& entry = map_entry();
                const auto pos_in_macro = entry.positions[offset()];
                if (pos_in_macro.is_map_entry())
                    return fmt::format("in macro '{}', defined at line {}, column {},\n{}", //
                        entry.name, entry.definition_position.line(), entry.definition_position.column(), //
                        pos_in_macro.to_string());
                return fmt::format("in macro '{}', at line {}, column {}", //
                    entry.name, pos_in_macro.line(), pos_in_macro.column());
            }
            return fmt::format("at line {}, column {}", line(), column());
        }

        struct PreprocessedText
        {
            TextPositionMap text; // Main preprocessed text
            std::unordered_map<std::string, TextPositionMap*> macros; // Active macros
            std::deque<TextPositionMap> maps; // All macro information (including shadowed macros)
        };

        // The preprocessor reduces the original text with macros into a form with no macros.
        // The processed resultant text is produced, together with a map from each character
        // to its origin.
        class Preprocessor final
        {
        public:
            explicit Preprocessor(std::string text, const std::size_t max_macro_length = 65535):
                text_(std::move(text)), max_macro_length_(max_macro_length)
            {
            }

            PreprocessedText process()
            {
                remove_whitespaces();
                std::string_view view = text_;
                while (!view.empty())
                {
                    const auto idx = view.find_first_of("!*");
                    if (idx == npos)
                    {
                        append_text_to_map(res_.text, view);
                        break;
                    }
                    append_text_to_map(res_.text, view.substr(0, idx));
                    view.remove_prefix(idx);
                    if (view[0] == '!')
                        parse_consume_macro_def(view);
                    else
                        append_macro_to_map(res_.text, parse_consume_macro_ref(view));
                }
                return std::move(res_);
            }

        private:
            std::string text_;
            std::size_t max_macro_length_;
            std::vector<TextPosition> original_pos_;
            PreprocessedText res_;

            std::size_t offset_of(const std::string_view view) const noexcept
            {
                return static_cast<std::size_t>(view.data() - text_.data());
            }

            void remove_whitespaces()
            {
                std::string res;
                res.reserve(text_.size());
                std::size_t line = 1, column = 1;
                for (const char ch : text_)
                {
                    switch (ch)
                    {
                        case '\r': continue;
                        case '\n':
                            line++;
                            column = 1;
                            continue;
                        case ' ': column++; continue;
                        case '\t': column += 4; continue;
                        default:
                            res.push_back(ch);
                            original_pos_.emplace_back(line, column);
                            column++;
                            continue;
                    }
                }
                text_ = std::move(res);
            }

            void append_text_to_map(TextPositionMap& map, const std::string_view view) const
            {
                if (map.content.size() + view.size() > max_macro_length_)
                    throw ParseError(fmt::format("{} expands exceeding the character limit of {}, {}", //
                        map.name.empty() ? "Preprocessed text" : fmt::format("Macro '{}'", map.name), max_macro_length_,
                        original_pos_[offset_of(view)].to_string()));
                map.content += view;
                const auto offset = offset_of(view);
                for (const auto [i] : clu::indices(view.size()))
                    map.positions.push_back(original_pos_[offset + i]);
            }

            void append_macro_to_map(TextPositionMap& map, const std::string_view macro) const
            {
                if (const auto iter = res_.macros.find(std::string(macro)); iter == res_.macros.end())
                    throw ParseError(fmt::format("Referenced macro '{}' is not yet defined, {}", //
                        macro, original_pos_[offset_of(macro)].to_string()));
                else
                {
                    auto& macro_map = *iter->second;
                    const std::string_view view = macro_map.content;
                    if (map.content.size() + view.size() > max_macro_length_)
                        throw ParseError(fmt::format("{} expands exceeding the character limit of {}, {}", //
                            map.name.empty() ? "Preprocessed text" : fmt::format("Macro '{}'", map.name),
                            max_macro_length_, original_pos_[offset_of(macro)].to_string()));

                    map.content += view;
                    for (const auto [i] : clu::indices(view.size()))
                        map.positions.emplace_back(macro_map, i);
                }
            }

            void parse_consume_macro_def(std::string_view& view)
            {
                const auto def_pos = original_pos_[offset_of(view)];

                auto idx = view.find('!', 1);
                if (idx == npos)
                    throw ParseError(fmt::format("Macro definition is not closed with another '!' {}", //
                        def_pos.to_string()));
                std::string_view def_view = view.substr(1, idx - 1);
                view.remove_prefix(idx + 1);

                idx = def_view.find(':');
                if (idx == npos)
                    throw ParseError(fmt::format("No ':' found to separate macro name and content, at {}", //
                        def_pos.to_string()));
                const std::string_view macro_name = def_view.substr(0, idx);
                validate_macro_name(macro_name);
                def_view.remove_prefix(idx + 1);

                auto& map = res_.maps.emplace_back(
                    TextPositionMap{.name = std::string(macro_name), .definition_position = def_pos});
                while (!def_view.empty())
                {
                    idx = def_view.find('*');
                    if (idx == npos)
                    {
                        append_text_to_map(map, def_view);
                        break;
                    }
                    append_text_to_map(map, def_view.substr(0, idx));
                    def_view.remove_prefix(idx);
                    append_macro_to_map(map, parse_consume_macro_ref(def_view));
                }

                res_.macros[map.name] = &map;
            }

            std::string_view parse_consume_macro_ref(std::string_view& view) const
            {
                const auto idx = view.find('*', 1);
                if (idx == npos)
                    throw ParseError(fmt::format("Macro reference is not closed with another '*' {}",
                        original_pos_[offset_of(view)].to_string()));
                const auto name = view.substr(1, idx - 1);
                view.remove_prefix(idx + 1);
                return name;
            }

            void validate_macro_name(const std::string_view name) const
            {
                const auto pos = original_pos_[offset_of(name)];
                if (name.empty())
                    throw ParseError(fmt::format("Macro name is empty {}", pos.to_string()));
                if (!std::ranges::all_of(name,
                        [](const char ch)
                        {
                            return (ch >= 'a' && ch <= 'z') || //
                                (ch >= 'A' && ch <= 'Z') || //
                                (ch >= '0' && ch <= '9') || //
                                ch == '_';
                        }) ||
                    (name[0] >= '0' && name[0] <= '9'))
                    throw ParseError(fmt::format("Macro name {} is not a valid identifier (containing only"
                                                 "ASCII alphanumeric characters and underscores, not starting"
                                                 "with a digit), defined {}",
                        name, pos.to_string()));
            }
        };

        class Parser final
        {
        public:
            explicit Parser(std::string text): text_(Preprocessor(std::move(text)).process()) {}

            Music parse()
            {
                std::string_view text = text_.text.content;
                while (!text.empty())
                {
                    allow_measure_attrs_ = true;
                    while (parse_attributes(text)) {}
                    // Parse section
                }
                return std::move(music_);
            }

        private:
            PreprocessedText text_;
            Music music_;
            bool allow_measure_attrs_ = false;
            Measure::Attributes measure_attrs_;
            Chord::Attributes chord_attrs_;
            int transposition_ = 0;

            std::size_t offset_of(const std::string_view view) const noexcept
            {
                return static_cast<std::size_t>(view.data() - text_.text.content.data());
            }

            TextPosition pos_of(const std::string_view view, const std::size_t offset = 0) const noexcept
            {
                return text_.text.positions[offset_of(view) + offset];
            }

            /* Attributes */

            bool parse_attributes(std::string_view& text)
            {
                if (!text.starts_with('%'))
                    return false;
                const auto idx = text.find('%', 1);
                if (idx == npos)
                    throw ParseError("Attribute specification block is not closed with another '%', beginning " +
                        pos_of(text).to_string());
                std::string_view attrs_view = text.substr(1, idx - 1);
                text.remove_prefix(idx + 1);
                for (const auto view : std::views::split(attrs_view, ","))
                    parse_one_attribute(view);
                return true;
            }

            void parse_one_attribute(const std::string_view text)
            {
                if (text.empty())
                    throw ParseError("Empty attribute found " + pos_of(text).to_string());
                if (text[0] == '#' || text[0] == 'b')
                    parse_transposition(text);
                else if (text.find('/') != npos)
                    parse_time_signature(text);
                else if (text.back() == 's' || text.back() == 'f')
                    parse_key_signature(text);
                else
                    parse_tempo(text);
            }

            void parse_transposition(std::string_view text)
            {
                const int sign = text[0] == '#' ? 1 : -1;
                text.remove_prefix(1);
                if (const auto opt = clu::parse<int>(text); //
                    !opt || *opt < -12 || *opt > 12)
                    throw ParseError(fmt::format("Expecting an integer between -12 and 12 after "
                                                 "transposition indicator '{}', but found '{}' {}",
                        sign == 1 ? '#' : 'b', text, pos_of(text).to_string()));
                else
                    transposition_ = *opt;
            }

            void parse_time_signature(const std::string_view text)
            {
                if (!allow_measure_attrs_)
                    throw ParseError("Time signatures should only appear at the beginning of "
                                     "bars, but got a key signature in the middle of a bar " +
                        pos_of(text).to_string());

                const auto [partial, num_view, den_view] = [&]
                {
                    const auto slash = text.find('/'); // not npos
                    const bool p = text.size() > slash + 2 && text[slash + 2] == '/';
                    return std::tuple(p, text.substr(0, slash), text.substr(slash + (p ? 2 : 1)));
                }();

                const auto check_number = [&](const std::string_view num_text, const std::string_view name)
                {
                    const auto opt = clu::parse<int>(num_text);
                    if (!opt || *opt <= 0 || *opt > 128)
                        throw ParseError(fmt::format("The {} of a time signature should be a positive integer "
                                                     "no greater than 128, but got '{}' {}",
                            name, num_text, pos_of(num_text).to_string()));
                    return *opt;
                };

                const int num = check_number(num_view, "numerator");
                const int den = check_number(den_view, "denominator");

                if (!std::has_single_bit(static_cast<unsigned>(den)))
                    throw ParseError(fmt::format("The denominator of a time signature should be "
                                                 "a power of 2, but got {} {}",
                        den, pos_of(den_view).to_string()));

                (partial ? measure_attrs_.partial : measure_attrs_.time) = Time{num, den};
            }

            void parse_key_signature(std::string_view text)
            {
                if (!allow_measure_attrs_)
                    throw ParseError("Key signatures should only appear at the beginning of "
                                     "bars, but got a key signature in the middle of a bar " +
                        pos_of(text).to_string());
                const int sign = text.back() == 's' ? 1 : -1;
                text.remove_suffix(1);
                const auto opt = clu::parse<int>(text);
                if (!opt)
                    throw ParseError(fmt::format("A key signature specification should be a number followed by "
                                                 "'s' or 'f' to indicate the amount of sharps or flats in that "
                                                 "key signature, but got {}{} {}",
                        text, sign == 1 ? 's' : 'f', pos_of(text).to_string()));
                const int num = *opt;
                if (num < 0 || num > 7)
                    throw ParseError(fmt::format("The amount of sharps or flats in a key signature should be "
                                                 "between 0 and 7, but got {} {}",
                        num, pos_of(text).to_string()));
                measure_attrs_.key = num * sign;
            }

            void parse_tempo(const std::string_view text)
            {
                if (const auto opt = clu::parse<float>(text); !opt)
                    throw ParseError(fmt::format("Unknown attribute '{}' {}", text, pos_of(text).to_string()));
                else if (const float tempo = *opt; tempo > 1000 || tempo < 10)
                    throw ParseError(fmt::format("Tempo markings should be between 10 and 1000, but got {} {}", tempo,
                        pos_of(text).to_string()));
                else
                    chord_attrs_.tempo = tempo;
            }
        };
    } // namespace

    Music parse_music(std::string text) { return Parser(std::move(text)).parse(); }
} // namespace hkr
