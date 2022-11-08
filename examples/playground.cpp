#include <iostream>

#include <hikari/api.h>

int main()
{
    try
    {
        const auto music = hkr::parse_music(R"(
!a: %1200%!
!b: *a*! !c: *b*! !d: *c*!
!e: *d*! !f: *e*! !g: *f*!
!h: *g*!
*h*
)");
    }
    catch (const std::exception& exc)
    {
        std::cout << "Exception: " << exc.what() << '\n';
    }
    return 0;
}
