#pragma once

#include <vector>
#include <cstdint>
#include <optional>

#include "export.h"

namespace hkr
{
    // clang-format off
    enum class NoteBase : std::uint8_t { c, d, e, f, g, a, b };
    // clang-format on

    struct HIKARI_API Time
    {
        int numerator = 4;
        int denominator = 4;
    };

    struct HIKARI_API Note
    {
        NoteBase base;
        int octave;
        int accidental;

        Note transposed_up(int semitones) noexcept;
        Note transposed_down(int semitones) noexcept;
    };

    struct HIKARI_API Chord
    {
        struct HIKARI_API Attributes
        {
            std::optional<float> tempo;
        };

        std::vector<Note> notes;
        Attributes attributes;
    };

    using Voice = std::vector<Chord>;
    using Beat = std::vector<Voice>;

    struct HIKARI_API Measure
    {
        struct HIKARI_API Attributes
        {
            std::optional<int> key;
            std::optional<Time> time;
            std::optional<Time> partial;
        };

        std::vector<Beat> beats;
        Attributes attributes;
    };

    using Staff = std::vector<Measure>;
    using Section = std::vector<Staff>;
    using Music = std::vector<Section>;
} // namespace hkr
