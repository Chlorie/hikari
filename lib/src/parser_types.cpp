#include "parser_types.h"

#include <fmt/format.h>

namespace hkr
{
    TextPosition::TextPosition(TextPositionMap& map_entry, const std::size_t offset):
        data_(reinterpret_cast<std::uintptr_t>(&map_entry)), offset_(offset)
    {
    }

    TextPosition::TextPosition(const std::size_t line, const std::size_t column)
    {
        set_line(line);
        set_column(column);
    }

    void TextPosition::set_line(const std::size_t value)
    {
        if (constexpr auto max = ~std::uintptr_t{} >> 1; value > max)
            throw ParseError(fmt::format("Line number must be less than {}", max));
        data_ = (value << 1) | 1;
    }

    std::string TextPosition::to_string() const
    {
        if (is_eof())
            return "at the end of input";
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
} // namespace hkr
