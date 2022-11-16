#pragma once

#include "preprocessor.h"

namespace hkr
{
    struct BeatWithMeasureAttrs
    {
        Beat beat;
        Measure::Attributes attrs;

        bool is_null() const noexcept;
        void replace_nulls_with_rests();
    };

    using UnmeasuredStaff = std::vector<BeatWithMeasureAttrs>;
    using UnmeasuredSection = std::vector<UnmeasuredStaff>;
    using UnmeasuredMusic = std::vector<UnmeasuredSection>;

    struct Transposition
    {
        Interval interval;
        bool up = true;
    };

    class Parser final
    {
    public:
        explicit Parser(PreprocessedText text): text_(std::move(text)) {}

        UnmeasuredMusic parse();

    private:
        PreprocessedText text_;
        UnmeasuredMusic music_;
        Measure::Attributes measure_attrs_;
        Chord::Attributes chord_attrs_;
        Transposition transposition_;
        int octave_ = 4;

        std::size_t offset_of(std::string_view view) const noexcept;
        TextPosition pos_of(std::string_view view, std::size_t offset = 0) const noexcept;

        bool parse_attributes(std::string_view& text);
        void parse_one_attribute(std::string_view text);
        void parse_transposition(std::string_view text);
        void parse_time_signature(std::string_view text);
        void parse_key_signature(std::string_view text);
        void parse_tempo(std::string_view text);
        void ensure_no_measure_attributes(TextPosition pos) const;

        std::string_view isolate_current_section(std::string_view& text) const;
        void parse_section(std::string_view text);

        std::string_view isolate_current_staff(std::string_view& text) const;
        void parse_staff(std::string_view text);

        std::string_view isolate_current_voiced_segment(std::string_view& text) const;
        void parse_voiced_segment(std::string_view text);
        void parse_voice(std::string_view text, std::size_t starting_beat, std::size_t voice_idx);

        std::string_view isolate_current_beat_in_voice(std::string_view& text) const;
        void parse_beat_in_voice(std::string_view text, BeatWithMeasureAttrs& beat, std::size_t voice_idx);
        Chord parse_chord(std::string_view& text);
        Note parse_note(std::string_view& text);
    };
}
