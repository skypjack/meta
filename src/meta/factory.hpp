#ifndef factory_HPP
#define factory_HPP


#include <cassert>
#include <utility>
#include <algorithm>
#include <functional>
#include <type_traits>
#include "meta.hpp"


namespace meta {


template<typename>
class factory;


template<typename Type, typename... Property>
factory<Type> reflect(const char *str, Property &&... property) noexcept;


/**
 * @brief A meta factory to be used for reflection purposes.
 *
 * A meta factory is an utility class used to reflect types, data and functions
 * of all sorts. This class ensures that the underlying web of types is built
 * correctly and performs some checks in debug mode to ensure that there are no
 * subtle errors at runtime.
 *
 * @tparam Type Reflected type for which the factory was created.
 */
template<typename Type>
class factory {
    static_assert(std::is_object_v<Type> && !(std::is_const_v<Type> || std::is_volatile_v<Type>));

    template<auto Data>
    static std::enable_if_t<std::is_member_object_pointer_v<decltype(Data)>, decltype(std::declval<Type>().*Data)>
    actual_type();

    template<auto Data>
    static std::enable_if_t<std::is_pointer_v<decltype(Data)>, decltype(*Data)>
    actual_type();

    template<auto Data>
    using data_type = std::remove_reference_t<decltype(factory::actual_type<Data>())>;

    template<auto Func>
    using func_type = internal::function_helper<std::integral_constant<decltype(Func), Func>>;

    template<typename Node>
    inline bool duplicate(const std::size_t &id, const Node *node) noexcept {
        return node ? node->id == id || duplicate(id, node->next) : false;
    }

    inline bool duplicate(const any &key, const internal::prop_node *node) noexcept {
        return node ? node->key() == key || duplicate(key, node->next) : false;
    }

    template<typename>
    internal::prop_node * properties() {
        return nullptr;
    }

    template<typename Owner, typename Property, typename... Other>
    internal::prop_node * properties(Property &&property, Other &&... other) {
        static auto prop{std::move(property)};

        static internal::prop_node node{
            properties<Owner>(std::forward<Other>(other)...),
            []() -> any {
                return std::get<0>(prop);
            },
            []() -> any {
                return std::get<1>(prop);
            },
            []() -> meta::prop {
                return &node;
            }
        };

        assert(!duplicate(any{std::get<0>(prop)}, node.next));
        return &node;
    }

    template<typename... Property>
    factory<Type> type(const char *name, Property &&... property) noexcept {
        static internal::type_node node{
            name,
            std::hash<std::string>{}(name),
            internal::type_info<>::type,
            properties<Type>(std::forward<Property>(property)...),
            std::is_void_v<Type>,
            std::is_integral_v<Type>,
            std::is_floating_point_v<Type>,
            std::is_enum_v<Type>,
            std::is_union_v<Type>,
            std::is_class_v<Type>,
            std::is_pointer_v<Type>,
            std::is_function_v<Type>,
            std::is_member_object_pointer_v<Type>,
            std::is_member_function_pointer_v<Type>,
            []() -> meta::type {
                return internal::type_info<std::remove_pointer_t<Type>>::resolve();
            },
            &internal::destroy<Type>,
            []() -> meta::type {
                return &node;
            }
        };

        assert(!duplicate(node.id, node.next));
        assert(!internal::type_info<Type>::type);
        internal::type_info<Type>::type = &node;
        internal::type_info<>::type = &node;

        return *this;
    }

    factory() noexcept = default;

public:
    /**
     * @brief Assigns a meta base to a meta type.
     *
     * A reflected base class must be a real base class of the reflected type.
     *
     * @tparam Base Type of the base class to assign to the meta type.
     * @return A meta factory for the parent type.
     */
    template<typename Base>
    factory base() noexcept {
        static_assert(std::is_base_of_v<Base, Type>);
        auto * const type = internal::type_info<Type>::resolve();

        static internal::base_node node{
            type->base,
            type,
            &internal::type_info<Base>::resolve,
            [](void *instance) -> void * {
                return static_cast<Base *>(static_cast<Type *>(instance));
            },
            []() -> meta::base {
                return &node;
            }
        };

        assert((!internal::type_info<Type>::template base<Base>));
        internal::type_info<Type>::template base<Base> = &node;
        type->base = &node;

        return *this;
    }

    /**
     * @brief Assigns a meta conversion function to a meta type.
     *
     * The given type must be such that an instance of the reflected type can be
     * converted to it.
     *
     * @tparam To Type of the conversion function to assign to the meta type.
     * @return A meta factory for the parent type.
     */
    template<typename To>
    factory conv() noexcept {
        static_assert(std::is_convertible_v<Type, std::decay_t<To>>);
        auto * const type = internal::type_info<Type>::resolve();

        static internal::conv_node node{
            type->conv,
            type,
            &internal::type_info<To>::resolve,
            [](void *instance) -> any {
                return static_cast<std::decay_t<To>>(*static_cast<Type *>(instance));
            },
            []() -> meta::conv {
                return &node;
            }
        };

        assert((!internal::type_info<Type>::template conv<To>));
        internal::type_info<Type>::template conv<To> = &node;
        type->conv = &node;

        return *this;
    }

    /**
     * @brief Assigns a meta constructor to a meta type.
     *
     * Free functions can be assigned to meta types in the role of
     * constructors. All that is required is that they return an instance of the
     * underlying type.<br/>
     * From a client's point of view, nothing changes if a constructor of a meta
     * type is a built-in one or a free function.
     *
     * @tparam Func The actual function to use as a constructor.
     * @tparam Property Types of properties to assign to the meta data.
     * @param property Properties to assign to the meta data.
     * @return A meta factory for the parent type.
     */
    template<auto Func, typename... Property>
    factory ctor(Property &&... property) noexcept {
        using helper_type = internal::function_helper<std::integral_constant<decltype(Func), Func>>;
        static_assert(std::is_same_v<typename helper_type::return_type, Type>);
        auto * const type = internal::type_info<Type>::resolve();

        static internal::ctor_node node{
            type->ctor,
            type,
            properties<typename helper_type::args_type>(std::forward<Property>(property)...),
            helper_type::size,
            &helper_type::arg,
            [](any * const any) {
                return internal::invoke<Type, Func>(nullptr, any, std::make_index_sequence<helper_type::size>{});
            },
            []() -> meta::ctor {
                return &node;
            }
        };

        assert((!internal::type_info<Type>::template ctor<typename helper_type::args_type>));
        internal::type_info<Type>::template ctor<typename helper_type::args_type> = &node;
        type->ctor = &node;

        return *this;
    }

    /**
     * @brief Assigns a meta constructor to a meta type.
     *
     * A meta constructor is uniquely identified by the types of its arguments
     * and is such that there exists an actual constructor of the underlying
     * type that can be invoked with parameters whose types are those given.
     *
     * @tparam Args Types of arguments to use to construct an instance.
     * @tparam Property Types of properties to assign to the meta data.
     * @param property Properties to assign to the meta data.
     * @return A meta factory for the parent type.
     */
    template<typename... Args, typename... Property>
    factory ctor(Property &&... property) noexcept {
        using helper_type = internal::function_helper<Type(Args...)>;
        auto * const type = internal::type_info<Type>::resolve();

        static internal::ctor_node node{
            type->ctor,
            type,
            properties<typename helper_type::args_type>(std::forward<Property>(property)...),
            helper_type::size,
            &helper_type::arg,
            [](any * const any) {
                return internal::construct<Type, Args...>(any, std::make_index_sequence<helper_type::size>{});
            },
            []() -> meta::ctor {
                return &node;
            }
        };

        assert((!internal::type_info<Type>::template ctor<typename helper_type::args_type>));
        internal::type_info<Type>::template ctor<typename helper_type::args_type> = &node;
        type->ctor = &node;

        return *this;
    }

    /**
     * @brief Assigns a meta destructor to a meta type.
     *
     * Free functions can be assigned to meta types in the role of destructors.
     * The signature of the function should identical to the following:
     *
     * @code{.cpp}
     * void(Type &);
     * @endcode
     *
     * From a client's point of view, nothing changes if the destructor of a
     * meta type is the default one or a custom one.
     *
     * @tparam Func The actual function to use as a destructor.
     * @return A meta factory for the parent type.
     */
    template<auto *Func>
    factory dtor() noexcept {
        static_assert(std::is_invocable_v<decltype(Func), Type &>);
        auto * const type = internal::type_info<Type>::resolve();

        static internal::dtor_node node{
            type,
            [](handle handle) {
                return handle.type() == internal::type_info<Type>::resolve()->clazz()
                        ? ((*Func)(*static_cast<Type *>(handle.data())), true)
                        : false;
            },
            []() -> meta::dtor {
                return &node;
            }
        };

        assert(!internal::type_info<Type>::type->dtor);
        assert((!internal::type_info<Type>::template dtor<Func>));
        internal::type_info<Type>::template dtor<Func> = &node;
        internal::type_info<Type>::type->dtor = &node;

        return *this;
    }

    /**
     * @brief Assigns a meta data to a meta type.
     *
     * Both data members and static and global variables, as well as constants
     * of any kind, can be assigned to a meta type.<br/>
     * From a client's point of view, all the variables associated with the
     * reflected object will appear as if they were part of the type itself.
     *
     * @tparam Data The actual variable to attach to the meta type.
     * @tparam Property Types of properties to assign to the meta data.
     * @param str The name to assign to the meta data.
     * @param property Properties to assign to the meta data.
     * @return A meta factory for the parent type.
     */
    template<auto Data, typename... Property>
    factory data(const char *str, Property &&... property) noexcept {
        auto * const type = internal::type_info<Type>::resolve();

        if constexpr(std::is_same_v<Type, decltype(Data)>) {
            static internal::data_node node{
                str,
                std::hash<std::string>{}(str),
                type->data,
                type,
                properties<std::integral_constant<Type, Data>>(std::forward<Property>(property)...),
                true,
                true,
                &internal::type_info<Type>::resolve,
                [](handle, any &) { return false; },
                [](handle) -> any { return Data; },
                []() -> meta::data {
                    return &node;
                }
            };

            assert(!duplicate(node.id, node.next));
            assert((!internal::type_info<Type>::template data<Data>));
            internal::type_info<Type>::template data<Data> = &node;
            type->data = &node;
        } else {
            static internal::data_node node{
                str,
                std::hash<std::string>{}(str),
                type->data,
                type,
                properties<std::integral_constant<decltype(Data), Data>>(std::forward<Property>(property)...),
                std::is_const_v<data_type<Data>>,
                !std::is_member_object_pointer_v<decltype(Data)>,
                &internal::type_info<data_type<Data>>::resolve,
                &internal::setter<std::is_const_v<data_type<Data>>, Type, Data>,
                &internal::getter<Type, Data>,
                []() -> meta::data {
                    return &node;
                }
            };

            assert(!duplicate(node.id, node.next));
            assert((!internal::type_info<Type>::template data<Data>));
            internal::type_info<Type>::template data<Data> = &node;
            type->data = &node;
        }

        return *this;
    }

    /**
     * @brief Assigns a meta data to a meta type by means of its setter and
     * getter.
     *
     * Setters and getters can be either free functions, member functions or a
     * mix of them.<br/>
     * In case of free functions, setters and getters must accept an instance of
     * the parent type as their first argument. A setter has then an extra
     * argument of a type convertible to that of the parameter to set.<br/>
     * In case of member functions, getters have no arguments at all, while
     * setters has an argument of a type convertible to that of the parameter to
     * set.
     *
     * @tparam Setter The actual function to use as a setter.
     * @tparam Getter The actual function to use as a getter.
     * @tparam Property Types of properties to assign to the meta data.
     * @param str The name to assign to the meta data.
     * @param property Properties to assign to the meta data.
     * @return A meta factory for the parent type.
     */
    template<auto Setter, auto Getter, typename... Property>
    factory data(const char *str, Property &&... property) noexcept {
        using data_type = std::invoke_result_t<decltype(Getter), Type &>;
        static_assert(std::is_invocable_v<decltype(Setter), Type &, data_type>);
        auto * const type = internal::type_info<Type>::resolve();

        static internal::data_node node{
            str,
            std::hash<std::string>{}(str),
            type->data,
            type,
            properties<std::tuple<decltype(Setter), decltype(Getter)>>(std::forward<Property>(property)...),
            false,
            false,
            &internal::type_info<data_type>::resolve,
            &internal::setter<false, Type, Setter>,
            &internal::getter<Type, Getter>,
            []() -> meta::data {
                return &node;
            }
        };

        assert(!duplicate(node.id, node.next));
        assert((!internal::type_info<Type>::template data<Setter, Getter>));
        internal::type_info<Type>::template data<Setter, Getter> = &node;
        type->data = &node;

        return *this;
    }


    /**
     * @brief Assigns a meta funcion to a meta type.
     *
     * Both member functions and free functions can be assigned to a meta
     * type.<br/>
     * From a client's point of view, all the functions associated with the
     * reflected object will appear as if they were part of the type itself.
     *
     * @tparam Func The actual function to attach to the meta type.
     * @tparam Property Types of properties to assign to the meta function.
     * @param str The name to assign to the meta function.
     * @param property Properties to assign to the meta function.
     * @return A meta factory for the parent type.
     */
    template<auto Func, typename... Property>
    factory func(const char *str, Property &&... property) noexcept {
        auto * const type = internal::type_info<Type>::resolve();

        static internal::func_node node{
            str,
            std::hash<std::string>{}(str),
            type->func,
            type,
            properties<std::integral_constant<decltype(Func), Func>>(std::forward<Property>(property)...),
            func_type<Func>::size,
            func_type<Func>::is_const,
            func_type<Func>::is_static,
            &internal::type_info<typename func_type<Func>::return_type>::resolve,
            &func_type<Func>::arg,
            [](handle handle, any *any) {
                return internal::invoke<Type, Func>(handle, any, std::make_index_sequence<func_type<Func>::size>{});
            },
            []() -> meta::func {
                return &node;
            }
        };

        assert(!duplicate(node.id, node.next));
        assert((!internal::type_info<Type>::template func<Func>));
        internal::type_info<Type>::template func<Func> = &node;
        type->func = &node;

        return *this;
    }

    template<typename Other, typename... Property>
    friend factory<Other> reflect(const char *str, Property &&... property) noexcept;
};


/**
 * @brief Basic function to use for reflection.
 *
 * This is the point from which everything starts.<br/>
 * By invoking this function with a type that is not yet reflected, a meta type
 * is created to which it will be possible to attach data and functions through
 * a dedicated factory.
 *
 * @tparam Type Type to reflect.
 * @tparam Property Types of properties to assign to the reflected type.
 * @param str The name to assign to the reflected type.
 * @param property Properties to assign to the reflected type.
 * @return A meta factory for the given type.
 */
template<typename Type, typename... Property>
inline factory<Type> reflect(const char *str, Property &&... property) noexcept {
    return factory<Type>{}.type(str, std::forward<Property>(property)...);
}


/**
 * @brief Basic function to use for reflection.
 *
 * This is the point from which everything starts.<br/>
 * By invoking this function with a type that is not yet reflected, a meta type
 * is created to which it will be possible to attach data and functions through
 * a dedicated factory.
 *
 * @tparam Type Type to reflect.
 * @return A meta factory for the given type.
 */
template<typename Type>
inline factory<Type> reflect() noexcept {
    return factory<Type>{};
}


/**
 * @brief Returns the meta type associated with a given type.
 * @tparam Type Type to use to search for a meta type.
 * @return The meta type associated with the given type, if any.
 */
template<typename Type>
inline type resolve() noexcept {
    return internal::type_info<Type>::resolve()->clazz();
}


/**
 * @brief Returns the meta type associated with a given name.
 * @param str The name to use to search for a meta type.
 * @return The meta type associated with the given name, if any.
 */
inline type resolve(const char *str) noexcept {
    const auto *curr = internal::find_if([id = std::hash<std::string>{}(str)](auto *node) {
        return node->id == id;
    }, internal::type_info<>::type);

    return curr ? curr->clazz() : type{};
}


/**
 * @brief Iterates all the reflected types.
 * @tparam Op Type of the function object to invoke.
 * @param op A valid function object.
 */
template<typename Op>
void resolve(Op op) noexcept {
    internal::iterate([op = std::move(op)](auto *node) {
        op(node->clazz());
    }, internal::type_info<>::type);
}


}


#endif // factory_HPP
