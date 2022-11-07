#pragma once

#include <vector>
#include <cstdint>

#include "export.h"

namespace hkr
{
    // clang-format off
    enum class NoteBase : std::uint8_t { c, d, e, f, g, a, b };
    // clang-format on

    struct HIKARI_API Note
    {
        NoteBase base;
        int octave;
        int accidental;
    };

    using Chord = std::vector<Note>;
    using Subdivision = std::vector<Chord>;
    using Voice = std::vector<Subdivision>;
    using Beat = std::vector<Voice>;
    using Measure = std::vector<Beat>;
    using Staff = std::vector<Measure>;
    using Segment = std::vector<Staff>;
    using Music = std::vector<Segment>;
} // namespace hkr
