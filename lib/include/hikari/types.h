#pragma once

#include <vector>
#include <cstdint>
#include <optional>

#include "export.h"

namespace hkr
{
    // clang-format off
    /// \brief The base part of a note name (C, D, E, F, G, A, B).
    enum class NoteBase : std::uint8_t { c, d, e, f, g, a, b };

    /// \brief Different qualities for an interval.
    enum class IntervalQuality : std::uint8_t { diminished, minor, perfect, major, augmented };
    // clang-format on

    /// \brief A music interval between two notes.
    struct HIKARI_API Interval
    {
        int number = 1; ///< Diatonic number.
        IntervalQuality quality = IntervalQuality::perfect; ///< Interval quality.

        int semitones() const; ///< Count how many semitones are there in the interval.
    };

    /// \brief Time signature
    struct HIKARI_API Time
    {
        int numerator = 4; ///< Numerator of the time signature, or how many beats are there in a measure.
        int denominator = 4; ///< Denominator of the time signature, or the duration of a single beat.
    };

    /// \brief A musical note
    struct HIKARI_API Note
    {
        NoteBase base; ///< Base of the note.
        int octave; ///< Octave of the note, C4 (octave=4) is the middle C.
        int accidental; ///< Accidental of a note, positive integer for the amount of sharps, or flats if negative.

        Note transposed_up(int semitones) const noexcept; ///< Find the note n seminotes above this one.
        Note transposed_down(int semitones) const noexcept; ///< Find the note n seminotes below this one.
        Note transposed_up(Interval interval) const noexcept; ///< Find the note at some interval above this one.
        Note transposed_down(Interval interval) const noexcept; ///< Find the note at some interval below this one.

        std::int8_t pitch_id() const; ///< Get the MIDI pitch ID of the note.
    };

    /// \brief A chord containing multiple notes.
    struct HIKARI_API Chord
    {
        /// \brief Attributes of a chord.
        struct HIKARI_API Attributes
        {
            std::optional<float> tempo; ///< Tempo marking of this chord.
        };

        std::vector<Note> notes; ///< Constituents of this chord.
        bool sustained = false; ///< Whether this chord is a prolongation of the previous one.
        Attributes attributes; ///< Attributes of this chord.
    };

    using Voice = std::vector<Chord>; ///< A voice containing multiple chords.
    using Beat = std::vector<Voice>; ///< A beat containing multiple voices.
    using Staff = std::vector<Beat>; ///< A staff containing multiple beats.

    /// \brief Information about a measure.
    struct HIKARI_API Measure
    {
        /// \brief Attributes of a measure.
        struct HIKARI_API Attributes
        {
            std::optional<int> key; ///< The amount of sharps in the key signature, negative integer for flats.
            std::optional<Time> time; ///< Time signature of the current measure.
            std::optional<Time> partial; ///< The actual time of the current measure (for pick-up beats).

            /**
             * \brief Merge another set of measure attributes into this one.
             * \details Any non-null attribute in the other attribute set will be overwritten upon this set.
             * \param other The other measure.
             */
            void merge_with(const Attributes& other);

            /// \brief Checks whether this attribute set is completely empty.
            bool is_null() const noexcept { return !key && !time && !partial; }
        };

        std::size_t start_beat = 0; ///< Index of the first beat in this measure.
        Attributes attributes; ///< Attributes of this measure.
    };

    /// \brief A music section, containing multiple staves.
    struct HIKARI_API Section
    {
        std::vector<Staff> staves; ///< The staves
        std::vector<Measure> measures; ///< Measure information

        /**
         * \brief Find the starting and ending beat indices of a measure in this section.
         * \param measure Index of the measure.
         * \return A pair of std::size_t being the starting (inclusive) and ending (exclusive) beats' indices.
         */
        std::pair<std::size_t, std::size_t> beat_index_range_of_measure(std::size_t measure) const;
    };

    using Music = std::vector<Section>; ///< Music structure, containing multiple sections.
} // namespace hkr
