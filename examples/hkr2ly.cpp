#include <iostream>
#include <fstream>
#include <hikari/api.h>
#include <clu/file.h>

int main(const int argc, const char** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: hkr2ly <in_file> <out_file>\n";
        return 1;
    }
    try
    {
        namespace fs = std::filesystem;
        const fs::path in = argv[1];
        const fs::path out = argv[2];
        std::cout << "Input: " << in << "\nOutput: " << out << '\n';
        hkr::Music music = hkr::parse_music(clu::read_all_text(in));
        std::ofstream out_file(argv[2]);
        out_file.exceptions(std::ofstream::badbit | std::ofstream::failbit);
        export_to_lilypond(out_file, std::move(music));
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
