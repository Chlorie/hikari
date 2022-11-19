#include "measurifier.h"

#include <algorithm>
#include <fmt/format.h>

#include "hikari/api.h"

namespace hkr
{
    Music parse_music(std::string text)
    {
        auto preproc = Preprocessor(std::move(text)).process();
        auto unmeasured = Parser(std::move(preproc)).parse();
        auto measured = Measurifier(std::move(unmeasured)).process();
        return measured;
    }

    Music Measurifier::process()
    {
        for (auto& in_sec : input_)
            res_.push_back(convert_section(in_sec));
        return std::move(res_);
    }

    Section Measurifier::convert_section(UnmeasuredSection& input)
    {
        Section res;
        Time partial;
        std::size_t beat_of_measure = 0;

        res.staves.resize(input.size());
        const std::size_t n_beats = std::ranges::max(input, std::less{}, &UnmeasuredStaff::size).size();
        for (auto& staff : res.staves)
            staff.resize(n_beats);

        // We need to loop inside-out, i.e. collect every beat synchronizedly from each of the staves
        // This is because we need to collect attributes from the staves
        for (std::size_t i = 0; i < n_beats; i++)
        {
            Measure::Attributes attrs;
            for (std::size_t j = 0; j < res.staves.size(); j++)
            {
                auto& in_staff = input[j];
                if (i >= in_staff.size()) // This staff ends early
                {
                    res.staves[j][i].emplace_back().emplace_back(); // Emplace a rest
                    continue;
                }
                auto& in_beat = in_staff[i];
                if (beat_of_measure != 0 && !in_beat.attrs.is_null())
                {
                    const std::string pos = fmt::format("on beat {}, measure {} with {}/{} time", //
                        beat_of_measure + 1, n_measures_, partial.numerator, partial.denominator);
                    if (in_beat.attrs.time.has_value() || in_beat.attrs.partial.has_value())
                        throw ParseError("Time signatures should only appear at the beginning of measures, "
                                         "but got a time signature " +
                            pos);
                    else
                        throw ParseError("Key signatures should only appear at the beginning of measures, "
                                         "but got a key signature " +
                            pos);
                }
                attrs.merge_with(in_beat.attrs);
                res.staves[j][i] = std::move(in_beat.beat);
            }
            if (beat_of_measure == 0)
            {
                res.measures.push_back({.start_beat = i, .attributes = attrs});
                if (attrs.time)
                    time_ = *attrs.time;
                if (attrs.partial) // Partial measures doesn't count toward the number
                    partial = *attrs.partial;
                else
                {
                    partial = time_;
                    n_measures_++;
                }
            }
            if (++beat_of_measure == static_cast<std::size_t>(partial.numerator)) // Last beat of current measure
                beat_of_measure = 0;
        }

        if (beat_of_measure != 0 && &input != &input_.back())
            throw ParseError(fmt::format("The section ends on an incomplete measure, beat {} of measure {} "
                                         "with {}/{} time",
                beat_of_measure, n_measures_, partial.numerator, partial.denominator));

        return res;
    }
} // namespace hkr
