#ifndef META_FACTORY_HPP
#define META_FACTORY_HPP


#include <tuple>
#include <array>
#include <cassert>
#include <cstddef>
#include <utility>
#include <functional>
#include <type_traits>
#include "policy.hpp"
#include "meta.hpp"


namespace meta {


/**
 * @cond TURN_OFF_DOXYGEN
 * Internal details not to be documented.
 */


namespace internal {


template<typename>
struct function_helper;


template<typename Ret, typename... Args>
struct function_helper<Ret(Args...)> {
    using return_type = std::remove_cv_t<std::remove_reference_t<Ret>>;
    using args_type = std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...>;

    static constexpr auto size = sizeof...(Args);
    static constexpr auto is_const = false;

    static auto arg(typename internal::func_node::size_type index) noexcept {
        return std::array<type_node *, sizeof...(Args)>{{type_info<Args>::resolve()...}}[index];
    }
};


template<typename Ret, typename... Args>
struct function_helper<Ret(Args...) const>: function_helper<Ret(Args...)> {
    static constexpr auto is_const = true;
};


template<typename Ret, typename... Args, typename Class>
constexpr function_helper<Ret(Args...)>
to_function_helper(Ret(Class:: *)(Args...));


template<typename Ret, typename... Args, typename Class>
constexpr function_helper<Ret(Args...) const>
to_function_helper(Ret(Class:: *)(Args...) const);


template<typename Ret, typename... Args>
constexpr function_helper<Ret(Args...)>
to_function_helper(Ret(*)(Args...));


constexpr void to_function_helper(...);


template<typename Candidate>
using function_helper_t = decltype(to_function_helper(std::declval<Candidate>()));


template<typename Type, typename... Args, std::size_t... Indexes>
any construct(any * const args, std::index_sequence<Indexes...>) {
    [[maybe_unused]] auto direct = std::make_tuple((args+Indexes)->try_cast<Args>()...);
    any any{};

    if(((std::get<Indexes>(direct) || (args+Indexes)->convert<Args>()) && ...)) {
        any = Type{(std::get<Indexes>(direct) ? *std::get<Indexes>(direct) : (args+Indexes)->cast<Args>())...};
    }

    return any;
}


template<bool Const, typename Type, auto Data>
bool setter([[maybe_unused]] handle handle, [[maybe_unused]] any index, [[maybe_unused]] any value) {
    bool accepted = false;

    if constexpr(!Const) {
        if constexpr(std::is_function_v<std::remove_pointer_t<decltype(Data)>> || std::is_member_function_pointer_v<decltype(Data)>) {
            using helper_type = function_helper_t<decltype(Data)>;
            using data_type = std::tuple_element_t<!std::is_member_function_pointer_v<decltype(Data)>, typename helper_type::args_type>;
            static_assert(std::is_invocable_v<decltype(Data), Type &, data_type>);
            auto *clazz = any{handle}.try_cast<Type>();
            auto *direct = value.try_cast<data_type>();

            if(clazz && (direct || value.convert<data_type>())) {
                std::invoke(Data, *clazz, direct ? *direct : value.cast<data_type>());
                accepted = true;
            }
        } else if constexpr(std::is_member_object_pointer_v<decltype(Data)>) {
            using data_type = std::remove_cv_t<std::remove_reference_t<decltype(std::declval<Type>().*Data)>>;
            static_assert(std::is_invocable_v<decltype(Data), Type *>);
            auto *clazz = any{handle}.try_cast<Type>();

            if constexpr(std::is_array_v<data_type>) {
                using underlying_type = std::remove_extent_t<data_type>;
                auto *direct = value.try_cast<underlying_type>();
                auto *idx = index.try_cast<std::size_t>();

                if(clazz && idx && (direct || value.convert<underlying_type>())) {
                    std::invoke(Data, clazz)[*idx] = direct ? *direct : value.cast<underlying_type>();
                    accepted = true;
                }
            } else {
                auto *direct = value.try_cast<data_type>();

                if(clazz && (direct || value.convert<data_type>())) {
                    std::invoke(Data, clazz) = (direct ? *direct : value.cast<data_type>());
                    accepted = true;
                }
            }
        } else {
            static_assert(std::is_pointer_v<decltype(Data)>);
            using data_type = std::remove_cv_t<std::remove_reference_t<decltype(*Data)>>;

            if constexpr(std::is_array_v<data_type>) {
                using underlying_type = std::remove_extent_t<data_type>;
                auto *direct = value.try_cast<underlying_type>();
                auto *idx = index.try_cast<std::size_t>();

                if(idx && (direct || value.convert<underlying_type>())) {
                    (*Data)[*idx] = (direct ? *direct : value.cast<underlying_type>());
                    accepted = true;
                }
            } else {
                auto *direct = value.try_cast<data_type>();

                if(direct || value.convert<data_type>()) {
                    *Data = (direct ? *direct : value.cast<data_type>());
                    accepted = true;
                }
            }
        }
    }

    return accepted;
}


template<typename Type, auto Data, typename Policy>
any getter([[maybe_unused]] handle handle, [[maybe_unused]] any index) {
    auto dispatch = [](auto &&value) {
        if constexpr(std::is_same_v<Policy, as_void_t>) {
            return any{std::in_place_type<void>};
        } else if constexpr(std::is_same_v<Policy, as_alias_t>) {
            return any{std::ref(std::forward<decltype(value)>(value))};
        } else {
            static_assert(std::is_same_v<Policy, as_is_t>);
            return any{std::forward<decltype(value)>(value)};
        }
    };

    if constexpr(std::is_function_v<std::remove_pointer_t<decltype(Data)>> || std::is_member_function_pointer_v<decltype(Data)>) {
        static_assert(std::is_invocable_v<decltype(Data), Type &>);
        auto *clazz = any{handle}.try_cast<Type>();
        return clazz ? dispatch(std::invoke(Data, *clazz)) : any{};
    } else if constexpr(std::is_member_object_pointer_v<decltype(Data)>) {
        using data_type = std::remove_cv_t<std::remove_reference_t<decltype(std::declval<Type>().*Data)>>;
        static_assert(std::is_invocable_v<decltype(Data), Type *>);
        auto *clazz = any{handle}.try_cast<Type>();

        if constexpr(std::is_array_v<data_type>) {
            auto *idx = index.try_cast<std::size_t>();
            return (clazz && idx) ? dispatch(std::invoke(Data, clazz)[*idx]) : any{};
        } else {
            return clazz ? dispatch(std::invoke(Data, clazz)) : any{};
        }
    } else {
        static_assert(std::is_pointer_v<std::decay_t<decltype(Data)>>);

        if constexpr(std::is_array_v<std::remove_pointer_t<decltype(Data)>>) {
            auto *idx = index.try_cast<std::size_t>();
            return idx ? dispatch((*Data)[*idx]) : any{};
        } else {
            return dispatch(*Data);
        }
    }
}


template<typename Type, auto Candidate, typename Policy, std::size_t... Indexes>
any invoke([[maybe_unused]] handle handle, any *args, std::index_sequence<Indexes...>) {
    using helper_type = function_helper_t<decltype(Candidate)>;

    auto dispatch = [](auto *... args) {
        if constexpr(std::is_void_v<typename helper_type::return_type> || std::is_same_v<Policy, as_void_t>) {
            std::invoke(Candidate, *args...);
            return any{std::in_place_type<void>};
        } else if constexpr(std::is_same_v<Policy, as_alias_t>) {
            return any{std::ref(std::invoke(Candidate, *args...))};
        } else {
            static_assert(std::is_same_v<Policy, as_is_t>);
            return any{std::invoke(Candidate, *args...)};
        }
    };

    [[maybe_unused]] const auto direct = std::make_tuple([](meta::any *any, auto *instance) {
        using arg_type = std::remove_reference_t<decltype(*instance)>;

        if(!instance && any->convert<arg_type>()) {
            instance = any->try_cast<arg_type>();
        }

        return instance;
    }(args+Indexes, (args+Indexes)->try_cast<std::tuple_element_t<Indexes, typename helper_type::args_type>>())...);

    if constexpr(std::is_function_v<std::remove_pointer_t<decltype(Candidate)>>) {
        return (std::get<Indexes>(direct) && ...) ? dispatch(std::get<Indexes>(direct)...) : any{};
    } else {
        auto *clazz = any{handle}.try_cast<Type>();
        return (clazz && (std::get<Indexes>(direct) && ...)) ? dispatch(clazz, std::get<Indexes>(direct)...) : any{};
    }
}


}


/**
 * Internal details not to be documented.
 * @endcond TURN_OFF_DOXYGEN
 */


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
    template<typename Node>
    bool duplicate(const std::size_t identifier, const Node *node) noexcept {
        return node && (node->identifier == identifier || duplicate(identifier, node->next));
    }

    bool duplicate(const any &key, const internal::prop_node *node) noexcept {
        return node && (node->key() == key || duplicate(key, node->next));
    }

    template<typename>
    internal::prop_node * properties() {
        return nullptr;
    }

    template<typename Owner, typename Property, typename... Other>
    internal::prop_node * properties(Property &&property, Other &&... other) {
        static std::remove_cv_t<std::remove_reference_t<Property>> prop{};

        static internal::prop_node node{
            nullptr,
            []() -> any {
                return std::as_const(std::get<0>(prop));
            },
            []() -> any {
                return std::as_const(std::get<1>(prop));
            },
            []() noexcept -> meta::prop {
                return &node;
            }
        };

        prop = std::forward<Property>(property);
        node.next = properties<Owner>(std::forward<Other>(other)...);
        assert(!duplicate(any{std::get<0>(prop)}, node.next));
        return &node;
    }

    void unregister_prop(internal::prop_node **prop) {
        while(*prop) {
            auto *node = *prop;
            *prop = node->next;
            node->next = nullptr;
        }
    }

    void unregister_dtor() {
        if(auto node = internal::type_info<Type>::type->dtor; node) {
            internal::type_info<Type>::type->dtor = nullptr;
            *node->underlying = nullptr;
        }
    }

    template<auto Member>
    auto unregister_all(int)
    -> decltype((internal::type_info<Type>::type->*Member)->prop, void()) {
        while(internal::type_info<Type>::type->*Member) {
            auto node = internal::type_info<Type>::type->*Member;
            internal::type_info<Type>::type->*Member = node->next;
            unregister_prop(&node->prop);
            node->next = nullptr;
            *node->underlying = nullptr;
        }
    }

    template<auto Member>
    void unregister_all(char) {
        while(internal::type_info<Type>::type->*Member) {
            auto node = internal::type_info<Type>::type->*Member;
            internal::type_info<Type>::type->*Member = node->next;
            node->next = nullptr;
            *node->underlying = nullptr;
        }
    }

public:
    /*! @brief Default constructor. */
    factory() noexcept = default;

    /**
     * @brief Extends a meta type by assigning it an identifier and properties.
     * @tparam Property Types of properties to assign to the meta type.
     * @param identifier Unique identifier.
     * @param property Properties to assign to the meta type.
     * @return A meta factory for the parent type.
     */
    template<typename... Property>
    factory type(const std::size_t identifier, Property &&... property) noexcept {
        assert(!internal::type_info<Type>::type);
        auto *node = internal::type_info<Type>::resolve();
        node->identifier = identifier;
        node->next = internal::type_info<>::type;
        node->prop = properties<Type>(std::forward<Property>(property)...);
        assert(!duplicate(node->identifier, node->next));
        internal::type_info<Type>::type = node;
        internal::type_info<>::type = node;

        return *this;
    }

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
            &internal::type_info<Type>::template base<Base>,
            type,
            nullptr,
            &internal::type_info<Base>::resolve,
            [](void *instance) noexcept -> void * {
                return static_cast<Base *>(static_cast<Type *>(instance));
            },
            []() noexcept -> meta::base {
                return &node;
            }
        };

        node.next = type->base;
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
        static_assert(std::is_convertible_v<Type, To>);
        auto * const type = internal::type_info<Type>::resolve();

        static internal::conv_node node{
            &internal::type_info<Type>::template conv<To>,
            type,
            nullptr,
            &internal::type_info<To>::resolve,
            [](const void *instance) -> any {
                return static_cast<To>(*static_cast<const Type *>(instance));
            },
            []() noexcept -> meta::conv {
                return &node;
            }
        };

        node.next = type->conv;
        assert((!internal::type_info<Type>::template conv<To>));
        internal::type_info<Type>::template conv<To> = &node;
        type->conv = &node;

        return *this;
    }

    /**
     * @brief Assigns a meta conversion function to a meta type.
     *
     * Conversion functions can be either free functions or member
     * functions.<br/>
     * In case of free functions, they must accept a const reference to an
     * instance of the parent type as an argument. In case of member functions,
     * they should have no arguments at all.
     *
     * @tparam Candidate The actual function to use for the conversion.
     * @return A meta factory for the parent type.
     */
    template<auto Candidate>
    factory conv() noexcept {
        using conv_type = std::invoke_result_t<decltype(Candidate), Type &>;
        auto * const type = internal::type_info<Type>::resolve();

        static internal::conv_node node{
            &internal::type_info<Type>::template conv<conv_type>,
            type,
            nullptr,
            &internal::type_info<conv_type>::resolve,
            [](const void *instance) -> any {
                return std::invoke(Candidate, *static_cast<const Type *>(instance));
            },
            []() noexcept -> meta::conv {
                return &node;
            }
        };

        node.next = type->conv;
        assert((!internal::type_info<Type>::template conv<conv_type>));
        internal::type_info<Type>::template conv<conv_type> = &node;
        type->conv = &node;

        return *this;
    }

    /**
     * @brief Assigns a meta constructor to a meta type.
     *
     * Free functions can be assigned to meta types in the role of constructors.
     * All that is required is that they return an instance of the underlying
     * type.<br/>
     * From a client's point of view, nothing changes if a constructor of a meta
     * type is a built-in one or a free function.
     *
     * @tparam Func The actual function to use as a constructor.
     * @tparam Policy Optional policy (no policy set by default).
     * @tparam Property Types of properties to assign to the meta data.
     * @param property Properties to assign to the meta data.
     * @return A meta factory for the parent type.
     */
    template<auto Func, typename Policy = as_is_t, typename... Property>
    factory ctor(Property &&... property) noexcept {
        using helper_type = internal::function_helper_t<decltype(Func)>;
        static_assert(std::is_same_v<typename helper_type::return_type, Type>);
        auto * const type = internal::type_info<Type>::resolve();

        static internal::ctor_node node{
            &internal::type_info<Type>::template ctor<typename helper_type::args_type>,
            type,
            nullptr,
            nullptr,
            helper_type::size,
            &helper_type::arg,
            [](any * const any) {
                return internal::invoke<Type, Func, Policy>({}, any, std::make_index_sequence<helper_type::size>{});
            },
            []() noexcept -> meta::ctor {
                return &node;
            }
        };

        node.next = type->ctor;
        node.prop = properties<typename helper_type::args_type>(std::forward<Property>(property)...);
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
        using helper_type = internal::function_helper_t<Type(*)(Args...)>;
        auto * const type = internal::type_info<Type>::resolve();

        static internal::ctor_node node{
            &internal::type_info<Type>::template ctor<typename helper_type::args_type>,
            type,
            nullptr,
            nullptr,
            helper_type::size,
            &helper_type::arg,
            [](any * const any) {
                return internal::construct<Type, std::remove_cv_t<std::remove_reference_t<Args>>...>(any, std::make_index_sequence<helper_type::size>{});
            },
            []() noexcept -> meta::ctor {
                return &node;
            }
        };

        node.next = type->ctor;
        node.prop = properties<typename helper_type::args_type>(std::forward<Property>(property)...);
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
     * The purpose is to give users the ability to free up resources that
     * require special treatment before an object is actually destroyed.
     *
     * @tparam Func The actual function to use as a destructor.
     * @return A meta factory for the parent type.
     */
    template<auto Func>
    factory dtor() noexcept {
        static_assert(std::is_invocable_v<decltype(Func), Type &>);
        auto * const type = internal::type_info<Type>::resolve();

        static internal::dtor_node node{
            &internal::type_info<Type>::template dtor<Func>,
            type,
            [](handle handle) {
                const auto valid = (handle.type() == internal::type_info<Type>::resolve()->clazz());

                if(valid) {
                    std::invoke(Func, *any{handle}.try_cast<Type>());
                }

                return valid;
            },
            []() noexcept -> meta::dtor {
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
     * @tparam Policy Optional policy (no policy set by default).
     * @tparam Property Types of properties to assign to the meta data.
     * @param identifier Unique identifier.
     * @param property Properties to assign to the meta data.
     * @return A meta factory for the parent type.
     */
    template<auto Data, typename Policy = as_is_t, typename... Property>
    factory data(const std::size_t identifier, Property &&... property) noexcept {
        auto * const type = internal::type_info<Type>::resolve();
        internal::data_node *curr = nullptr;

        if constexpr(std::is_same_v<Type, decltype(Data)>) {
            static_assert(std::is_same_v<Policy, as_is_t>);

            static internal::data_node node{
                &internal::type_info<Type>::template data<Data>,
                {},
                type,
                nullptr,
                nullptr,
                true,
                true,
                &internal::type_info<Type>::resolve,
                [](handle, any, any) { return false; },
                [](handle, any) -> any { return Data; },
                []() noexcept -> meta::data {
                    return &node;
                }
            };

            node.prop = properties<std::integral_constant<Type, Data>>(std::forward<Property>(property)...);
            curr = &node;
        } else if constexpr(std::is_member_object_pointer_v<decltype(Data)>) {
            using data_type = std::remove_reference_t<decltype(std::declval<Type>().*Data)>;

            static internal::data_node node{
                &internal::type_info<Type>::template data<Data>,
                {},
                type,
                nullptr,
                nullptr,
                std::is_const_v<data_type>,
                !std::is_member_object_pointer_v<decltype(Data)>,
                &internal::type_info<data_type>::resolve,
                &internal::setter<std::is_const_v<data_type>, Type, Data>,
                &internal::getter<Type, Data, Policy>,
                []() noexcept -> meta::data {
                    return &node;
                }
            };

            node.prop = properties<std::integral_constant<decltype(Data), Data>>(std::forward<Property>(property)...);
            curr = &node;
        } else {
            static_assert(std::is_pointer_v<std::decay_t<decltype(Data)>>);
            using data_type = std::remove_pointer_t<std::decay_t<decltype(Data)>>;

            static internal::data_node node{
                &internal::type_info<Type>::template data<Data>,
                {},
                type,
                nullptr,
                nullptr,
                std::is_const_v<data_type>,
                !std::is_member_object_pointer_v<decltype(Data)>,
                &internal::type_info<data_type>::resolve,
                &internal::setter<std::is_const_v<data_type>, Type, Data>,
                &internal::getter<Type, Data, Policy>,
                []() noexcept -> meta::data {
                    return &node;
                }
            };

            node.prop = properties<std::integral_constant<decltype(Data), Data>>(std::forward<Property>(property)...);
            curr = &node;
        }

        curr->identifier = identifier;
        curr->next = type->data;
        assert(!duplicate(curr->identifier, curr->next));
        assert((!internal::type_info<Type>::template data<Data>));
        internal::type_info<Type>::template data<Data> = curr;
        type->data = curr;

        return *this;
    }

    /**
     * @brief Assigns a meta data to a meta type by means of its setter and
     * getter.
     *
     * Setters and getters can be either free functions, member functions or a
     * mix of them.<br/>
     * In case of free functions, setters and getters must accept a reference to
     * an instance of the parent type as their first argument. A setter has then
     * an extra argument of a type convertible to that of the parameter to
     * set.<br/>
     * In case of member functions, getters have no arguments at all, while
     * setters has an argument of a type convertible to that of the parameter to
     * set.
     *
     * @tparam Setter The actual function to use as a setter.
     * @tparam Getter The actual function to use as a getter.
     * @tparam Policy Optional policy (no policy set by default).
     * @tparam Property Types of properties to assign to the meta data.
     * @param identifier Unique identifier.
     * @param property Properties to assign to the meta data.
     * @return A meta factory for the parent type.
     */
    template<auto Setter, auto Getter, typename Policy = as_is_t, typename... Property>
    factory data(const std::size_t identifier, Property &&... property) noexcept {
        using owner_type = std::tuple<std::integral_constant<decltype(Setter), Setter>, std::integral_constant<decltype(Getter), Getter>>;
        using underlying_type = std::invoke_result_t<decltype(Getter), Type &>;
        static_assert(std::is_invocable_v<decltype(Setter), Type &, underlying_type>);
        auto * const type = internal::type_info<Type>::resolve();

        static internal::data_node node{
            &internal::type_info<Type>::template data<Setter, Getter>,
            {},
            type,
            nullptr,
            nullptr,
            false,
            false,
            &internal::type_info<underlying_type>::resolve,
            &internal::setter<false, Type, Setter>,
            &internal::getter<Type, Getter, Policy>,
            []() noexcept -> meta::data {
                return &node;
            }
        };

        node.identifier = identifier;
        node.next = type->data;
        node.prop = properties<owner_type>(std::forward<Property>(property)...);
        assert(!duplicate(node.identifier, node.next));
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
     * @tparam Candidate The actual function to attach to the meta type.
     * @tparam Policy Optional policy (no policy set by default).
     * @tparam Property Types of properties to assign to the meta function.
     * @param identifier Unique identifier.
     * @param property Properties to assign to the meta function.
     * @return A meta factory for the parent type.
     */
    template<auto Candidate, typename Policy = as_is_t, typename... Property>
    factory func(const std::size_t identifier, Property &&... property) noexcept {
        using owner_type = std::integral_constant<decltype(Candidate), Candidate>;
        using helper_type = internal::function_helper_t<decltype(Candidate)>;
        auto * const type = internal::type_info<Type>::resolve();

        static internal::func_node node{
            &internal::type_info<Type>::template func<Candidate>,
            {},
            type,
            nullptr,
            nullptr,
            helper_type::size,
            helper_type::is_const,
            !std::is_member_function_pointer_v<decltype(Candidate)>,
            &internal::type_info<std::conditional_t<std::is_same_v<Policy, as_void_t>, void, typename helper_type::return_type>>::resolve,
            &helper_type::arg,
            [](handle handle, any *any) {
                return internal::invoke<Type, Candidate, Policy>(handle, any, std::make_index_sequence<helper_type::size>{});
            },
            []() noexcept -> meta::func {
                return &node;
            }
        };

        node.identifier = identifier;
        node.next = type->func;
        node.prop = properties<owner_type>(std::forward<Property>(property)...);
        assert(!duplicate(node.identifier, node.next));
        assert((!internal::type_info<Type>::template func<Candidate>));
        internal::type_info<Type>::template func<Candidate> = &node;
        type->func = &node;

        return *this;
    }

    /**
     * @brief Unregisters a meta type and all its parts.
     *
     * This function unregisters a meta type and all its data members, member
     * functions and properties, as well as its constructors, destructors and
     * conversion functions if any.<br/>
     * Base classes aren't unregistered but the link between the two types is
     * removed.
     *
     * @return True if the meta type exists, false otherwise.
     */
    bool unregister() noexcept {
        const auto registered = internal::type_info<Type>::type;

        if(registered) {
            if(auto *curr = internal::type_info<>::type; curr == internal::type_info<Type>::type) {
                internal::type_info<>::type = internal::type_info<Type>::type->next;
            } else {
                while(curr && curr->next != internal::type_info<Type>::type) {
                    curr = curr->next;
                }

                if(curr) {
                    curr->next = internal::type_info<Type>::type->next;
                }
            }

            unregister_prop(&internal::type_info<Type>::type->prop);
            unregister_all<&internal::type_node::base>(0);
            unregister_all<&internal::type_node::conv>(0);
            unregister_all<&internal::type_node::ctor>(0);
            unregister_all<&internal::type_node::data>(0);
            unregister_all<&internal::type_node::func>(0);
            unregister_dtor();

            internal::type_info<Type>::type->identifier = {};
            internal::type_info<Type>::type->next = nullptr;
            internal::type_info<Type>::type = nullptr;
        }

        return registered;
    }
};


/**
 * @brief Utility function to use for reflection.
 *
 * This is the point from which everything starts.<br/>
 * By invoking this function with a type that is not yet reflected, a meta type
 * is created to which it will be possible to attach data and functions through
 * a dedicated factory.
 *
 * @tparam Type Type to reflect.
 * @tparam Property Types of properties to assign to the reflected type.
 * @param identifier Unique identifier.
 * @param property Properties to assign to the reflected type.
 * @return A meta factory for the given type.
 */
template<typename Type, typename... Property>
inline factory<Type> reflect(const std::size_t identifier, Property &&... property) noexcept {
    return factory<Type>{}.type(identifier, std::forward<Property>(property)...);
}


/**
 * @brief Utility function to use for reflection.
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
 * @brief Utility function to unregister a type.
 *
 * This function unregisters a type and all its data members, member functions
 * and properties, as well as its constructors, destructors and conversion
 * functions if any.<br/>
 * Base classes aren't unregistered but the link between the two types is
 * removed.
 *
 * @tparam Type Type to unregister.
 * @return True if the type to unregister exists, false otherwise.
 */
template<typename Type>
inline bool unregister() noexcept {
    return factory<Type>{}.unregister();
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
 * @brief Returns the meta type associated with a given identifier.
 * @param identifier Unique identifier.
 * @return The meta type associated with the given identifier, if any.
 */
inline type resolve(const std::size_t identifier) noexcept {
    const auto *curr = internal::find_if([identifier](auto *node) {
        return node->identifier == identifier;
    }, internal::type_info<>::type);

    return curr ? curr->clazz() : type{};
}


/**
 * @brief Iterates all the reflected types.
 * @tparam Op Type of the function object to invoke.
 * @param op A valid function object.
 */
template<typename Op>
inline std::enable_if_t<std::is_invocable_v<Op, type>, void>
resolve(Op op) noexcept {
    internal::iterate([op = std::move(op)](auto *node) {
        op(node->clazz());
    }, internal::type_info<>::type);
}


}


#endif // META_FACTORY_HPP
