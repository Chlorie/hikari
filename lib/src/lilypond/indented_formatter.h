#pragma once

#include <ostream>
#include <fmt/format.h>
#include <clu/macros.h>

namespace hkr::ly
{
    class IndentedFormatter
    {
    public:
        class [[nodiscard]] IndentedScope
        {
        public:
            CLU_IMMOVABLE_TYPE(IndentedScope);
            ~IndentedScope() noexcept;

        private:
            friend IndentedFormatter;
            IndentedFormatter* parent_ = nullptr;

            explicit IndentedScope(IndentedFormatter& parent);
        };

        explicit IndentedFormatter(std::ostream& stream, std::size_t indent_size = 4);

        template <typename... Ts>
        void print(fmt::format_string<Ts...> format, Ts&&... args)
        {
            indent();
            this->vprint(format, fmt::make_format_args(static_cast<Ts&&>(args)...));
        }

        void println()
        {
            *os_ << '\n';
            should_indent_ = true;
        }

        template <typename... Ts>
        void println(fmt::format_string<Ts...> format, Ts&&... args)
        {
            indent();
            this->vprint(format, fmt::make_format_args(static_cast<Ts&&>(args)...));
            println();
        }

        template <typename... Ts>
        IndentedScope new_scope(fmt::format_string<Ts...> format, Ts&&... args)
        {
            this->print(format, static_cast<Ts&&>(args)...);
            return IndentedScope(*this);
        }

    private:
        std::ostream* os_ = nullptr;
        std::size_t indent_ = 0;
        std::size_t current_ = 0;
        bool should_indent_ = false;

        void indent();
        void vprint(const fmt::string_view format, const fmt::format_args args) const { *os_ << vformat(format, args); }
    };
}
