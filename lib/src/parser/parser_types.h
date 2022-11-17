#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace hkr
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
        explicit TextPosition(TextPositionMap& map_entry, std::size_t offset);
        explicit TextPosition(std::size_t line, std::size_t column);

        bool is_eof() const noexcept { return data_ == 0; }
        bool is_map_entry() const noexcept { return (data_ & 1) == 0 && data_ != 0; }

        // NOLINTNEXTLINE(performance-no-int-to-ptr)
        TextPositionMap& map_entry() const noexcept { return *reinterpret_cast<TextPositionMap*>(data_); }
        std::size_t offset() const noexcept { return offset_; }

        std::size_t line() const noexcept { return data_ >> 1; }
        std::size_t column() const noexcept { return offset_; }

        void set_line(std::size_t value);
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
} // namespace hkr
