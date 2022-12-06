#pragma once

#include <span>

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

    struct RationalRange
    {
        clu::rational<int> begin;
        clu::rational<int> end;
    };

    class DurationPartitioner
    {
    public:
        explicit DurationPartitioner(LyMeasure& measure): measure_(measure) {}

        void partition();

    private:
        class TupletPartitioner;

        LyMeasure& measure_;

        // 2^n * (1|3|7)/2^k, use a single note for the whole measure
        bool check_use_one_note(const LyVoice& voice) const;
        // 2^n * 1/2^k, like 4/4 or 2/4
        void partite_regular(LyVoice& voice, const RationalRange& range);
        // 2^n * 3/2^k, like 6/8 or 12/8
        void partite_regular_over_3(LyVoice& voice, const RationalRange& range, int regular);
        // 3/2^k, like 3/4 or 3/8
        void partite_3beats(LyVoice& voice, const RationalRange& range);

        void break_at(LyVoice& voice, clu::rational<int> pos) const;
        void break_tuplets(LyVoice& voice) const;
        bool is_syncopated_4beat(const LyVoice& voice, const RationalRange& range) const;
    };
}
