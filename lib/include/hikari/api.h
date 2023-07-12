#pragma once

#include <string>
#include <ostream>

#include "types.h"

HIKARI_SUPPRESS_EXPORT_WARNING
namespace hkr
{
    /**
     * \brief Parse a string into a structured form.
     * \details Please refer to the syntax guide for more details.
     * \param text Text input.
     * \return Parsed music structure.
     */
    HIKARI_API Music parse_music(std::string text);

    /**
     * \brief Convert structured music into Lilypond notation.
     * \param stream The output stream to write into.
     * \param music The music to export.
     */
    HIKARI_API void export_to_lilypond(std::ostream& stream, Music music);
} // namespace hkr
HIKARI_RESTORE_EXPORT_WARNING
