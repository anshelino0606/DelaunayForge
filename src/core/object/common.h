#ifndef FEM_OBJECT_COMMON_H
#define FEM_OBJECT_COMMON_H

#include <string>

namespace fem {

inline std::string get_display_name_from_raw(const std::string& raw_name) {
    std::string result;
    result.reserve(raw_name.size());

    bool make_char_capital = false;

    for (char c : raw_name) {
        if (c == '_') {
            make_char_capital = true;
        } else {
            if (make_char_capital) {
                result += std::toupper(c);
                make_char_capital = false;
            } else {
                result += c;
            }
        }
    }

    result[0] = std::toupper(result[0]);

    return result;
}

}

#endif // FEM_OBJECT_COMMON_H