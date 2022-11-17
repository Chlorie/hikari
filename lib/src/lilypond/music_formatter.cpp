#include "types.h"

#include <bit>
#include <algorithm>
#include <clu/assertion.h>

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
                auto score = file_.new_scope("\\score");
                {
                    auto layout = file_.new_scope("\\layout");
                    {
                        auto ctx = file_.new_scope("\\context");
                        file_.println("\\Staff");
                        file_.println("\\override VerticalAxisGroup #'remove-first = ##t");
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

        void write_staff(const LyStaff& staff)
        {
            for (const auto& measure : staff)
            {
                write_measure_attributes(measure.attributes);
                write_measure(measure);
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

        bool is_measure_empty(const LyMeasure& measure) const
        {
            for (const auto& voice : measure.voices)
                for (const auto& chord : voice)
                    if (chord.chord && !chord.chord->notes.empty()) // Not rest or spacer
                        return false;
            return true;
        }

        void write_measure(const LyMeasure& measure)
        {
            if (is_measure_empty(measure)) // Just rests
            {
                file_.println("R{}*{}", measure.actual_time.denominator, measure.actual_time.numerator);
                return;
            }
            if (measure.voices.size() > 1)
                file_.print("<< ");
            for (std::size_t i = 0; const auto& voice : measure.voices)
            {
                if (i++ != 0)
                    file_.println("\\\\");
                write_voice(voice, measure.actual_time);
            }
            if (measure.voices.size() > 1)
                file_.print(">>");
            file_.println();
        }

        void write_voice(const LyVoice& voice, const Time measure_time)
        {
            const auto find_chord_end = [&](const LyChord& chord) -> clu::rational<int>
            { return &chord == &voice.back() ? measure_time.numerator : (&chord + 1)->start; };

            bool in_tuplet = false;
            for (const auto& chord : voice)
            {
                write_clef(chord.clef_change);

                // Deal with tuplets
                switch (chord.tuplet.pos)
                {
                    case TupletGroupPosition::head:
                    {
                        if (!in_tuplet)
                        {
                            in_tuplet = true;
                            const auto ratio = chord.tuplet.ratio;
                            file_.print("\\tuplet {}/{} {{ ", ratio.numerator(), ratio.denominator());
                        }
                        break;
                    }
                    case TupletGroupPosition::last:
                    {
                        file_.print("}} ");
                        in_tuplet = false;
                        break;
                    }
                    default: break;
                }

                const auto duration =
                    (find_chord_end(chord) - chord.start) / measure_time.denominator * chord.tuplet.ratio;
                write_chord_with_duration(chord.chord, duration);
            }
        }

        void write_clef(const Clef clef)
        {
            if (clef == Clef::none)
                return;
            file_.print("\\clef ");
            switch (clef)
            {
                case Clef::bass_8vb: file_.print("bass_8 "); return;
                case Clef::bass: file_.print("bass "); return;
                case Clef::treble: file_.print("treble "); return;
                case Clef::treble_8va: file_.print("treble^8 "); return;
                default: clu::unreachable();
            }
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
            if (chord.attributes.tempo)
                file_.print("\\tempo 4={} ", *chord.attributes.tempo);

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
