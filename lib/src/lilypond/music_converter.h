#pragma once

#include <span>
#include <clu/rational.h>

#include "types.h"

namespace hkr::ly
{
    class LyMusicConverter
    {
    public:
        explicit LyMusicConverter(Music music): music_(std::move(music)) {}

        LyMusic convert();

    private:
        Music music_;
        LyMusic res_;

        LyStaff unroll_staff(std::size_t idx);
        static void unroll_voices(LyMeasure& measure, std::span<Beat> in_beats, LyMeasure* last_measure);
    };

    struct NoteRange
    {
        Note low;
        Note high;
    };

    class ClefChangePlacer
    {
    public:
        explicit ClefChangePlacer(LyStaff& staff): staff_(staff) {}

        void place();

    private:
        struct ChordInfo
        {
            LyChord* chord;
            NoteRange range;
        };

        struct MeasureNotesInfo
        {
            LyMeasure* measure = nullptr;
            std::vector<ChordInfo> chords;
        };

        LyStaff& staff_;
        std::vector<MeasureNotesInfo> measures_;
        Clef current_clef_ = Clef::none;

        void extract_and_sort_chords();
        void merge_simultaneous_chords();
        void find_clef_changes();
        void adjust_clef_changes();
    };
}
