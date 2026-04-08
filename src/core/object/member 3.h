#ifndef FEM_OBJECT_MEMBER_H
#define FEM_OBJECT_MEMBER_H

#include <string>
#include <cassert>

namespace fem {

enum class MemberType {
    PROPERTY,
    FUNCTION
};

class Member {
public:
    Member(const char* name) {
        assert(name);

        raw_name_ = name;

        display_name_.reserve(raw_name_.size());

        bool make_char_capital = false;

        for (char c : raw_name_) {
            if (c == '_') {
                make_char_capital = true;
            } else {
                if (make_char_capital) {
                    display_name_ += std::toupper(c);
                    make_char_capital = false;
                } else {
                    display_name_ += c;
                }
            }
        }

        display_name_[0] = std::toupper(display_name_[0]);
    }

    virtual ~Member() = default;

    virtual MemberType member_type() const = 0;

    std::string_view raw_name() const { return raw_name_; }
    const std::string& display_name() const { return display_name_; }

    template<typename T>
    bool has_attribute() const {
        return has_attribute(T::name);
    }

    template<typename T>
    const T* get_attribute() const {
        if (const void* v = get_attribute(T::name))
            return static_cast<const T*>(v);
        return nullptr;
    }

protected:
    std::string_view raw_name_;
    std::string display_name_;

    virtual bool has_attribute(const char*) const { return false; }
    virtual const void* get_attribute(const char*) const { return nullptr; }
};

}

#endif // FEM_OBJECT_MEMBER_H