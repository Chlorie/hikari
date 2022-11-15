#include <iostream>
#include <fstream>

#include <hikari/api.h>

int main()
{
    try
    {
        const auto music = hkr::parse_music(R"(
%120,4/1%
C,-,-,-,
%4/4%
{DEFG, E-CD, -, ,;,;,;,}
%2/4,3s%,,%5/8,4f%,,,,,%3//8,0s%,,,%2/2%,,,,
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
