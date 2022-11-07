#include <iostream>

#include <hikari/api.h>

int main()
{
    try
    {
        const auto music = hkr::parse_music(R"(
%120,4/4,7s,#1%
!a: ABC>!
!b: *a* DE *a*!
!a: *a* *a* *a* *a*!
!a: *a* *a* *a* *a*!
!a: *a* *a* *a* *a*!
!a: *a* *a* *a* *a*!
!a: *a* *a* *a* *a*!
!a: *a* *a* *a* *a*!
!a: *a* *a* *a* *a*!
!a: *a* *a* *a* *a*!
*a*;*b*|[*a*;*b*]
)");
    }
    catch (const std::exception& exc)
    {
        std::cout << "Exception: " << exc.what() << '\n';
    }
    return 0;
}
