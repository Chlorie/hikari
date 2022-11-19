#include "music_converter.h"

#include <algorithm>
#include <utility>
#include <ranges>
#include <clu/scope.h>

namespace hkr::ly
{
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
            // TODO: restructure durations
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
        {
            if (measure.chords.empty())
                continue;
            const auto end = measure.chords.end();
            auto iter = measure.chords.begin(), new_end = iter;
            while (++iter != end)
            {
                if (new_end->chord->start == iter->chord->start)
                    new_end->range = merge_range(new_end->range, iter->range);
                else if (++new_end != iter)
                    *new_end = *iter;
            }
            measure.chords.erase(++new_end, end);
        }
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
                        if (k==0 || ch.chord->start.denominator() == 1)
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

    LyMusic convert_to_ly(Music music) { return LyMusicConverter(std::move(music)).convert(); }
} // namespace hkr::ly
