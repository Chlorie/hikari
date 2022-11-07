#include "hikari/api.h"

#include <unordered_map>
#include <deque>
#include <algorithm>
#include <fmt/format.h>
#include <clu/indices.h>

namespace hkr
{
    namespace
    {
        /*
         * Music grammar reference:
         *
         * %120,4/4,7s,#1%  // Tempo=120, Time=4/4, Key=7 sharps, Transpose +1 semitone
         * !a: ABC>!        // Set macro a to be "ABC>"
         * !b: *a* DE *a*!  // Set macro b to be "ABC> DE ABC>"
         * *a*;*b*|[*a*;*b*]
         */

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
                    return fmt::format("in macro {}, defined at line {}, column {},\n{}", //
                        entry.name, entry.definition_position.line(), entry.definition_position.column(), //
                        pos_in_macro.to_string());
                return fmt::format("in macro {}, at line {}, column {}", //
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
                std::size_t line = 0, column = 0;
                for (const char ch : text_)
                {
                    switch (ch)
                    {
                        case '\r': continue;
                        case '\n':
                            line++;
                            column = 0;
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
                    throw ParseError(fmt::format("{} expands exceeding the character limit of {} characters, {}", //
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
                    map.content += macro_map.content;
                    for (const auto [i] : clu::indices(macro_map.content.size()))
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
                // TODO
                return {};
            }

        private:
            PreprocessedText text_;
        };
    } // namespace

    Music parse_music(std::string text) { return Parser(std::move(text)).parse(); }
} // namespace hkr
