#include "music_converter.h"

#include <algorithm>
#include <functional>
#include <numeric>
#include <utility>
#include <ranges>
#include <clu/scope.h>
#include <clu/concepts.h>

namespace hkr::ly
{
    namespace
    {
        template <typename T, typename E, typename M>
        void merge_elements(std::vector<T>& vec, E&& equal, M&& merger)
        {
            if (vec.empty())
                return;
            const auto end = vec.end();
            auto iter = vec.begin(), new_end = iter;
            while (++iter != end)
            {
                if (equal(*new_end, *iter))
                    merger(*new_end, *iter);
                else if (++new_end != iter)
                    *new_end = std::move(*iter);
            }
            vec.erase(++new_end, end);
        }
    } // namespace

    LyMusic LyMusicConverter::convert()
    {
        const auto n_staves = std::ranges::max(music_, std::less{}, //
            [](const Section& sec) {
                return sec.staves.size();
            }).staves.size();
        res_.reserve(n_staves);
        for (std::size_t i = 0; i < n_staves; i++)
        {
            LyStaff& staff = res_.emplace_back(unroll_staff(i));
            ClefChangePlacer(staff).place();
            for (auto& measure : staff)
                DurationPartitioner(measure).partition();
        }
        return std::move(res_);
    }

    LyStaff LyMusicConverter::unroll_staff(const std::size_t idx)
    {
        LyStaff res;
        Time time;
        for (auto& sec : music_)
            for (std::size_t j = 0; j < sec.measures.size(); j++)
            {
                const auto& attrs = sec.measures[j].attributes;
                if (attrs.time)
                    time = *attrs.time;
                const Time partial = attrs.partial ? *attrs.partial : time;

                auto& measure = res.emplace_back();
                measure.attributes = attrs;
                measure.current_time = time;
                measure.current_partial = partial;
                if (sec.staves.size() <= idx) // Empty section, so empty measure
                    continue;

                const auto [begin, end] = sec.beat_index_range_of_measure(j);
                Staff& in_staff = sec.staves[idx];
                const std::span in_beats{in_staff.data() + begin, end - begin};
                LyMeasure* last_measure = res.size() == 1 ? nullptr : &measure - 1;
                unroll_voices(measure, in_beats, last_measure);
            }
        return res;
    }

    void LyMusicConverter::unroll_voices(LyMeasure& measure, const std::span<Beat> in_beats, LyMeasure* last_measure)
    {
        const auto n_voices = std::ranges::max(in_beats, std::less{}, &Beat::size).size();
        measure.voices.resize(n_voices);
        for (int i = 0; auto& in_beat : in_beats)
        {
            for (std::size_t j = 0; j < in_beat.size(); j++)
            {
                auto& in_voice = in_beat[j];
                auto& voice = measure.voices[j];
                for (int k = 0; auto& in_chord : in_voice)
                {
                    const auto start = i + clu::rational(k, static_cast<int>(in_voice.size()));
                    if (in_chord.sustained)
                    {
                        if (!voice.empty())
                        {
                            if (voice.back().chord) // Sustain the last chord
                            {
                                k++;
                                continue;
                            }
                        }
                        else if (last_measure)
                        {
                            if (auto& voices_prev = last_measure->voices; //
                                voices_prev.size() > j)
                            {
                                // Sustain the last chord in the previous measure,
                                // if that chord is not a rest or a spacer
                                if (auto& chord_prev = voices_prev[j].back().chord; //
                                    chord_prev && !chord_prev->notes.empty())
                                {
                                    in_chord.notes = chord_prev->notes;
                                    chord_prev->sustained = true;
                                }
                            }
                        }
                        // else: insert as a rest
                        in_chord.sustained = false;
                    }
                    voice.push_back(LyChord{.start = start, .chord = std::move(in_chord)});
                    k++;
                }
            }
            for (std::size_t j = in_beat.size(); j < n_voices; j++)
                measure.voices[j].push_back(LyChord{.start = i});
            i++;
        }
    }

    // Clef change related utilities
    namespace
    {
        // Accidentals are ignored

        int to_int(const Note note) noexcept { return note.octave * 7 + static_cast<int>(note.base); }
        Note to_note(const int value) noexcept { return Note{static_cast<NoteBase>(value % 7), value / 7}; }
        Note average_note(const Note lhs, const Note rhs) noexcept { return to_note((to_int(lhs) + to_int(rhs)) / 2); }

        constexpr NoteRange in_staff_range(const Clef clef) noexcept
        {
            using enum NoteBase;
            switch (clef)
            {
                case Clef::bass_8va_bassa: return {{g, 1}, {a, 2}};
                case Clef::bass: return {{g, 2}, {a, 3}};
                case Clef::treble: return {{e, 4}, {f, 5}};
                case Clef::treble_8va: return {{e, 5}, {f, 6}};
                default: return {{d, 0}, {c, 0}}; // "none" clef is always unacceptable
            }
        }

        int ledger_line_in_staff(const Note note, const Clef clef) noexcept
        {
            const auto range = in_staff_range(clef);
            const int notei = to_int(note);
            if (const int low = to_int(range.low); notei < low)
                return (low - notei) / 2;
            if (const int high = to_int(range.high); notei > high)
                return (notei - high) / 2;
            return 0;
        }

        bool cmp_note_staff_position(const Note lhs, const Note rhs) noexcept
        {
            if (lhs.octave < rhs.octave)
                return true;
            if (lhs.octave > rhs.octave)
                return false;
            return static_cast<int>(lhs.base) < static_cast<int>(rhs.base);
        }

        NoteRange merge_range(const NoteRange lhs, const NoteRange rhs) noexcept
        {
            return {cmp_note_staff_position(lhs.low, rhs.low) ? lhs.low : rhs.low, //
                cmp_note_staff_position(lhs.high, rhs.high) ? rhs.high : lhs.high};
        }

        bool note_in_staff_range(const Note note, const NoteRange range) noexcept
        {
            return !cmp_note_staff_position(note, range.low) && !cmp_note_staff_position(range.high, note);
        }

        NoteRange clef_acceptable_range(const Clef clef) noexcept
        {
            // The acceptable range of a clef is defined as the range of pitches that need
            // at most 3 ledger lines in that clef.
            // Octtava alta clefs should not contain notes that extends away from the octave
            // alteration direction.
            // The MIDI note range 0~127 corresponds to C-1 to G9
            using enum NoteBase;
            switch (clef)
            {
                case Clef::bass_8va_bassa: return {{c, -1}, {b, 2}};
                case Clef::bass: return {{g, 1}, {a, 4}};
                case Clef::treble: return {{e, 3}, {f, 6}};
                case Clef::treble_8va: return {{d, 5}, {g, 9}};
                default: return {{d, 0}, {c, 0}}; // "none" clef is always unacceptable
            }
        }

        bool clef_is_acceptable(const Note note, const Clef clef) noexcept
        {
            return note_in_staff_range(note, clef_acceptable_range(clef));
        }

        bool clef_is_acceptable(const NoteRange range, const Clef clef) noexcept
        {
            return (clef_is_acceptable(range.low, clef) && clef_is_acceptable(range.high, clef)) ||
                clef_is_acceptable(average_note(range.low, range.high), clef);
        }

        Clef preferred_clef(const Note note) noexcept
        {
            using enum NoteBase;
            if (cmp_note_staff_position({b, 5}, note)) // C6~
                return Clef::treble_8va;
            if (cmp_note_staff_position({b, 3}, note)) // C4~
                return Clef::treble;
            if (cmp_note_staff_position({b, 1}, note)) // C2~
                return Clef::bass;
            // else: ~B1
            return Clef::bass_8va_bassa;
        }

        Clef preferred_clef(const NoteRange range) noexcept
        {
            if (range.low.base == range.high.base && range.low.octave == range.high.octave)
                return preferred_clef(range.low); // Shortcut
            const auto low_pref = preferred_clef(range.low);
            const auto high_pref = preferred_clef(range.high);
            if (low_pref == high_pref)
                return low_pref;
            const bool low_acceptable = clef_is_acceptable(range.high, low_pref);
            const bool high_acceptable = clef_is_acceptable(range.low, high_pref);
            if (low_acceptable != high_acceptable) // One of them is acceptable
                return low_acceptable ? low_pref : high_pref;
            if (!low_acceptable && !high_acceptable) // None of them is acceptable
                return preferred_clef(average_note(range.low, range.high)); // Just use the average
            // Both of them are acceptable, find the one with less ledger lines
            const int ledger_low =
                ledger_line_in_staff(range.low, low_pref) + ledger_line_in_staff(range.high, low_pref);
            const int ledger_high =
                ledger_line_in_staff(range.low, high_pref) + ledger_line_in_staff(range.high, high_pref);
            return ledger_low < ledger_high ? low_pref : high_pref;
        }
    } // namespace

    void ClefChangePlacer::place()
    {
        extract_and_sort_chords();
        merge_simultaneous_chords();
        find_clef_changes();
        adjust_clef_changes();
    }

    void ClefChangePlacer::extract_and_sort_chords()
    {
        for (auto& in_measure : staff_)
        {
            auto& measure = measures_.emplace_back();
            measure.measure = &in_measure;
            for (auto& in_voice : in_measure.voices)
            {
                for (auto& in_chord : in_voice)
                {
                    if (!in_chord.chord || in_chord.chord->notes.empty())
                        continue;
                    const auto& notes = in_chord.chord->notes;
                    const auto [min, max] = std::ranges::minmax(notes, cmp_note_staff_position);
                    measure.chords.push_back({&in_chord, {min, max}});
                }
            }
            std::ranges::sort(measure.chords,
                [](const ChordInfo& lhs, const ChordInfo& rhs) noexcept
                { return lhs.chord->start < rhs.chord->start; });
        }
    }

    void ClefChangePlacer::merge_simultaneous_chords()
    {
        for (auto& measure : measures_)
            merge_elements(
                measure.chords,
                [](const ChordInfo& lhs, const ChordInfo& rhs) noexcept
                { return lhs.chord->start == rhs.chord->start; },
                [](ChordInfo& lhs, const ChordInfo& rhs) noexcept { lhs.range = merge_range(lhs.range, rhs.range); });
    }

    void ClefChangePlacer::find_clef_changes()
    {
        Clef current = Clef::none;
        for (std::size_t i = 0; auto& measure : measures_)
        {
            for (std::size_t j = 0; auto& chord : measure.chords)
            {
                clu::scope_exit inc{[&] { j++; }};
                // We only grant a clef change when the former clef is unacceptable for some notes
                if (clef_is_acceptable(chord.range, current))
                    continue;
                current = preferred_clef(chord.range);

                // Preference (highest to lowest):
                // modifying previous clef change
                // change at start of this measure
                // change on some beat in this measure
                // change on the very note
                [&]
                {
                    ChordInfo* info = &chord;
                    for (std::size_t k = j + 1; k-- > 0;)
                    {
                        auto& ch = measure.chords[k];
                        if (!clef_is_acceptable(ch.range, current))
                            return info;
                        if (ch.chord->clef_change != Clef::none)
                            return &ch;
                        // On a whole beat, k == 0 means that this chord is the first to appear
                        // in the current staff (maybe preceded with rests)
                        // TODO: maybe prefer beat->half beat->quarter beat->... ?
                        if (k == 0 || ch.chord->start.denominator() == 1)
                            info = &ch;
                    }
                    // Find the last clef change
                    for (std::size_t k = i; k-- > 0;)
                        for (auto& ch : std::views::reverse(measures_[k].chords))
                        {
                            if (!clef_is_acceptable(ch.range, current))
                                return info;
                            if (ch.chord->clef_change != Clef::none)
                                return &ch;
                        }
                    return info;
                }()
                    ->chord->clef_change = current;
            }
            i++;
        }
    }

    void ClefChangePlacer::adjust_clef_changes()
    {
        for (auto& measure : measures_)
        {
            if (measure.chords.empty())
                continue;
            // Move the clef change to the start of a measure if the note is only preceded by rests
            if (LyChord& chord = *measure.chords[0].chord; chord.start != 0)
            {
                const Clef clef = std::exchange(chord.clef_change, Clef::none);
                for (LyVoice& voice : measure.measure->voices)
                    if (!voice.empty())
                    {
                        voice[0].clef_change = clef;
                        break;
                    }
            }
        }
    }

    // Duration partition related utilities
    namespace
    {
        bool has_single_bit(const int value) noexcept { return std::has_single_bit(static_cast<unsigned>(value)); }

        clu::rational<int> to_rational(const Time time) noexcept { return {time.numerator, time.denominator}; }

        bool both_rest_or_spacer(const LyChord& lhs, const LyChord& rhs) noexcept
        {
            if (lhs.chord.has_value() != rhs.chord.has_value())
                return false;
            if (!lhs.chord && !rhs.chord)
                return true;
            return lhs.chord->notes.empty() && rhs.chord->notes.empty();
        }

        int without_trailing_zero(const int value) noexcept
        {
            const auto v = static_cast<unsigned>(value);
            return static_cast<int>(v >> std::countr_zero(v));
        }

        std::span<LyChord> from_range(LyVoice& voice, const RationalRange& range) noexcept
        {
            const auto begin = std::ranges::find_if(voice, //
                [pos = range.begin](const LyChord& chord) { return chord.start >= pos; });
            const auto end = std::ranges::find_if(voice, //
                [pos = range.end](const LyChord& chord) { return chord.start >= pos; });
            return {begin, end};
        }

        std::span<const LyChord> from_range(const LyVoice& voice, const RationalRange& range) noexcept
        {
            const auto begin = std::ranges::find_if(voice, //
                [pos = range.begin](const LyChord& chord) { return chord.start >= pos; });
            const auto end = std::ranges::find_if(voice, //
                [pos = range.end](const LyChord& chord) { return chord.start >= pos; });
            return {begin, end};
        }

        clu::rational<int> gcd(const clu::rational<int> lhs, const clu::rational<int> rhs) noexcept
        {
            const int d = std::lcm(lhs.denominator(), rhs.denominator());
            const int ln = lhs.numerator() * (d / lhs.denominator());
            const int rn = rhs.numerator() * (d / rhs.denominator());
            return {std::gcd(ln, rn), d};
        }

        bool is_regular_chord(const LyChord& chord) noexcept { return has_single_bit(chord.start.denominator()); }
    } // namespace

    void DurationPartitioner::partition()
    {
        for (LyVoice& voice : measure_.voices)
        {
            // Merge rests and spacers
            merge_elements(voice, both_rest_or_spacer, [](const LyChord&, const LyChord&) noexcept {});
            // if (check_use_one_note(voice))
            //     continue;
            break_tuplets(voice);

            const int n_beats = measure_.current_time.numerator;
            const auto ratio = to_rational(measure_.current_time);
            const auto partial_ratio = to_rational(measure_.current_partial);
            const auto initial = (partial_ratio - ratio) * measure_.current_time.denominator;
            const auto last = partial_ratio * measure_.current_time.denominator;

            if (const int irregular = without_trailing_zero(n_beats); irregular == 1) // regular
                partite_regular(voice, {initial, last});
            else if (irregular == 3) // regular over 3
                partite_regular_over_3(voice, {initial, last}, n_beats / irregular);
            else if (n_beats % 3 == 0) // irregular over 3
            {
                for (int i = 0; i < n_beats; i += 3)
                    partite_3beats(voice, {initial + i, initial + (i + 3)});
            }
            else if (n_beats % 3 == 1)
            {
                partite_regular(voice, {initial, initial + 4});
                for (int i = 4; i < n_beats; i += 3)
                    partite_3beats(voice, {initial + i, initial + (i + 3)});
            }
            else // n_beats % 3 == 2
            {
                for (int i = 0; i < n_beats - 2; i += 3)
                    partite_3beats(voice, {initial + i, initial + (i + 3)});
                partite_regular(voice, {last - 2, last});
            }
        }
    }

    // (1, 3, 7) * 2^n beats in the measure, just use one note
    bool DurationPartitioner::check_use_one_note(const LyVoice& voice) const
    {
        if (voice.empty() || voice.size() == 1)
            return true; // Filler or one note
        if (std::ranges::all_of(
                voice, [](const LyChord& chord) noexcept { return !chord.chord || chord.chord->notes.empty(); }))
            return true; // All rests
        if (to_rational(measure_.current_partial) != to_rational(measure_.current_time))
            return false;
        const auto beats_no2 = without_trailing_zero(measure_.current_time.numerator);
        return beats_no2 == 1 || beats_no2 == 3 || beats_no2 == 7;
    }

    void DurationPartitioner::partite_regular(LyVoice& voice, const RationalRange& range)
    {
        if (range.end <= 0)
            return;
        break_at(voice, range.end);
        if (is_syncopated_4beat(voice, range))
            return;
    }

    void DurationPartitioner::partite_regular_over_3(LyVoice& voice, const RationalRange& range, const int regular)
    {
        if (range.end <= 0)
            return;
        break_at(voice, range.end);
    }

    void DurationPartitioner::partite_3beats(LyVoice& voice, const RationalRange& range)
    {
        if (range.end <= 0)
            return;
        break_at(voice, range.end);
    }

    void DurationPartitioner::break_at(LyVoice& voice, const clu::rational<int> pos) const
    {
        if (pos == to_rational(measure_.current_partial) * measure_.current_time.denominator)
            return;
        const auto iter = std::ranges::find_if(voice, [pos](const LyChord& chord) { return chord.start >= pos; });
        if (iter != voice.end() && iter->start == pos)
            return;
        const auto inserted = voice.insert(iter, *std::prev(iter));
        inserted->start = pos;
        if (auto& chord = inserted->chord)
            chord->attributes = {};
        if (auto& prev_tuplet = std::prev(inserted)->tuplet; //
            prev_tuplet.pos == TupletGroupPosition::last)
            inserted->tuplet.pos = std::exchange(prev_tuplet.pos, TupletGroupPosition::head);
        if (auto& chord = std::prev(inserted)->chord)
            chord->sustained = true;
    }

    class DurationPartitioner::TupletPartitioner
    {
    public:
        explicit TupletPartitioner(const DurationPartitioner& parent, LyVoice& voice) noexcept:
            parent_(parent), voice_(voice)
        {
        }

        void partition() const
        {
            // To partition the tuplets out:
            // 1. We find a segment in the voice s.t. the segment starts and ends with chords at regular
            //    positions (k / 2^n) and all the other chords in which are irregular.
            // 2. Maintain a list of potential "break points":
            //    1) Find the gcd of position differences of adjacent chords in the segment, and
            //       regularize the gcd (k / (2^n*p) -> k / 2^n).
            //    2) Cut the segment into intervals of the same length, with the length being the
            //       regularized gcd.
            //    3) For each pair of regularly positioned markers (break points and the endings of the
            //       segment)
            //       a) ignore all the break points between them and recalculate the gcd, and
            //       b) count the break points that don't lie on a multiple of such gcd.
            //    4) Find the pair with the largest count:
            //       a) If such count is zero, goto 3.
            //       b) Otherwise, remove all the counted break points from the list, and goto 3).
            // 3. Break the voice with the final break point list.
            break_tuplets();
            set_tuplet_ratios();
        }

    private:
        enum class Type
        {
            chord,
            break_point,
            placeholder
        };

        struct Position
        {
            clu::rational<int> start;
            Type type;
        };

        const DurationPartitioner& parent_;
        LyVoice& voice_;

        using ChordIter = LyVoice::iterator;

        void break_tuplets() const
        {
            auto iter = voice_.begin();
            while (true)
            {
                iter = std::ranges::find_if(iter, voice_.end(), std::not_fn(is_regular_chord));
                if (iter == voice_.end())
                    break;
                const auto end = std::ranges::find_if(iter, voice_.end(), is_regular_chord);
                auto pos = construct_positions(std::prev(iter), end);
                fill_break_points(pos);
                while (remove_unnecessary_breaks_once(pos)) {}
                const auto idx = std::distance(voice_.begin(), end); // end will be invalidated after the break
                break_with_positions(pos);
                iter = voice_.begin() + idx;
            }
        }

        void set_tuplet_ratios() const
        {
            auto iter = voice_.begin();
            while (true)
            {
                iter = std::ranges::find_if(iter, voice_.end(), std::not_fn(is_regular_chord));
                if (iter == voice_.end())
                    break;
                const auto end = std::ranges::find_if(iter, voice_.end(), is_regular_chord);
                set_tuplet_ratios_in_range(std::prev(iter), end);
                const auto idx = std::distance(voice_.begin(), end); // end will be invalidated after the break
                break_compound_durations(std::prev(iter), end);
                iter = voice_.begin() + idx;
            }
        }

        std::vector<Position> construct_positions(const ChordIter begin, const ChordIter end) const
        {
            const std::span subrange(begin, end);
            std::vector<Position> pos;
            pos.reserve(subrange.size() + 1);
            for (const auto& chord : subrange)
                pos.push_back({.start = chord.start, .type = Type::chord});
            pos.push_back({
                .start = end == voice_.end() ? parent_.measure_.current_partial.numerator : end->start,
                .type = Type::chord //
            });
            return pos;
        }

        void fill_break_points(std::vector<Position>& pos) const
        {
            auto period = find_subrange_gcd(pos);
            const auto den = static_cast<unsigned>(period.denominator());
            const auto multi = den >> std::countr_zero(den);
            period *= static_cast<int>(multi);

            const auto begin = pos.front().start, end = pos.back().start;
            for (auto i = begin + period; i < end; i += period)
                pos.push_back({.start = i, .type = Type::break_point});

            std::ranges::sort(pos, std::less{}, &Position::start);
        }

        bool remove_unnecessary_breaks_once(std::vector<Position>& pos) const
        {
            const auto is_regular_non_placeholder = [](const Position& p) noexcept
            { return p.type != Type::placeholder && has_single_bit(p.start.denominator()); };

            std::span<Position> best_subrange;
            std::size_t max_breaks_removed = 0;

            auto begin = pos.begin();
            while (true)
            {
                begin = std::ranges::find_if(begin, pos.end(), is_regular_non_placeholder);
                if (begin == pos.end())
                    break;
                auto end = std::next(begin);
                while (true)
                {
                    end = std::ranges::find_if(end, pos.end(), is_regular_non_placeholder);
                    if (end == pos.end())
                        break;
                    ++end;
                    const std::span subrange{begin, end};
                    if (const std::size_t count = count_unnecessary_breaks_in_range(subrange);
                        count > max_breaks_removed)
                    {
                        max_breaks_removed = count;
                        best_subrange = subrange;
                    }
                }
                ++begin;
            }

            if (max_breaks_removed == 0)
                return false;
            remove_unnecessary_breaks_in_range(best_subrange);
            return true;
        }

        void foreach_unnecessary_breaks_in_range( //
            const std::span<Position> subrange, clu::callable<Position&> auto&& func) const
        {
            if (subrange.size() <= 1)
                return;
            const auto period = find_subrange_gcd(subrange);
            for (auto& pos : subrange.subspan(1, subrange.size() - 2))
            {
                if (pos.type != Type::break_point)
                    continue;
                if (((pos.start - subrange[0].start) / period).denominator() != 1)
                    func(pos);
                // pos.type = Type::placeholder;
            }
        }

        void remove_unnecessary_breaks_in_range(const std::span<Position> subrange) const
        {
            foreach_unnecessary_breaks_in_range( //
                subrange, [](Position& pos) { pos.type = Type::placeholder; });
        }

        std::size_t count_unnecessary_breaks_in_range(const std::span<Position> subrange) const
        {
            std::size_t result = 0;
            foreach_unnecessary_breaks_in_range(subrange, [&](auto&) { result++; });
            return result;
        }

        void break_with_positions(const std::vector<Position>& pos) const
        {
            for (const auto& p : pos)
                if (p.type == Type::break_point)
                    parent_.break_at(voice_, p.start);
        }

        void set_tuplet_ratios_in_range(const ChordIter begin, const ChordIter end) const
        {
            const auto rational_bit_ceil = [](const clu::rational<int> value) noexcept
            {
                const int num = value.numerator(), den = value.denominator();
                const int ceil = num / den + (num % den != 0);
                const int ceil2 = static_cast<int>(std::bit_ceil(static_cast<unsigned>(ceil)));
                return ceil2;
            };

            const auto period = find_subrange_gcd(construct_positions(begin, end));
            auto ratio = 1 / period;
            if (ratio.denominator() > ratio.numerator())
                ratio *= rational_bit_ceil(period);
            ratio /= rational_bit_ceil(ratio) / 2;

            for (auto iter = begin; iter != end; ++iter)
                iter->tuplet = {.ratio = ratio, .pos = TupletGroupPosition::head};
            std::prev(end)->tuplet.pos = TupletGroupPosition::last;
        }

        void break_compound_durations(const ChordIter begin, const ChordIter end) const
        {
            std::vector<clu::rational<int>> breaks;
            const Time partial = parent_.measure_.current_partial;
            const auto factor = partial.denominator / begin->tuplet.ratio;
            for (auto iter = begin; iter != end; ++iter)
            {
                auto pos = iter->start;
                const auto end_pos = std::next(iter) == voice_.end() ? partial.numerator : std::next(iter)->start;
                auto diff = (end_pos - pos) / factor;
                while (diff > 4 && diff != 6)
                {
                    diff -= 4;
                    pos += 4 * factor;
                    breaks.push_back(pos);
                }
                while (diff.numerator() > 4 && diff.numerator() != 6) // 0, 1, 2, 3, 4, 6 -> end loop
                {
                    const int floor2 = static_cast<int>(std::bit_floor(static_cast<unsigned>(diff.numerator())));
                    const clu::rational dur(floor2, diff.denominator());
                    diff -= dur;
                    pos += dur * factor;
                    breaks.push_back(pos);
                }
            }
            for (const auto p : breaks)
                parent_.break_at(voice_, p);
        }

        clu::rational<int> find_subrange_gcd(const std::span<const Position> subrange) const
        {
            const auto endpoint_or_chord = [=](const Position& pos)
            {
                return pos.type == Type::chord || // chord
                    &pos == subrange.data() || &pos == &(subrange.back()); // endpoint
            };
            auto starts = subrange | std::views::filter(endpoint_or_chord) | std::views::transform(&Position::start);
            auto iter = starts.begin();
            clu::rational res = 0;
            auto prev = *iter++;
            while (iter != starts.end())
            {
                const auto cur = *iter++;
                const auto diff = cur - prev;
                res = res == 0 ? diff : gcd(diff, res);
                prev = cur;
            }
            return res;
        }
    };

    void DurationPartitioner::break_tuplets(LyVoice& voice) const { TupletPartitioner(*this, voice).partition(); }

    // | 4/4: 8th 4th 4th 4th 8th |
    bool DurationPartitioner::is_syncopated_4beat(const LyVoice& voice, const RationalRange& range) const
    {
        const auto span = from_range(voice, range);
        if (span.size() != 5)
            return false;
        const auto half_beat = (range.end - range.begin) / 8;
        const auto not_rest = [](const LyChord& chord) noexcept { return chord.chord && !chord.chord->notes.empty(); };

        if (span[0].start != range.begin)
            return false;
        if (span[1].start != range.begin + half_beat || !not_rest(span[1]))
            return false;
        if (span[2].start != range.begin + 3 * half_beat || !not_rest(span[2]))
            return false;
        if (span[3].start != range.begin + 5 * half_beat || !not_rest(span[3]))
            return false;
        return span[4].start == range.begin + 7 * half_beat;
    }

    LyMusic convert_to_ly(Music music) { return LyMusicConverter(std::move(music)).convert(); }
} // namespace hkr::ly
