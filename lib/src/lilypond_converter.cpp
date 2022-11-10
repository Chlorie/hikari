#include "lilypond_converter.h"

#include <ranges>
#include <algorithm>

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
            \RemoveEmptyStaves
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
        for (std::size_t i = 0; i < n_staves; i++)
            write_staff(i);
        stream << R"(        >>}
    }
})";
    }

    void LilypondConverter::write_staff(const std::size_t staff_idx) const
    {

    }
} // namespace hkr
