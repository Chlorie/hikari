#include "lilypond_converter.h"

#include <ranges>
#include <algorithm>
#include <fmt/format.h>
#include <clu/assertion.h>

namespace hkr
{
    void export_to_lilypond(std::ostream& stream, const Music& music)
    {
        LilypondConverter(music).write_to_stream(stream);
    }

    void LilypondConverter::write_to_stream(std::ostream& stream)
    {
        stream_ = &stream;
        stream << R"(\version "2.22.1"
\language "english"
\score {
    \layout {
        \context {
            \Staff
            \override VerticalAxisGroup #'remove-first = ##t
        }
        \context {
            \PianoStaff
            \remove "Keep_alive_together_engraver"
        }
    }
    \midi {}
    {
        \new PianoStaff {<<
)";
        const auto n_staves = std::ranges::max(music_, std::less{}, //
            [](const Section& sec) {
                return sec.staves.size();
            }).staves.size();
        clefs_.resize(n_staves);
        for (std::size_t i = 0; i < n_staves; i++)
        {
            stream << R"(            \new Staff \with {
                midiInstrument = "acoustic grand"
            } {
                \numericTimeSignature
)";
            for (std::size_t j = 0; j < music_.size(); j++)
                write_staff_section(i, j);
            stream << "            }\n";
        }
        stream << R"(        >>}
    }
})";
    }

    void LilypondConverter::write_staff_section(const std::size_t staff_idx, const std::size_t section_idx)
    {
        const Section& sec = music_[section_idx];
        if (sec.staves.size() <= staff_idx)
        {
            for (const auto& measure : sec.measures)
            {
                write_measure_attributes(measure.attributes);
                *stream_ << fmt::format("R{}*{}\n", partial_.denominator, partial_.numerator);
            }
            return;
        }
    }

    void LilypondConverter::write_measure_attributes(const Measure::Attributes& attrs)
    {
        // Time signatures
        if (attrs.time)
        {
            time_ = *attrs.time;
            *stream_ << fmt::format("\\time {}/{}\n", time_.numerator, time_.denominator);
        }
        if (attrs.partial)
        {
            partial_ = *attrs.partial;
            *stream_ << fmt::format("\\partial {}*{}\n", partial_.denominator, partial_.numerator);
        }
        else
            partial_ = time_;

        // Key signatures
        if (attrs.key)
        {
            static constexpr std::string_view names[]{
                "cf", "gf", "df", "af", "ef", "bf", "f", //
                "c", "g", "d", "a", "e", "b", "fs", "cs" //
            };
            *stream_ << fmt::format("\\key {} \\major\n", names[*attrs.key + 7]);
        }
    }
} // namespace hkr
