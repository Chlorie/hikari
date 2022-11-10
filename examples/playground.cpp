#include <iostream>
#include <fstream>

#include <hikari/api.h>

int main()
{
    try
    {
        const auto music = hkr::parse_music(R"(
%120%
DEFG, E-CD, -,
)");
        std::ofstream of("test.ly");
        export_to_lilypond(of, music);
        return 0;
    }
    catch (const std::exception& exc)
    {
        std::cout << "Exception: " << exc.what() << '\n';
    }
}
