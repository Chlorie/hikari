#pragma once

#include <unordered_map>
#include <deque>

#include "parser_types.h"

namespace hkr
{
    struct PreprocessedText
    {
        TextPositionMap text; // Main preprocessed text
        std::unordered_map<std::string, TextPositionMap*> macros; // Active macros
        std::deque<TextPositionMap> maps; // All macro information (including shadowed macros)
    };

    // The preprocessor reduces the original text with macros into a form with no macros.
    // The processed resultant text is produced, together with a map from each character
    // to its origin.
    class Preprocessor final
    {
    public:
        explicit Preprocessor(std::string text, std::size_t max_macro_length = 65535);

        PreprocessedText process();

    private:
        std::string text_;
        std::size_t max_macro_length_;
        std::vector<TextPosition> original_pos_;
        PreprocessedText res_;

        std::size_t offset_of(std::string_view view) const noexcept;

        void remove_whitespaces();
        void append_text_to_map(TextPositionMap& map, std::string_view view) const;
        void append_macro_to_map(TextPositionMap& map, std::string_view macro) const;
        void parse_consume_macro_def(std::string_view& view);
        std::string_view parse_consume_macro_ref(std::string_view& view) const;
        void validate_macro_name(std::string_view name) const;
    };
}
