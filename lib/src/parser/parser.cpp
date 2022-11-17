#include "hikari/api.h"

#include <ranges>
#include <algorithm>
#include <utility>
#include <fmt/format.h>
#include <clu/parse.h>
#include <clu/concepts.h>

#include "parser.h"

namespace hkr
{
    namespace
    {
        template <std::ranges::contiguous_range R>
            requires std::same_as<std::ranges::range_value_t<R>, char>
        std::string_view as_sv(R&& range)
        {
            return std::string_view(range.begin(), range.end());
        }

        bool consume_if_starts_with(std::string_view& view, const std::string_view prefix) noexcept
        {
            if (view.starts_with(prefix))
            {
                view.remove_prefix(prefix.size());
                return true;
            }
            return false;
        }

        bool consume_if_starts_with(std::string_view& view, const char prefix) noexcept
        {
            if (view.starts_with(prefix))
            {
                view.remove_prefix(1);
                return true;
            }
            return false;
        }
    } // namespace

    bool BeatWithMeasureAttrs::is_null() const noexcept { return std::ranges::all_of(beat, &Voice::empty); }

    void BeatWithMeasureAttrs::replace_nulls_with_rests()
    {
        for (Voice& voice : beat)
        {
            if (voice.empty())
                voice.emplace_back();
        }
    }

    UnmeasuredMusic Parser::parse()
    {
        measure_attrs_.time = Time{4, 4};
        std::string_view text = text_.text.content;
        while (!text.empty())
            parse_section(isolate_current_section(text));
        return std::move(music_);
    }

    std::size_t Parser::offset_of(const std::string_view view) const noexcept
    {
        return static_cast<std::size_t>(view.data() - text_.text.content.data());
    }

    TextPosition Parser::pos_of(const std::string_view view, const std::size_t offset) const noexcept
    {
        return text_.text.positions[offset_of(view) + offset];
    }

    bool Parser::parse_attributes(std::string_view& text)
    {
        if (!text.starts_with('%'))
            return false;
        const auto idx = text.find('%', 1);
        if (idx == npos)
            throw ParseError("Attribute specification block is not closed with another '%', beginning " + //
                pos_of(text).to_string());
        std::string_view attrs_view = text.substr(1, idx - 1);
        text.remove_prefix(idx + 1);
        for (const auto view : std::views::split(attrs_view, ','))
            parse_one_attribute(as_sv(view));
        return true;
    }

    void Parser::parse_one_attribute(const std::string_view text)
    {
        if (text.empty())
            throw ParseError("Empty attribute found " + pos_of(text).to_string());
        if (text[0] == '+' || text[0] == '-')
            parse_transposition(text);
        else if (text.find('/') != npos)
            parse_time_signature(text);
        else if (text.back() == 's' || text.back() == 'f')
            parse_key_signature(text);
        else
            parse_tempo(text);
    }

    void Parser::parse_transposition(std::string_view text)
    {
        transposition_.up = text[0] == '+';
        text.remove_prefix(1);
        if (text.empty())
            throw ParseError("Transposition specifier unexpectedly ends " + pos_of(text).to_string());

        const auto quality = [&]() -> IntervalQuality
        {
            using enum IntervalQuality;
            switch (text[0])
            {
                case 'd': return diminished;
                case 'm': return minor;
                case 'P': return perfect;
                case 'M': return major;
                case 'A': return augmented;
                default:
                    throw ParseError(fmt::format("Expecting interval quality abbreviation, only 'd' for "
                                                 "diminished, 'm' for minor, 'P' for perfect, 'M' for major, "
                                                 "and 'A' for augmented is accepted, but found '{}' {}",
                        text[0], pos_of(text).to_string()));
            }
        }();
        text.remove_prefix(1);

        if (const auto opt = clu::parse<int>(text); //
            !opt || *opt < 1 || *opt > 8)
            throw ParseError(fmt::format("Expecting an integer between 1 and 8 for the diatonic number "
                                         "of the transposition interval, but found '{}' {}",
                text, pos_of(text).to_string()));
        else
            transposition_.interval = {.number = *opt, .quality = quality};
    }

    void Parser::parse_time_signature(const std::string_view text)
    {
        const auto [partial, num_view, den_view] = [&]
        {
            const auto slash = text.find('/'); // not npos
            const bool p = text.size() > slash + 2 && text[slash + 1] == '/';
            return std::tuple(p, text.substr(0, slash), text.substr(slash + (p ? 2 : 1)));
        }();

        const auto check_number = [&](const std::string_view num_text, const std::string_view name)
        {
            const auto opt = clu::parse<int>(num_text);
            if (!opt || *opt <= 0 || *opt > 128)
                throw ParseError(fmt::format("The {} of a time signature should be a positive integer "
                                             "no greater than 128, but got '{}' {}",
                    name, num_text, pos_of(num_text).to_string()));
            return *opt;
        };

        const int num = check_number(num_view, "numerator");
        const int den = check_number(den_view, "denominator");

        if (!std::has_single_bit(static_cast<unsigned>(den)))
            throw ParseError(fmt::format("The denominator of a time signature should be "
                                         "a power of 2, but got {} {}",
                den, pos_of(den_view).to_string()));

        (partial ? measure_attrs_.partial : measure_attrs_.time) = Time{num, den};
    }

    void Parser::parse_key_signature(std::string_view text)
    {
        const int sign = text.back() == 's' ? 1 : -1;
        text.remove_suffix(1);
        const auto opt = clu::parse<int>(text);
        if (!opt)
            throw ParseError(fmt::format("A key signature specification should be a number followed by "
                                         "'s' or 'f' to indicate the amount of sharps or flats in that "
                                         "key signature, but got {}{} {}",
                text, sign == 1 ? 's' : 'f', pos_of(text).to_string()));
        const int num = *opt;
        if (num < 0 || num > 7)
            throw ParseError(fmt::format("The amount of sharps or flats in a key signature should be "
                                         "between 0 and 7, but got {} {}",
                num, pos_of(text).to_string()));
        measure_attrs_.key = num * sign;
    }

    void Parser::parse_tempo(const std::string_view text)
    {
        if (const auto opt = clu::parse<float>(text); !opt)
            throw ParseError(fmt::format("Unknown attribute '{}' {}", text, pos_of(text).to_string()));
        else if (const float tempo = *opt; tempo > 1000 || tempo < 10)
            throw ParseError(fmt::format(
                "Tempo markings should be between 10 and 1000, but got {} {}", tempo, pos_of(text).to_string()));
        else
            chord_attrs_.tempo = tempo;
    }

    void Parser::ensure_no_measure_attributes(const TextPosition pos) const
    {
        if (measure_attrs_.time.has_value() || measure_attrs_.partial.has_value())
            throw ParseError("Time signatures should only appear at the beginning of "
                             "bars, but got a time signature before a chord in the middle of a beat " +
                pos.to_string());
        if (measure_attrs_.key)
            throw ParseError("Key signatures should only appear at the beginning of "
                             "bars, but got a key signature before a chord in the middle of a beat " +
                pos.to_string());
    }

    std::string_view Parser::isolate_current_section(std::string_view& text) const
    {
        // Braced section
        if (text[0] == '{')
        {
            const auto idx = text.find('}');
            if (idx == npos)
                throw ParseError("A section is not closed by a right curly brace '}', starting " + //
                    pos_of(text).to_string());
            const auto res = text.substr(1, idx - 1);
            text.remove_prefix(idx + 1);
            return res;
        }
        // Brace-omitted section
        const auto idx = text.find('{');
        const auto res = text.substr(0, idx);
        text.remove_prefix(res.size());
        return res;
    }

    void Parser::parse_section(std::string_view text)
    {
        if (const auto idx = text.find('{'); idx != npos)
            throw ParseError("Sections are not nestable, but found '{' in a section " + //
                pos_of(text, idx).to_string());
        const auto& section = music_.emplace_back(); // Add a section
        while (!text.empty())
            parse_staff(isolate_current_staff(text));
        // Section with no staves (only attributes)
        if (section.empty())
            music_.pop_back();
    }

    std::string_view Parser::isolate_current_staff(std::string_view& text) const
    {
        std::size_t idx = 0;
        while (true)
        {
            idx = text.find_first_of("[;", idx);
            if (idx == npos)
            {
                const std::string_view res = text;
                text.remove_prefix(text.size());
                return res;
            }
            // Found '[' (start of a voiced segment)
            if (text[idx] == '[')
            {
                const auto closing = text.find_first_of("[]", idx + 1);
                if (closing == npos)
                    throw ParseError("A voiced segment is not closed by ']', starting " + //
                        pos_of(text, idx).to_string());
                if (text[closing] == '[')
                    throw ParseError("Voices are not nestable, but found '[' in a voice " + //
                        pos_of(text, idx).to_string());
                idx = closing + 1;
                continue;
            }
            // We have found a semicolon
            const std::string_view res = text.substr(0, idx);
            text.remove_prefix(idx + 1);
            return res;
        }
    }

    void Parser::parse_staff(std::string_view text)
    {
        auto& section = music_.back();
        const auto& staff = section.emplace_back();
        while (!text.empty())
            parse_voiced_segment(isolate_current_voiced_segment(text));
        // Remove the empty staff, when the staff only contains null beats of attributes.
        // Null beats are beats with no chord in it (compared to "empty beats" which
        // contain rests in them), used as a placeholder for temporarily saving end-of-beat
        // measure attributes. Null beats should not exist in the final data structure,
        // as they should be removed and gotten their attributes merged into the next beat
        // once we have parsed the next beat.
        if (staff.empty() || staff[0].beat.empty())
            section.pop_back();
    }

    std::string_view Parser::isolate_current_voiced_segment(std::string_view& text) const
    {
        // This segment is multi-voiced, just find the delimiter
        if (text[0] == '[')
        {
            const auto idx = text.find(']'); // Not npos, validated in "isolate_current_staff"
            const auto res = text.substr(1, idx - 1);
            text.remove_prefix(idx + 1);
            return res;
        }
        const auto idx = text.find('['); // Find next multi-voiced segment
        const auto res = text.substr(0, idx);
        text.remove_prefix(res.size());
        return res;
    }

    void Parser::parse_voiced_segment(std::string_view text)
    {
        auto& staff = music_.back().back();
        const auto starting_beat = staff.size();
        // Parse the respective voices
        for (std::size_t i = 0; const auto view : std::views::split(text, ';'))
        {
            parse_voice(as_sv(view), starting_beat, i);
            i++;
        }
        // If the last beat is a null beat, move the measure attribute into the class field,
        // and then remove the null beat
        if (BeatWithMeasureAttrs& last = staff.back(); last.is_null())
        {
            measure_attrs_ = std::exchange(last.attrs, {});
            staff.pop_back();
        }
        // Fill remaining null beats with rests
        for (auto i = starting_beat; i < staff.size(); i++)
            staff[i].replace_nulls_with_rests();
    }

    void Parser::parse_voice(std::string_view text, const std::size_t starting_beat, const std::size_t voice_idx)
    {
        auto& staff = music_.back().back(); // Get current staff
        std::size_t beat_idx = starting_beat;
        bool should_add_null_beat = false;

        const auto get_beat = [&](const std::size_t idx) -> BeatWithMeasureAttrs&
        {
            BeatWithMeasureAttrs& beat = idx < staff.size() ? staff[idx] : staff.emplace_back();
            // Fill former voices with fewer beats with null beats to match this voice
            for (std::size_t i = beat.beat.size(); i <= voice_idx; i++)
                beat.beat.emplace_back();
            return beat;
        };

        while (!text.empty())
        {
            BeatWithMeasureAttrs& beat = get_beat(beat_idx);
            parse_beat_in_voice(isolate_current_beat_in_voice(text), beat, voice_idx);
            // Only if we get a normal beat at the end, do we need to add another null beat
            // if we've got attributes to merge
            should_add_null_beat = !beat.beat[voice_idx].empty();
            beat_idx++;
        }

        if (should_add_null_beat && !measure_attrs_.is_null())
        {
            BeatWithMeasureAttrs& beat = get_beat(beat_idx++);
            beat.attrs.merge_with(measure_attrs_);
            measure_attrs_ = {};
        }

        // Fill up the current voice with null beats
        for (; beat_idx < staff.size(); beat_idx++)
            staff[beat_idx].beat.emplace_back();
    }

    std::string_view Parser::isolate_current_beat_in_voice(std::string_view& text) const
    {
        std::size_t idx = 0;
        while (true)
        {
            // Commas can also appear in attribute specifications, we need to skip those things
            idx = text.find_first_of("%,", idx);
            if (idx == npos)
            {
                const std::string_view res = text;
                text.remove_prefix(text.size());
                return res;
            }
            // Found '%' (start of some attributes)
            if (text[idx] == '%')
            {
                const auto closing = text.find('%', idx + 1);
                if (closing == npos)
                    throw ParseError("Attribute specification block is not closed with another '%', beginning " + //
                        pos_of(text, idx).to_string());
                idx = closing + 1;
                continue;
            }
            // We have found a comma
            const std::string_view res = text.substr(0, idx + 1); // Include the comma in the view
            text.remove_prefix(idx + 1);
            return res;
        }
    }

    void Parser::parse_beat_in_voice(std::string_view text, BeatWithMeasureAttrs& beat, const std::size_t voice_idx)
    {
        Voice& voice = beat.beat[voice_idx];
        while (!(text.empty() || text == ","))
        {
            if (parse_attributes(text)) // Accumulate attributes
                continue;
            voice.push_back(parse_chord(text));
            // We got a new chord, merge the measure attributes if needed
            if (voice.size() == 1) // Chord at the start of a beat
            {
                beat.attrs.merge_with(measure_attrs_);
                measure_attrs_ = {};
            }
            else // Got a measure attribute applied to a chord in the middle of a beat
                ensure_no_measure_attributes(pos_of(text));
        }

        // Fill current beat with rest if there's a delimiter
        if (text == "," && voice.empty())
        {
            voice.emplace_back().attributes = std::exchange(chord_attrs_, {});
            beat.attrs.merge_with(measure_attrs_);
            measure_attrs_ = {};
        }
        else if (text.empty())
        {
            if (!voice.empty()) // We've got notes, but no comma for ending the beat, err out
                throw ParseError("A beat should end with a comma, but a beat ends unexpectedly without the comma " +
                    pos_of(text).to_string());
            else // Apply measure attributes to the null beat
            {
                beat.attrs.merge_with(measure_attrs_);
                measure_attrs_ = {};
            }
        }
    }

    Chord Parser::parse_chord(std::string_view& text)
    {
        Chord chord{.attributes = std::exchange(chord_attrs_, {})};
        // Rest
        if (consume_if_starts_with(text, '.'))
            return chord;
        // Sustain
        if (consume_if_starts_with(text, '-'))
        {
            chord.sustained = true;
            return chord;
        }
        // Multi-note chord
        if (consume_if_starts_with(text, '('))
        {
            while (!consume_if_starts_with(text, ')'))
                chord.notes.push_back(parse_note(text));
            return chord;
        }
        // Single note
        chord.notes.push_back(parse_note(text));
        return chord;
    }

    Note Parser::parse_note(std::string_view& text)
    {
        if (text.empty())
            throw ParseError(
                "Expecting a note in the chord, but the beat unexpectedly ends " + pos_of(text).to_string());
        if (text[0] == '.' || text[0] == '-')
            throw ParseError("A chord enclosed with parentheses '()' should not contain rests '.' "
                             "or sustain markings '-', but got one " +
                pos_of(text).to_string());
        if (text[0] < 'A' || text[0] > 'G')
            throw ParseError(fmt::format("The base of a note should be an upper-cased letter from A to G, "
                                         "but got {} {}", //
                text[0], pos_of(text).to_string()));

        const auto full = text;
        static constexpr NoteBase bases[]{
            NoteBase::a, NoteBase::b, NoteBase::c, NoteBase::d, NoteBase::e, NoteBase::f, NoteBase::g //
        };
        const auto base = bases[text[0] - 'A'];
        text.remove_prefix(1);

        const int accidental = [&]
        {
            if (consume_if_starts_with(text, 'x'))
                return 2;
            if (consume_if_starts_with(text, '#'))
                return 1;
            if (consume_if_starts_with(text, "bb"))
                return -2;
            if (consume_if_starts_with(text, 'b'))
                return -1;
            return 0;
        }();

        if (const auto opt = clu::parse_consume<int>(text))
        {
            const int oct = *opt;
            if (oct > 10 || oct < -2)
                throw ParseError(fmt::format("Octave specifier should be an integer between -2 and 10, "
                                             "but got {} {}",
                    oct, pos_of(full).to_string()));
            octave_ = oct;
        }

        const int octave_diff = [&]
        {
            int res = 0;
            while (!text.empty() && (text[0] == '<' || text[0] == '>'))
            {
                res += text[0] == '<' ? -1 : 1;
                text.remove_prefix(1);
            }
            return res;
        }();

        const Note written_note = Note{.base = base, .octave = octave_ + octave_diff, .accidental = accidental};
        const Note note = transposition_.up //
            ? written_note.transposed_up(transposition_.interval)
            : written_note.transposed_down(transposition_.interval);
        try
        {
            (void)note.pitch_id();
        }
        catch (const std::out_of_range&)
        {
            const std::string_view note_view(full.data(), text.data());
            throw ParseError(fmt::format("The note {} applied with a transposition of {} semitone(s) {} "
                                         "gets a pitch id out of the range 0 to 127, {}",
                note_view, transposition_.interval.semitones(), transposition_.up ? "upwards" : "downwards",
                pos_of(full).to_string()));
        }

        return note;
    }
} // namespace hkr
