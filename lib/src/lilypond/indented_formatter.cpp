#include "indented_formatter.h"

#include <fmt/ostream.h>

namespace hkr::ly
{
    IndentedFormatter::IndentedScope::~IndentedScope() noexcept
    {
        parent_->current_ -= parent_->indent_;
        parent_->println("}}");
    }

    IndentedFormatter::IndentedScope::IndentedScope(IndentedFormatter& parent): parent_(&parent)
    {
        parent_->current_ += parent_->indent_;
        parent_->println("{{");
    }

    IndentedFormatter::IndentedFormatter(std::ostream& stream, const std::size_t indent_size):
        os_(&stream), indent_(indent_size)
    {
    }

    void IndentedFormatter::indent()
    {
        if (!should_indent_)
            return;
        should_indent_ = false;
        print("{: >{}}", "", current_);
    }
} // namespace hkr::ly
