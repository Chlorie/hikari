#pragma once

#include <ostream>

#include "hikari/types.h"

namespace hkr
{
    class LilypondConverter final
    {
    public:
        explicit LilypondConverter(const Music& music): music_(music) {}

        void write_to_stream(std::ostream& stream);

    private:
        const Music& music_;
        std::ostream* stream_ = nullptr;

        void write_staff(std::size_t staff_idx) const;
    };
}
