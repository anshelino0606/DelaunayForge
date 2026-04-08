#ifndef FEM_FUNCTION_H
#define FEM_FUNCTION_H

#include "object.h"
#include "member.h"

#include <any>
#include <vector>
#include <cassert>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace fem {

template<typename T>
using EnableIfValidPropertyOwner = typename std::enable_if_t<
    std::is_base_of_v<Object, T> || 
    std::is_base_of_v<Struct, T>,
    int
>;


template<typename T>
struct FunctionTraits;

template<typename C, typename R, typename... Args>
struct FunctionTraits<R (C::*)(Args...)> {
    using owner_type  = C;
    using return_type = R;
    using args_tuple  = std::tuple<Args...>;
    static constexpr bool is_const = false;
};

template<typename C, typename R, typename... Args>
struct FunctionTraits<R (C::*)(Args...) const> {
    using owner_type  = C;
    using return_type = R;
    using args_tuple  = std::tuple<Args...>;
    static constexpr bool is_const = true;
};

class Function : public Member {
public:
    explicit Function(const char* name) : Member(name) { }

    virtual ~Function() = default;

    template<
        typename OwnerType, 
        EnableIfValidPropertyOwner<OwnerType> = 0
    >
    std::any invoke(OwnerType* owner, const std::vector<std::any>& args) {
        return handler_(owner, args);
    }

    virtual MemberType member_type() const override {
        return MemberType::FUNCTION;
    }

protected:
    using Handler = std::function<std::any(void*, const std::vector<std::any>&)>;

    Handler handler_;
};

template<typename FunctionPtr>
class FunctionImpl : public Function {
    using Traits     = FunctionTraits<FunctionPtr>;
    using OwnerType  = typename Traits::owner_type;
    using ReturnType = typename Traits::return_type;
    using ArgsTuple  = typename Traits::args_tuple;

public:
    explicit FunctionImpl(const char* name, FunctionPtr method)
        : Function(name)
    {
        handler_ = [method](void* owner, const std::vector<std::any>& args) -> std::any {
            constexpr size_t ArgCount = std::tuple_size_v<ArgsTuple>;
            assert(args.size() == ArgCount);

            if constexpr (Traits::is_const) {
                const OwnerType* obj = static_cast<OwnerType*>(owner);
                return invoke_impl(obj, method, args,
                    std::make_index_sequence<ArgCount>{});
            } else {
                OwnerType* obj = static_cast<OwnerType*>(owner);
                return invoke_impl(obj, method, args,
                    std::make_index_sequence<ArgCount>{});
            }
        };
    }

private:
    template<typename ObjPtr, size_t... I>
    static std::any invoke_impl(
        ObjPtr obj,
        FunctionPtr method,
        const std::vector<std::any>& args,
        std::index_sequence<I...>
    ) {
        if constexpr (std::is_void_v<ReturnType>) {
            (obj->*method)(
                std::any_cast<std::tuple_element_t<I, ArgsTuple>>(args[I])...
            );
            return {};
        } else {
            return (obj->*method)(
                std::any_cast<std::tuple_element_t<I, ArgsTuple>>(args[I])...
            );
        }
    }
};

template<typename FunctionType>
Function* allocate_function(const char* function_name) {
    return new FunctionType(function_name);
}

#define FEM_REGISTER_FUNCTION(TypeName, FunctionName, ...)                                      \
    using FunctionName##MethodPtr = decltype(&TypeName::FunctionName);                          \
    using FunctionName##ImplBase  = FunctionImpl<FunctionName##MethodPtr>;                      \
    class FunctionRegistrator_##FunctionName : public FunctionName##ImplBase {                  \
    public:                                                                                     \
        FunctionRegistrator_##FunctionName(const char* name)                                    \
            : FunctionName##ImplBase(name, &TypeName::FunctionName) {}                          \
        bool has_attribute(const char* attrName) const override {                               \
            FEM_DEFINE_HAS_ATTRIBUTE(attrName, __VA_ARGS__);                                    \
        }                                                                                       \
        const void* get_attribute(const char* attrName) const override {                        \
            FEM_DEFINE_GET_ATTRIBUTE(attrName, __VA_ARGS__);                                    \
        }                                                                                       \
    };                                                                                          \
    add_member(                                                                                 \
        TypeName::get_static_type_info(),                                                       \
        allocate_function<FunctionRegistrator_##FunctionName>(#FunctionName)                    \
    );

}

#endif // FEM_FUNCTION_H
