#pragma once

#include <ostream>
#include <optional>
#include <clu/rational.h>

#include "hikari/types.h"

namespace hkr::ly
{
    enum class Clef : std::uint8_t
    {
        none = 0,
        bass_8vb,
        bass,
        treble,
        treble_8va
    };

    enum class TupletGroupPosition : std::uint8_t
    {
        none = 0,
        head = 1,
        last = 2
    };

    struct TupletAttributes
    {
        clu::rational<int> ratio = 1;
        TupletGroupPosition pos = TupletGroupPosition::none;
    };

    // In a `LyChord`, when a chord is sustained, it means that it extends to the next chord,
    // contrary to in the original `Chord` where sustained means that it sustains the previous chord
    struct LyChord
    {
        clu::rational<int> start;
        TupletAttributes tuplet;
        std::optional<Chord> chord; // nullopt for when the voice is skipped here (spacer)
        Clef clef_change{};
    };

    using LyVoice = std::vector<LyChord>;

    struct LyMeasure
    {
        Time actual_time;
        Measure::Attributes attributes;
        std::vector<LyVoice> voices;
    };

    using LyStaff = std::vector<LyMeasure>;
    using LyMusic = std::vector<LyStaff>;

    LyMusic convert_to_ly(Music music);
    void write_to_stream(std::ostream& stream, const LyMusic& music);
}
