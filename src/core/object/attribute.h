#ifndef FEM_ATTRIBUTE
#define FEM_ATTRIBUTE

#include <tuple>

namespace fem {

#define FEM_DEFINE_ATTRIBUTE(Attr)                              \
    constexpr static const char* name = #Attr;                  \
    constexpr const char* get_name() const { return name; } 

template<typename ...Attrs>
constexpr auto setup_attributes(Attrs&&... attrs)
{
    return std::make_tuple(std::forward<Attrs>(attrs)...);
}

#define FEM_DEFINE_HAS_ATTRIBUTE(AttributeName, ...)                                        \
    static auto attributes = setup_attributes(__VA_ARGS__);                                 \
    return std::apply([&](const auto&... args)                                              \
    {                                                                                       \
        return ( (strcmp(AttributeName, args.get_name()) == 0) || ... );                    \
    }, attributes);

#define FEM_DEFINE_GET_ATTRIBUTE(AttributeName, ...)                                        \
    static auto attributes = setup_attributes(__VA_ARGS__);                                 \
    return std::apply(                                                                      \
        [&](auto const&... args)                                                            \
        {                                                                                   \
            const void* res = nullptr;                                                      \
            (( strcmp(AttributeName, args.get_name()) == 0                                  \
                    && (res = &args, true) )                                                \
                || ...);                                                                    \
            return res;                                                                     \
        },                                                                                  \
        attributes                                                                          \
    );

struct DisplayName {
    FEM_DEFINE_ATTRIBUTE(DisplayName);

    constexpr DisplayName(const char* in_display_name)
        : display_name(in_display_name) { } 

    const char* display_name;
};

struct NoTypeHeader {
    FEM_DEFINE_ATTRIBUTE(NoTypeHeader);
};

}

#endif // FEM_ATTRIBUTE