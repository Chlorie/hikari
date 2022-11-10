#pragma once

#include "hikari/types.h"

namespace hkr
{
    class TieProcessor final
    {
    public:
        explicit TieProcessor(Music music): music_(std::move(music)) {}
        Music process();

    private:
        Music music_;
    };
}
