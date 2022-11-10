#pragma once

#include <ostream>

#include "hikari/types.h"

namespace hkr
{
    enum class Clef : std::uint8_t
    {
        none,
        bass,
        treble
    };

    class LilypondConverter final
    {
    public:
        explicit LilypondConverter(const Music& music): music_(music) {}

        void write_to_stream(std::ostream& stream);

    private:
        const Music& music_;
        Time time_, partial_;
        std::vector<Clef> clefs_;
        std::ostream* stream_ = nullptr;

        void write_staff(std::size_t staff_idx);
        void write_staff_section(std::size_t staff_idx, std::size_t section_idx);

        void write_measure_attributes(const Measure::Attributes& attrs);
    };
}
