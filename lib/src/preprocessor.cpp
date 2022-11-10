#include "preprocessor.h"

#include <algorithm>
#include <fmt/format.h>
#include <clu/indices.h>

namespace hkr
{
    Preprocessor::Preprocessor(std::string text, const std::size_t max_macro_length):
        text_(std::move(text)), max_macro_length_(max_macro_length)
    {
    }

    PreprocessedText Preprocessor::process()
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
        res_.text.positions.emplace_back(); // Add an EOF mark at the end
        return std::move(res_);
    }

    std::size_t Preprocessor::offset_of(const std::string_view view) const noexcept
    {
        return static_cast<std::size_t>(view.data() - text_.data());
    }

    void Preprocessor::remove_whitespaces()
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

    void Preprocessor::append_text_to_map(TextPositionMap& map, const std::string_view view) const
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

    void Preprocessor::append_macro_to_map(TextPositionMap& map, const std::string_view macro) const
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
                    map.name.empty() ? "Preprocessed text" : fmt::format("Macro '{}'", map.name), max_macro_length_,
                    original_pos_[offset_of(macro)].to_string()));

            map.content += view;
            for (const auto [i] : clu::indices(view.size()))
                map.positions.emplace_back(macro_map, i);
        }
    }

    void Preprocessor::parse_consume_macro_def(std::string_view& view)
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

        auto& map =
            res_.maps.emplace_back(TextPositionMap{.name = std::string(macro_name), .definition_position = def_pos});
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

    std::string_view Preprocessor::parse_consume_macro_ref(std::string_view& view) const
    {
        const auto idx = view.find('*', 1);
        if (idx == npos)
            throw ParseError("Macro reference is not closed with another '*' " + //
                original_pos_[offset_of(view)].to_string());
        const auto name = view.substr(1, idx - 1);
        view.remove_prefix(idx + 1);
        return name;
    }

    void Preprocessor::validate_macro_name(const std::string_view name) const
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
} // namespace hkr
