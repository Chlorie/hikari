#include "types.h"

#include <span>
#include <algorithm>

namespace hkr::ly
{
    class LyMusicConverter
    {
    public:
        explicit LyMusicConverter(Music music): music_(std::move(music)) {}

        LyMusic convert()
        {
            const auto n_staves = std::ranges::max(music_, std::less{}, //
                [](const Section& sec) {
                    return sec.staves.size();
                }).staves.size();
            res_.reserve(n_staves);
            for (std::size_t i = 0; i < n_staves; i++)
            {
                LyStaff& staff = res_.emplace_back(unroll_staff(i));
                place_clef_changes(staff);
                restructure_durations(staff);
            }
            return std::move(res_);
        }

    private:
        Music music_;
        LyMusic res_;

        LyStaff unroll_staff(const std::size_t idx)
        {
            LyStaff res;
            Time time;
            for (auto& sec : music_)
            {
                for (std::size_t i = 0; i < sec.measures.size(); i++)
                {
                    const auto& attrs = sec.measures[i].attributes;
                    if (attrs.time)
                        time = *attrs.time;
                    const Time partial = attrs.partial ? *attrs.partial : time;

                    auto& measure = res.emplace_back();
                    measure.attributes = attrs;
                    measure.actual_time = partial;
                    if (sec.staves.size() <= idx) // Empty section, so empty measure
                        continue;

                    const auto [begin, end] = sec.beat_index_range_of_measure(i);
                    Staff& in_staff = sec.staves[idx];
                    const std::span in_beats{in_staff.data() + begin, end - begin};
                    LyMeasure* last_measure = res.size() == 1 ? nullptr : &measure - 1;
                    unroll_voices(measure, in_beats, last_measure);
                }
            }
            return res;
        }

        static void unroll_voices(LyMeasure& measure, const std::span<Beat> in_beats, LyMeasure* last_measure)
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

        static void place_clef_changes(LyStaff& staff)
        {
            // TODO
            (void)staff;
        }

        static void restructure_durations(LyStaff& staff)
        {
            // TODO
            (void)staff;
        }
    };

    LyMusic convert_to_ly(Music music) { return LyMusicConverter(std::move(music)).convert(); }
}
