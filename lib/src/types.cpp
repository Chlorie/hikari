#include "hikari/types.h"

#include <stdexcept>

namespace hkr
{
    namespace
    {
        constexpr int values[]{0, 2, 4, 5, 7, 9, 11}; // C, D, E, F, G, A, B

        // For the interval, 0 is unison, 1 is a 2nd, and so on
        Note transpose_up_pure(const Note note, const int semitone, const int interval) noexcept
        {
            const int old_base = static_cast<int>(note.base);
            const int new_base = (old_base + 7 + interval) % 7;
            const int diff_octave = (old_base + interval - new_base) / 7;
            const int diff_accidental = values[old_base] + semitone - values[new_base] - diff_octave * 12;
            return {
                .base = static_cast<NoteBase>(new_base),
                .octave = note.octave + diff_octave,
                .accidental = note.accidental + diff_accidental //
            };
        }

        Note normalize_multi_accidentals(const Note note) noexcept
        {
            if (note.accidental >= 3)
                return transpose_up_pure(note, 0, 1);
            if (note.accidental <= -3)
                return transpose_up_pure(note, 0, -1);
            return note;
        }

        Note transpose_up_impl(const Note note, const int semitone, const int interval) noexcept
        {
            return normalize_multi_accidentals(transpose_up_pure(note, semitone, interval));
        }
    } // namespace

    int Interval::semitones() const
    {
        if (number < 1)
            throw std::out_of_range("Interval number should be greater than 0");
        const int octave_semitones = (number - 1) / 7 * 12;
        const int simple = (number - 1) % 7; // Actually this is simple interval minus 1
        const int qual = static_cast<int>(quality);
        static constexpr int defaults[]{0, 2, 4, 5, 7, 9, 11};
        static constexpr int modifier_2367[]{-2, -1, 0, 0, 1};
        static constexpr int modifier_145[]{-1, 0, 0, 0, 1};
        switch (simple)
        {
            case 0:
            case 3:
            case 4:
                if (quality == IntervalQuality::major || quality == IntervalQuality::minor)
                    throw std::runtime_error(
                        "Intervals based on a unison/fourth/fifth cannot be of major or minor quality");
                return octave_semitones + defaults[simple] + modifier_145[qual];
            default:
                if (quality == IntervalQuality::perfect)
                    throw std::runtime_error(
                        "Intervals based on a second/third/sixth/seventh cannot be of perfect quality");
                return octave_semitones + defaults[simple] + modifier_2367[qual];
        }
    }

    Note Note::transposed_up(int semitones) const noexcept
    {
        if (semitones == 0)
            return *this;
        if (semitones < 0)
            return transposed_down(-semitones);
        Note result = *this;
        result.octave += semitones / 12;
        semitones %= 12;
        static constexpr int intervals[]{0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
        return transpose_up_impl(result, semitones, intervals[semitones]);
    }

    Note Note::transposed_down(int semitones) const noexcept
    {
        if (semitones == 0)
            return *this;
        if (semitones < 0)
            return transposed_up(-semitones);
        Note result = *this;
        result.octave -= semitones / 12;
        semitones %= 12;
        static constexpr int intervals[]{0, -1, -1, -2, -2, -3, -3, -4, -5, -5, -6, -6};
        return transpose_up_impl(result, semitones, intervals[semitones]);
    }

    Note Note::transposed_up(Interval interval) const noexcept
    {
        Note result = *this;
        result.octave += (interval.number - 1) / 7;
        interval.number = (interval.number - 1) % 7 + 1;
        return transpose_up_impl(result, interval.semitones(), interval.number - 1);
    }

    Note Note::transposed_down(Interval interval) const noexcept
    {
        Note result = *this;
        result.octave -= (interval.number - 1) / 7;
        interval.number = (interval.number - 1) % 7 + 1;
        return transpose_up_impl(result, -interval.semitones(), 1 - interval.number);
    }

    std::int8_t Note::pitch_id() const
    {
        const int value = values[static_cast<int>(base)] + (octave + 1) * 12;
        if (value < 0 || value > 127)
            throw std::out_of_range("Note value must be between 0 and 127");
        return static_cast<std::int8_t>(value);
    }

    void Measure::Attributes::merge_with(const Attributes& other)
    {
        if (other.key)
            key = other.key;
        if (other.time)
            time = other.time;
        if (other.partial)
            partial = other.partial;
    }

    std::pair<std::size_t, std::size_t> Section::beat_index_range_of_measure(const std::size_t measure) const
    {
        const auto start = measures[measure].start_beat;
        const auto stop = measures.size() == measure + 1 ? staves[0].size() : measures[measure + 1].start_beat;
        return {start, stop};
    }
} // namespace hkr
