#pragma once

#include "core/object/property.h"

#include <string>
#include <string_view>

namespace fem {

class BaseWidget {
protected:
    std::string get_type_label(const TypeInfo* type_info) {
        const DisplayName* display_name_attr = type_info->get_attribute<DisplayName>();
        return get_label(display_name_attr ? display_name_attr->display_name : type_info->get_name());
    }

    std::string get_label(std::string_view name) {
        std::string result_string;

        bool is_prev_char_space = false;
        bool is_prev_char_upper = false;
        size_t name_len = name.length();

        for (size_t i = 0; i != name_len; ++i) {
            char c = name[i];

            if (!result_string.empty() && std::isupper(c) && !is_prev_char_space && !is_prev_char_upper) {
                result_string += ' ';
            }

            is_prev_char_space = c == ' ';
            is_prev_char_upper = std::isupper(c);

            result_string += c;
        }

        return result_string;
    }
};

}