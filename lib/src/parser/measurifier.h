#pragma once

#include "hikari/types.h"
#include "parser.h"

namespace hkr
{
    class Measurifier final
    {
    public:
        explicit Measurifier(UnmeasuredMusic input): input_(std::move(input)) {}

        Music process();

    private:
        Section convert_section(UnmeasuredSection& input);

        std::size_t n_measures_ = 0;
        UnmeasuredMusic input_;
        Music res_;
    };
}
