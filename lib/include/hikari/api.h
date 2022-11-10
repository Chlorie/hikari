#pragma once

#include <string>
#include <ostream>

#include "types.h"

namespace hkr
{
    HIKARI_API Music parse_music(std::string text);
    HIKARI_API void export_to_lilypond(std::ostream& stream, const Music& music);
} // namespace hkr
