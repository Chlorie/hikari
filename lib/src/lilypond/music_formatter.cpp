#include "types.h"

#include <bit>
#include <algorithm>

#include "hikari/api.h"
#include "indented_formatter.h"

namespace hkr
{
    void export_to_lilypond(std::ostream& stream, Music music)
    {
        write_to_stream(stream, ly::convert_to_ly(std::move(music)));
    }
} // namespace hkr

namespace hkr::ly
{
    namespace
    {
        bool has_single_bit(const int value) noexcept { return std::has_single_bit(static_cast<unsigned>(value)); }

        int ottava_marking(const Clef clef) noexcept
        {
            switch (clef)
            {
                case Clef::bass_8va_bassa: return -1;
                case Clef::bass:
                case Clef::treble: return 0;
                case Clef::treble_8va: return 1;
                default: return 0;
            }
        }

        bool derived_from_treble(const Clef clef) noexcept
        {
            switch (clef)
            {
                case Clef::treble:
                case Clef::treble_8va: return true;
                default: return false;
            }
        }
    } // namespace

    class LyFormatter
    {
    public:
        explicit LyFormatter(std::ostream& stream): file_(stream) {}

        void write(const LyMusic& music)
        {
            file_.println(R"(\version "2.22.1")");
            file_.println(R"(\language "english")");
            {
                auto single_voice = file_.new_scope("singleVoice = ");
                file_.println("\\stemNeutral");
                // TODO: revert tie direction (^~ or _~) if it goes into a multi-voiced measure
                file_.println("\\tieNeutral");
                file_.println("\\dotsNeutral");
                file_.println("\\tupletNeutral");
                file_.println("\\override Rest.voiced-position = 0");
            }
            {
                auto score = file_.new_scope("\\score");
                {
                    auto layout = file_.new_scope("\\layout");
                    {
                        auto ctx = file_.new_scope("\\context");
                        file_.println("\\Staff");
                        file_.println("\\override VerticalAxisGroup #'remove-first = ##t");
                        file_.println(R"(\consists "Merge_rests_engraver")");
                    }
                    {
                        auto ctx = file_.new_scope("\\context");
                        file_.println("\\PianoStaff");
                        file_.println(R"(\remove "Keep_alive_together_engraver")");
                    }
                }
                {
                    auto midi = file_.new_scope("\\midi");
                }
                {
                    auto main_scope = file_.new_scope("");
                    {
                        auto piano_staff = file_.new_scope("\\new PianoStaff");
                        file_.println("<<");
                        for (const auto& staff : music)
                        {
                            auto new_staff = file_.new_scope("\\new Staff");
                            file_.println("\\numericTimeSignature");
                            write_staff(staff);
                        }
                        file_.println(">>");
                    }
                }
            }
        }

    private:
        IndentedFormatter file_;
        Clef current_clef_ = Clef::none;

        void write_staff(const LyStaff& staff)
        {
            current_clef_ = Clef::none;
            const auto n_max_staves = std::ranges::max(staff, std::less{}, //
                [](const LyMeasure& measure) {
                    return measure.voices.size();
                }).voices.size();
            for (const auto& measure : staff)
            {
                write_measure_attributes(measure.attributes);
                write_measure(measure, n_max_staves);
            }
        }

        void write_measure_attributes(const Measure::Attributes& attrs)
        {
            // Time signatures
            if (attrs.time)
                file_.println("\\time {}/{}", attrs.time->numerator, attrs.time->denominator);
            if (attrs.partial)
                file_.println("\\partial {}*{}", attrs.partial->denominator, attrs.partial->numerator);

            // Key signatures
            if (attrs.key)
            {
                static constexpr std::string_view names[]{
                    "cf", "gf", "df", "af", "ef", "bf", "f", //
                    "c", "g", "d", "a", "e", "b", "fs", "cs" //
                };
                file_.println("\\key {} \\major", names[*attrs.key + 7]);
            }
        }

        static bool is_non_empty_voice(const LyVoice& voice)
        {
            return std::ranges::any_of(voice,
                [](const LyChord& chord) noexcept //
                { return chord.chord && !chord.chord->notes.empty(); });
        }

        static std::size_t count_non_empty_voices(const LyMeasure& measure)
        {
            std::size_t res = 0;
            for (const auto& voice : measure.voices)
                res += is_non_empty_voice(voice);
            return res;
        }

        void write_measure(const LyMeasure& measure, const std::size_t n_max_staves)
        {
            const std::size_t n_non_empty_voices = count_non_empty_voices(measure);
            if (n_non_empty_voices == 0) // Just rests
            {
                file_.println("R{}*{}", measure.current_partial.denominator, measure.current_partial.numerator);
                return;
            }

            file_.print("<< ");
            for (std::size_t i = 0; const auto& voice : measure.voices)
            {
                if (i++ != 0)
                    file_.println("\\\\");
                file_.print("{{ ");

                if (is_non_empty_voice(voice))
                {
                    if (n_non_empty_voices == 1)
                        file_.print("\\singleVoice ");
                    write_voice(voice, measure.current_partial);
                }
                else
                    file_.print("s{}*{}", measure.current_partial.denominator, measure.current_partial.numerator);

                file_.print("}} ");
            }
            file_.println("{:\\>{}}>>", "", 2 * (n_max_staves - measure.voices.size()));
        }

        void write_voice(const LyVoice& voice, const Time measure_time)
        {
            const auto find_chord_end = [&](const LyChord& chord) -> clu::rational<int>
            { return &chord == &voice.back() ? measure_time.numerator : (&chord + 1)->start; };

            bool in_tuplet = false;
            for (const auto& chord : voice)
            {
                write_clef(chord.clef_change);

                if (chord.tuplet.pos == TupletGroupPosition::head && !in_tuplet)
                {
                    in_tuplet = true;
                    const auto ratio = chord.tuplet.ratio;
                    if (!has_single_bit(ratio.denominator()))
                        file_.print("\\once \\override TupletNumber.text = #tuplet-number::calc-fraction-text ");
                    file_.print("\\tuplet {}/{} {{ ", ratio.numerator(), ratio.denominator());
                }

                const auto duration =
                    (find_chord_end(chord) - chord.start) / measure_time.denominator * chord.tuplet.ratio;
                write_chord_with_duration(chord.chord, duration);

                if (chord.tuplet.pos == TupletGroupPosition::last)
                {
                    file_.print("}} ");
                    in_tuplet = false;
                }
            }
        }

        void write_clef(const Clef clef)
        {
            if (clef == Clef::none)
                return;
            if (const auto is_treble = derived_from_treble(clef); //
                current_clef_ == Clef::none || is_treble != derived_from_treble(current_clef_))
                file_.print("\\clef {} ", is_treble ? "treble" : "bass");
            if (const auto ottava = ottava_marking(clef); //
                ottava != ottava_marking(current_clef_))
                file_.print("\\ottava #{} ", ottava);
            current_clef_ = clef;
        }

        void write_chord_with_duration(const std::optional<Chord>& chord_or_spacer, const clu::rational<int> duration)
        {
            write_chord_notes(chord_or_spacer);
            write_duration(duration);
            if (!chord_or_spacer)
                return;
            if (chord_or_spacer->sustained)
                file_.print("~ ");
        }

        void write_chord_notes(const std::optional<Chord>& chord_or_spacer)
        {
            if (!chord_or_spacer) // spacer
            {
                file_.print("s");
                return;
            }

            const Chord& chord = *chord_or_spacer;
            if (chord.attributes.tempo) // TODO: any chance to support non-integer tempi?
                file_.print("\\tempo 4={} ", static_cast<int>(*chord.attributes.tempo));

            if (chord.notes.empty()) // rest
            {
                file_.print("r");
                return;
            }

            if (chord.notes.size() > 1)
                file_.print("< ");
            for (const auto note : chord.notes)
                write_note(note);
            if (chord.notes.size() > 1)
                file_.print("> ");
        }

        void write_note(const Note note)
        {
            static constexpr std::string_view accidentals[]{"ff", "f", "", "s", "ss"};
            static constexpr char base_names[] = "cdefgab";

            const char base_name = base_names[static_cast<int>(note.base)];
            const auto accidental = accidentals[note.accidental + 2];
            file_.print("{}{}", base_name, accidental);

            if (const int octave_delta = note.octave - 3; octave_delta > 0)
                file_.print("{:'>{}} ", "", octave_delta);
            else
                file_.print("{:,>{}} ", "", -octave_delta);
        }

        void write_duration(const clu::rational<int> duration)
        {
            const auto write_validated = [&]
            {
                // None power-of-2 denominator
                if (!has_single_bit(duration.denominator()))
                    return false;

                static constexpr std::string_view long_duration_names[]{
                    "", "1", "\\breve", "\\breve.", //
                    "\\longa", "", "\\longa.", "\\longa.." //
                };

                // Notes no shorter than a whole-note (semibreve)
                if (duration.denominator() == 1)
                {
                    const int value = duration.numerator();
                    if (value > 7)
                        return false;
                    file_.print("{} ", long_duration_names[value]);
                    return true;
                }

                // Normal lengths of 1/2^n
                if (duration.numerator() == 1)
                {
                    file_.print("{} ", duration.denominator());
                    return true;
                }

                // Dotted notes of 1 - 1/2^n
                const auto delta = clu::rational(1, duration.denominator());
                const auto rounded = (duration + clu::rational(1, duration.denominator())) / 2;
                const auto multi = rounded / delta;
                if (multi.denominator() != 1 || !has_single_bit(multi.numerator()))
                    return false;
                const auto dots = std::bit_width(static_cast<unsigned>(multi.numerator())) - 1;

                if (rounded.denominator() == 1)
                {
                    const int value = rounded.numerator();
                    if (value > 7)
                        return false;
                    file_.print("{}", long_duration_names[value]);
                }
                else
                    file_.print("{}", rounded.denominator());
                file_.print("{:.>{}} ", "", dots);
                return true;
            };

            if (!write_validated())
                // Something went wrong, we've got a bullsh*t duration, just do something that works
                file_.print("1*{}/{} ", duration.numerator(), duration.denominator());
        }
    };

    void write_to_stream(std::ostream& stream, const LyMusic& music) { LyFormatter(stream).write(music); }
}
