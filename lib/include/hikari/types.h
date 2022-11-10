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

        Note transposed_up(int semitones) const noexcept;
        Note transposed_down(int semitones) const noexcept;

        std::int8_t pitch_id() const;
    };

    struct HIKARI_API Chord
    {
        struct HIKARI_API Attributes
        {
            std::optional<float> tempo;
        };

        std::vector<Note> notes;
        bool sustained = false;
        Attributes attributes;
    };

    using Voice = std::vector<Chord>;
    using Beat = std::vector<Voice>;
    using Staff = std::vector<Beat>;

    struct HIKARI_API Measure
    {
        struct HIKARI_API Attributes
        {
            std::optional<int> key;
            std::optional<Time> time;
            std::optional<Time> partial;

            void merge_with(const Attributes& other);
            bool is_null() const noexcept { return !key && !time && !partial; }
        };

        std::size_t start_beat = 0;
        Attributes attributes;
    };

    struct HIKARI_API Section
    {
        std::vector<Staff> staves;
        std::vector<Measure> measures;

        std::pair<std::size_t, std::size_t> beat_index_range_of_measure(std::size_t measure) const;
    };

    using Music = std::vector<Section>;
} // namespace hkr
