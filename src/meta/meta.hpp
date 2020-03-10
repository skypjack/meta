#ifndef META_META_HPP
#define META_META_HPP


#include <array>
#include <memory>
#include <cstring>
#include <cstddef>
#include <utility>
#include <type_traits>
#include <cassert>


namespace meta {


class any;
class handle;
class prop;
class base;
class conv;
class ctor;
class dtor;
class data;
class func;
class type;


/**
 * @cond TURN_OFF_DOXYGEN
 * Internal details not to be documented.
 */


namespace internal {


struct type_node;


struct prop_node {
    prop_node * next;
    any(* const key)();
    any(* const value)();
    prop(* const clazz)() noexcept;
};


struct base_node {
    base_node ** const underlying;
    type_node * const parent;
    base_node * next;
    type_node *(* const ref)() noexcept;
    void *(* const cast)(void *) noexcept;
    base(* const clazz)() noexcept;
};


struct conv_node {
    conv_node ** const underlying;
    type_node * const parent;
    conv_node * next;
    type_node *(* const ref)() noexcept;
    any(* const convert)(const void *);
    conv(* const clazz)() noexcept;
};


struct ctor_node {
    using size_type = std::size_t;
    ctor_node ** const underlying;
    type_node * const parent;
    ctor_node * next;
    prop_node * prop;
    const size_type size;
    type_node *(* const arg)(size_type) noexcept;
    any(* const invoke)(any * const);
    ctor(* const clazz)() noexcept;
};


struct dtor_node {
    dtor_node ** const underlying;
    type_node * const parent;
    bool(* const invoke)(handle);
    dtor(* const clazz)() noexcept;
};


struct data_node {
    data_node ** const underlying;
    std::size_t identifier;
    type_node * const parent;
    data_node * next;
    prop_node * prop;
    const bool is_const;
    const bool is_static;
    type_node *(* const ref)() noexcept;
    bool(* const set)(handle, any, any);
    any(* const get)(handle, any);
    data(* const clazz)() noexcept;
};


struct func_node {
    using size_type = std::size_t;
    func_node ** const underlying;
    std::size_t identifier;
    type_node * const parent;
    func_node * next;
    prop_node * prop;
    const size_type size;
    const bool is_const;
    const bool is_static;
    type_node *(* const ret)() noexcept;
    type_node *(* const arg)(size_type) noexcept;
    any(* const invoke)(handle, any *);
    func(* const clazz)() noexcept;
};


struct type_node {
    using size_type = std::size_t;
    std::size_t identifier;
    type_node * next;
    prop_node * prop;
    const bool is_void;
    const bool is_integral;
    const bool is_floating_point;
    const bool is_array;
    const bool is_enum;
    const bool is_union;
    const bool is_class;
    const bool is_pointer;
    const bool is_function_pointer;
    const bool is_member_object_pointer;
    const bool is_member_function_pointer;
    const size_type extent;
    bool(* const compare)(const void *, const void *);
    type(* const remove_pointer)() noexcept;
    type(* const clazz)() noexcept;
    base_node *base{nullptr};
    conv_node *conv{nullptr};
    ctor_node *ctor{nullptr};
    dtor_node *dtor{nullptr};
    data_node *data{nullptr};
    func_node *func{nullptr};
};


template<typename...>
struct info_node {
    inline static type_node *type = nullptr;
};


template<typename Type>
struct info_node<Type> {
    inline static type_node *type = nullptr;

    template<typename>
    inline static base_node *base = nullptr;

    template<typename>
    inline static conv_node *conv = nullptr;

    template<typename>
    inline static ctor_node *ctor = nullptr;

    template<auto>
    inline static dtor_node *dtor = nullptr;

    template<auto...>
    inline static data_node *data = nullptr;

    template<auto>
    inline static func_node *func = nullptr;

    inline static type_node * resolve() noexcept;
};


template<typename... Type>
struct type_info: info_node<std::remove_cv_t<std::remove_reference_t<Type>>...> {};


template<typename Op, typename Node>
void iterate(Op op, const Node *curr) noexcept {
    while(curr) {
        op(curr);
        curr = curr->next;
    }
}


template<auto Member, typename Op>
void iterate(Op op, const type_node *node) noexcept {
    if(node) {
        auto *curr = node->base;
        iterate(op, node->*Member);

        while(curr) {
            iterate<Member>(op, curr->ref());
            curr = curr->next;
        }
    }
}


template<typename Op, typename Node>
auto find_if(Op op, const Node *curr) noexcept {
    while(curr && !op(curr)) {
        curr = curr->next;
    }

    return curr;
}


template<auto Member, typename Op>
auto find_if(Op op, const type_node *node) noexcept
-> decltype(find_if(op, node->*Member)) {
    decltype(find_if(op, node->*Member)) ret = nullptr;

    if(node) {
        ret = find_if(op, node->*Member);
        auto *curr = node->base;

        while(curr && !ret) {
            ret = find_if<Member>(op, curr->ref());
            curr = curr->next;
        }
    }

    return ret;
}


template<typename Type>
const Type * try_cast(const type_node *node, void *instance) noexcept {
    const auto *type = type_info<Type>::resolve();
    void *ret = nullptr;

    if(node == type) {
        ret = instance;
    } else {
        const auto *base = find_if<&type_node::base>([type](auto *candidate) {
            return candidate->ref() == type;
        }, node);

        ret = base ? base->cast(instance) : nullptr;
    }

    return static_cast<const Type *>(ret);
}


template<auto Member>
inline bool can_cast_or_convert(const type_node *from, const type_node *to) noexcept {
    return (from == to) || find_if<Member>([to](auto *node) {
        return node->ref() == to;
    }, from);
}


template<typename... Args, std::size_t... Indexes>
inline auto ctor(std::index_sequence<Indexes...>, const type_node *node) noexcept {
    return internal::find_if([](auto *candidate) {
        return candidate->size == sizeof...(Args) &&
                (([](auto *from, auto *to) {
                    return internal::can_cast_or_convert<&internal::type_node::base>(from, to)
                            || internal::can_cast_or_convert<&internal::type_node::conv>(from, to);
                }(internal::type_info<Args>::resolve(), candidate->arg(Indexes))) && ...);
    }, node->ctor);
}


}


/**
 * Internal details not to be documented.
 * @endcond TURN_OFF_DOXYGEN
 */


/**
 * @brief Meta any object.
 *
 * A meta any is an opaque container for single values of any type.
 *
 * This class uses a technique called small buffer optimization (SBO) to
 * completely eliminate the need to allocate memory, where possible.<br/>
 * From the user's point of view, nothing will change, but the elimination of
 * allocations will reduce the jumps in memory and therefore will avoid chasing
 * of pointers. This will greatly improve the use of the cache, thus increasing
 * the overall performance.
 */
class any {
    /*! @brief A meta handle is allowed to _inherit_ from a meta any. */
    friend class handle;

    using storage_type = std::aligned_storage_t<sizeof(void *), alignof(void *)>;
    using copy_fn_type = void *(storage_type &, const void *);
    using destroy_fn_type = void(void *);
    using steal_fn_type = void *(storage_type &, void *, destroy_fn_type *);

    template<typename Type, typename = std::void_t<>>
    struct type_traits {
        template<typename... Args>
        static void * instance(storage_type &storage, Args &&... args) {
            auto instance = std::make_unique<Type>(std::forward<Args>(args)...);
            new (&storage) Type *{instance.get()};
            return instance.release();
        }

        static void destroy(void *instance) {
            auto *node = internal::type_info<Type>::resolve();
            auto *actual = static_cast<Type *>(instance);
            [[maybe_unused]] const bool destroyed = node->clazz().destroy(*actual);
            assert(destroyed);
            delete actual;
        }

        static void * copy(storage_type &storage, const void *other) {
            auto instance = std::make_unique<Type>(*static_cast<const Type *>(other));
            new (&storage) Type *{instance.get()};
            return instance.release();
        }

        static void * steal(storage_type &to, void *from, destroy_fn_type *) {
            auto *instance = static_cast<Type *>(from);
            new (&to) Type *{instance};
            return instance;
        }
    };

    template<typename Type>
    struct type_traits<Type, std::enable_if_t<sizeof(Type) <= sizeof(void *) && std::is_nothrow_move_constructible_v<Type>>> {
        template<typename... Args>
        static void * instance(storage_type &storage, Args &&... args) {
            return new (&storage) Type{std::forward<Args>(args)...};
        }

        static void destroy(void *instance) {
            auto *node = internal::type_info<Type>::resolve();
            auto *actual = static_cast<Type *>(instance);
            [[maybe_unused]] const bool destroyed = node->clazz().destroy(*actual);
            assert(destroyed);
            actual->~Type();
        }

        static void * copy(storage_type &storage, const void *instance) {
            return new (&storage) Type{*static_cast<const Type *>(instance)};
        }

        static void * steal(storage_type &to, void *from, destroy_fn_type *destroy_fn) {
            void *instance = new (&to) Type{std::move(*static_cast<Type *>(from))};
            destroy_fn(from);
            return instance;
        }
    };

public:
    /*! @brief Default constructor. */
    any() noexcept
        : storage{},
          instance{nullptr},
          node{nullptr},
          destroy_fn{nullptr},
          copy_fn{nullptr},
          steal_fn{nullptr}
    {}

    /**
     * @brief Constructs a meta any by directly initializing the new object.
     * @tparam Type Type of object to use to initialize the container.
     * @tparam Args Types of arguments to use to construct the new instance.
     * @param args Parameters to use to construct the instance.
     */
    template<typename Type, typename... Args>
    explicit any(std::in_place_type_t<Type>, [[maybe_unused]] Args &&... args)
        : any{}
    {
        node = internal::type_info<Type>::resolve();

        if constexpr(!std::is_void_v<Type>) {
            using traits_type = type_traits<std::remove_cv_t<std::remove_reference_t<Type>>>;
            instance = traits_type::instance(storage, std::forward<Args>(args)...);
            destroy_fn = &traits_type::destroy;
            copy_fn = &traits_type::copy;
            steal_fn = &traits_type::steal;
        }
    }

    /**
     * @brief Constructs a meta any that holds an unmanaged object.
     * @tparam Type Type of object to use to initialize the container.
     * @param type An instance of an object to use to initialize the container.
     */
    template<typename Type>
    explicit any(std::reference_wrapper<Type> type)
        : any{}
    {
        node = internal::type_info<Type>::resolve();
        instance = &type.get();
    }

    /**
     * @brief Constructs a meta any from a meta handle object.
     * @param handle A reference to an object to use to initialize the meta any.
     */
    inline any(handle handle) noexcept;

    /**
     * @brief Constructs a meta any from a given value.
     * @tparam Type Type of object to use to initialize the container.
     * @param type An instance of an object to use to initialize the container.
     */
    template<typename Type, typename = std::enable_if_t<!std::is_same_v<std::remove_cv_t<std::remove_reference_t<Type>>, any>>>
    any(Type &&type)
        : any{std::in_place_type<std::remove_cv_t<std::remove_reference_t<Type>>>, std::forward<Type>(type)}
    {}

    /**
     * @brief Copy constructor.
     * @param other The instance to copy from.
     */
    any(const any &other)
        : any{}
    {
        node = other.node;
        instance = other.copy_fn ? other.copy_fn(storage, other.instance) : other.instance;
        destroy_fn = other.destroy_fn;
        copy_fn = other.copy_fn;
        steal_fn = other.steal_fn;
    }

    /**
     * @brief Move constructor.
     *
     * After meta any move construction, instances that have been moved from
     * are placed in a valid but unspecified state. It's highly discouraged to
     * continue using them.
     *
     * @param other The instance to move from.
     */
    any(any &&other) noexcept
        : any{}
    {
        swap(*this, other);
    }

    /*! @brief Frees the internal storage, whatever it means. */
    ~any() {
        if(destroy_fn) {
            destroy_fn(instance);
        }
    }

    /**
     * @brief Assignment operator.
     * @tparam Type Type of object to use to initialize the container.
     * @param type An instance of an object to use to initialize the container.
     * @return This meta any object.
     */
    template<typename Type, typename = std::enable_if_t<!std::is_same_v<std::remove_cv_t<std::remove_reference_t<Type>>, any>>>
    any & operator=(Type &&type) {
        return (*this = any{std::forward<Type>(type)});
    }

    /**
     * @brief Copy assignment operator.
     * @param other The instance to assign.
     * @return This meta any object.
     */
    any & operator=(const any &other) {
        return (*this = any{other});
    }

    /**
     * @brief Move assignment operator.
     * @param other The instance to assign.
     * @return This meta any object.
     */
    any & operator=(any &&other) noexcept {
        any any{std::move(other)};
        swap(any, *this);
        return *this;
    }

    /**
     * @brief Returns the meta type of the underlying object.
     * @return The meta type of the underlying object, if any.
     */
    inline meta::type type() const noexcept;

    /**
     * @brief Returns an opaque pointer to the contained instance.
     * @return An opaque pointer the contained instance, if any.
     */
    const void * data() const noexcept {
        return instance;
    }

    /*! @copydoc data */
    void * data() noexcept {
        return const_cast<void *>(std::as_const(*this).data());
    }

    /**
     * @brief Tries to cast an instance to a given type.
     * @tparam Type Type to which to cast the instance.
     * @return A (possibly null) pointer to the contained instance.
     */
    template<typename Type>
    const Type * try_cast() const noexcept {
        return internal::try_cast<Type>(node, instance);
    }

    /*! @copydoc try_cast */
    template<typename Type>
    Type * try_cast() noexcept {
        return const_cast<Type *>(std::as_const(*this).try_cast<Type>());
    }

    /**
     * @brief Tries to cast an instance to a given type.
     *
     * The type of the instance must be such that the cast is possible.
     *
     * @warning
     * Attempting to perform a cast that isn't viable results in undefined
     * behavior.<br/>
     * An assertion will abort the execution at runtime in debug mode in case
     * the cast is not feasible.
     *
     * @tparam Type Type to which to cast the instance.
     * @return A reference to the contained instance.
     */
    template<typename Type>
    const Type & cast() const noexcept {
        auto *actual = try_cast<Type>();
        assert(actual);
        return *actual;
    }

    /*! @copydoc cast */
    template<typename Type>
    Type & cast() noexcept {
        return const_cast<Type &>(std::as_const(*this).cast<Type>());
    }

    /**
     * @brief Tries to convert an instance to a given type and returns it.
     * @tparam Type Type to which to convert the instance.
     * @return A valid meta any object if the conversion is possible, an invalid
     * one otherwise.
     */
    template<typename Type>
    any convert() const {
        any any{};

        if(const auto *type = internal::type_info<Type>::resolve(); node == type) {
            any = *static_cast<const Type *>(instance);
        } else {
            const auto *conv = internal::find_if<&internal::type_node::conv>([type](auto *other) {
                return other->ref() == type;
            }, node);

            if(conv) {
                any = conv->convert(instance);
            }
        }

        return any;
    }

    /**
     * @brief Tries to convert an instance to a given type.
     * @tparam Type Type to which to convert the instance.
     * @return True if the conversion is possible, false otherwise.
     */
    template<typename Type>
    bool convert() {
        bool valid = (node == internal::type_info<Type>::resolve());

        if(!valid) {
            if(auto any = std::as_const(*this).convert<Type>(); any) {
                swap(any, *this);
                valid = true;
            }
        }

        return valid;
    }

    /**
     * @brief Replaces the contained object by initializing a new instance
     * directly.
     * @tparam Type Type of object to use to initialize the container.
     * @tparam Args Types of arguments to use to construct the new instance.
     * @param args Parameters to use to construct the instance.
     */
    template<typename Type, typename... Args>
    void emplace(Args&& ... args) {
        *this = any{std::in_place_type_t<Type>{}, std::forward<Args>(args)...};
    }

    /**
     * @brief Returns false if a container is empty, true otherwise.
     * @return False if the container is empty, true otherwise.
     */
    explicit operator bool() const noexcept {
        return node;
    }

    /**
     * @brief Checks if two containers differ in their content.
     * @param other Container with which to compare.
     * @return False if the two containers differ in their content, true
     * otherwise.
     */
    bool operator==(const any &other) const noexcept {
        return node == other.node && (!node || node->compare(instance, other.instance));
    }

    /**
     * @brief Swaps two meta any objects.
     * @param lhs A valid meta any object.
     * @param rhs A valid meta any object.
     */
    friend void swap(any &lhs, any &rhs) noexcept {
        if(lhs.steal_fn && rhs.steal_fn) {
            storage_type buffer;
            auto *temp = lhs.steal_fn(buffer, lhs.instance, lhs.destroy_fn);
            lhs.instance = rhs.steal_fn(lhs.storage, rhs.instance, rhs.destroy_fn);
            rhs.instance = lhs.steal_fn(rhs.storage, temp, lhs.destroy_fn);
        } else if(lhs.steal_fn) {
            lhs.instance = lhs.steal_fn(rhs.storage, lhs.instance, lhs.destroy_fn);
            std::swap(rhs.instance, lhs.instance);
        } else if(rhs.steal_fn) {
            rhs.instance = rhs.steal_fn(lhs.storage, rhs.instance, rhs.destroy_fn);
            std::swap(rhs.instance, lhs.instance);
        } else {
            std::swap(lhs.instance, rhs.instance);
        }

        std::swap(lhs.node, rhs.node);
        std::swap(lhs.destroy_fn, rhs.destroy_fn);
        std::swap(lhs.copy_fn, rhs.copy_fn);
        std::swap(lhs.steal_fn, rhs.steal_fn);
    }

private:
    storage_type storage;
    void *instance;
    const internal::type_node *node;
    destroy_fn_type *destroy_fn;
    copy_fn_type *copy_fn;
    steal_fn_type *steal_fn;
};


/**
 * @brief Meta handle object.
 *
 * A meta handle is an opaque pointer to an instance of any type.
 *
 * A handle doesn't perform copies and isn't responsible for the contained
 * object. It doesn't prolong the lifetime of the pointed instance. Users are
 * responsible for ensuring that the target object remains alive for the entire
 * interval of use of the handle.
 */
class handle {
    /*! @brief A meta any is allowed to _inherit_ from a meta handle. */
    friend class any;

public:
    /*! @brief Default constructor. */
    handle() noexcept
        : node{nullptr},
          instance{nullptr}
    {}

    /**
     * @brief Constructs a meta handle from a meta any object.
     * @param any A reference to an object to use to initialize the handle.
     */
    handle(any &any) noexcept
        : node{any.node},
          instance{any.instance}
    {}

    /**
     * @brief Constructs a meta handle from a given instance.
     * @tparam Type Type of object to use to initialize the handle.
     * @param obj A reference to an object to use to initialize the handle.
     */
    template<typename Type, typename = std::enable_if_t<!std::is_same_v<std::remove_cv_t<std::remove_reference_t<Type>>, handle>>>
    handle(Type &obj) noexcept
        : node{internal::type_info<Type>::resolve()},
          instance{&obj}
    {}

    /**
     * @brief Returns the meta type of the underlying object.
     * @return The meta type of the underlying object, if any.
     */
    inline meta::type type() const noexcept;

    /**
     * @brief Returns an opaque pointer to the contained instance.
     * @return An opaque pointer the contained instance, if any.
     */
    const void * data() const noexcept {
        return instance;
    }

    /*! @copydoc data */
    void * data() noexcept {
        return const_cast<void *>(std::as_const(*this).data());
    }

    /**
     * @brief Returns false if a handle is empty, true otherwise.
     * @return False if the handle is empty, true otherwise.
     */
    explicit operator bool() const noexcept {
        return instance;
    }

private:
    const internal::type_node *node;
    void *instance;
};


/**
 * @brief Checks if two containers differ in their content.
 * @param lhs A meta any object, either empty or not.
 * @param rhs A meta any object, either empty or not.
 * @return True if the two containers differ in their content, false otherwise.
 */
inline bool operator!=(const any &lhs, const any &rhs) noexcept {
    return !(lhs == rhs);
}


/**
 * @brief Meta property object.
 *
 * A meta property is an opaque container for a key/value pair.<br/>
 * Properties are associated with any other meta object to enrich it.
 */
class prop {
    /*! @brief A meta factory is allowed to create meta objects. */
    template<typename> friend class factory;

    prop(const internal::prop_node *curr) noexcept
        : node{curr}
    {}

public:
    /*! @brief Default constructor. */
    prop() noexcept
        : node{nullptr}
    {}

    /**
     * @brief Returns the stored key.
     * @return A meta any containing the key stored with the given property.
     */
    any key() const noexcept {
        return node->key();
    }

    /**
     * @brief Returns the stored value.
     * @return A meta any containing the value stored with the given property.
     */
    any value() const noexcept {
        return node->value();
    }

    /**
     * @brief Returns true if a meta object is valid, false otherwise.
     * @return True if the meta object is valid, false otherwise.
     */
    explicit operator bool() const noexcept {
        return node;
    }

    /**
     * @brief Checks if two meta objects refer to the same node.
     * @param other The meta object with which to compare.
     * @return True if the two meta objects refer to the same node, false
     * otherwise.
     */
    bool operator==(const prop &other) const noexcept {
        return node == other.node;
    }

private:
    const internal::prop_node *node;
};


/**
 * @brief Checks if two meta objects refer to the same node.
 * @param lhs A meta object, either valid or not.
 * @param rhs A meta object, either valid or not.
 * @return True if the two meta objects refer to the same node, false otherwise.
 */
inline bool operator!=(const prop &lhs, const prop &rhs) noexcept {
    return !(lhs == rhs);
}


/**
 * @brief Meta base object.
 *
 * A meta base is an opaque container for a base class to be used to walk
 * through hierarchies.
 */
class base {
    /*! @brief A meta factory is allowed to create meta objects. */
    template<typename> friend class factory;

    base(const internal::base_node *curr) noexcept
        : node{curr}
    {}

public:
    /*! @brief Default constructor. */
    base() noexcept
        : node{nullptr}
    {}

    /**
     * @brief Returns the meta type to which a meta base belongs.
     * @return The meta type to which the meta base belongs.
     */
    inline meta::type parent() const noexcept;

    /**
     * @brief Returns the meta type of a given meta base.
     * @return The meta type of the meta base.
     */
    inline meta::type type() const noexcept;

    /**
     * @brief Casts an instance from a parent type to a base type.
     * @param instance The instance to cast.
     * @return An opaque pointer to the base type.
     */
    void * cast(void *instance) const noexcept {
        return node->cast(instance);
    }

    /**
     * @brief Returns true if a meta object is valid, false otherwise.
     * @return True if the meta object is valid, false otherwise.
     */
    explicit operator bool() const noexcept {
        return node;
    }

    /**
     * @brief Checks if two meta objects refer to the same node.
     * @param other The meta object with which to compare.
     * @return True if the two meta objects refer to the same node, false
     * otherwise.
     */
    bool operator==(const base &other) const noexcept {
        return node == other.node;
    }

private:
    const internal::base_node *node;
};


/**
 * @brief Checks if two meta objects refer to the same node.
 * @param lhs A meta object, either valid or not.
 * @param rhs A meta object, either valid or not.
 * @return True if the two meta objects refer to the same node, false otherwise.
 */
inline bool operator!=(const base &lhs, const base &rhs) noexcept {
    return !(lhs == rhs);
}


/**
 * @brief Meta conversion function object.
 *
 * A meta conversion function is an opaque container for a conversion function
 * to be used to convert a given instance to another type.
 */
class conv {
    /*! @brief A meta factory is allowed to create meta objects. */
    template<typename> friend class factory;

    conv(const internal::conv_node *curr) noexcept
        : node{curr}
    {}

public:
    /*! @brief Default constructor. */
    conv() noexcept
        : node{nullptr}
    {}

    /**
     * @brief Returns the meta type to which a meta conversion function belongs.
     * @return The meta type to which the meta conversion function belongs.
     */
    inline meta::type parent() const noexcept;

    /**
     * @brief Returns the meta type of a given meta conversion function.
     * @return The meta type of the meta conversion function.
     */
    inline meta::type type() const noexcept;

    /**
     * @brief Converts an instance to a given type.
     * @param instance The instance to convert.
     * @return An opaque pointer to the instance to convert.
     */
    any convert(const void *instance) const noexcept {
        return node->convert(instance);
    }

    /**
     * @brief Returns true if a meta object is valid, false otherwise.
     * @return True if the meta object is valid, false otherwise.
     */
    explicit operator bool() const noexcept {
        return node;
    }

    /**
     * @brief Checks if two meta objects refer to the same node.
     * @param other The meta object with which to compare.
     * @return True if the two meta objects refer to the same node, false
     * otherwise.
     */
    bool operator==(const conv &other) const noexcept {
        return node == other.node;
    }

private:
    const internal::conv_node *node;
};


/**
 * @brief Checks if two meta objects refer to the same node.
 * @param lhs A meta object, either valid or not.
 * @param rhs A meta object, either valid or not.
 * @return True if the two meta objects refer to the same node, false otherwise.
 */
inline bool operator!=(const conv &lhs, const conv &rhs) noexcept {
    return !(lhs == rhs);
}


/**
 * @brief Meta constructor object.
 *
 * A meta constructor is an opaque container for a function to be used to
 * construct instances of a given type.
 */
class ctor {
    /*! @brief A meta factory is allowed to create meta objects. */
    template<typename> friend class factory;

    ctor(const internal::ctor_node *curr) noexcept
        : node{curr}
    {}

public:
    /*! @brief Unsigned integer type. */
    using size_type = typename internal::ctor_node::size_type;

    /*! @brief Default constructor. */
    ctor() noexcept
        : node{nullptr}
    {}

    /**
     * @brief Returns the meta type to which a meta constructor belongs.
     * @return The meta type to which the meta constructor belongs.
     */
    inline meta::type parent() const noexcept;

    /**
     * @brief Returns the number of arguments accepted by a meta constructor.
     * @return The number of arguments accepted by the meta constructor.
     */
    size_type size() const noexcept {
        return node->size;
    }

    /**
     * @brief Returns the meta type of the i-th argument of a meta constructor.
     * @param index The index of the argument of which to return the meta type.
     * @return The meta type of the i-th argument of a meta constructor, if any.
     */
    meta::type arg(size_type index) const noexcept;

    /**
     * @brief Creates an instance of the underlying type, if possible.
     *
     * To create a valid instance, the types of the parameters must coincide
     * exactly with those required by the underlying meta constructor.
     * Otherwise, an empty and then invalid container is returned.
     *
     * @tparam Args Types of arguments to use to construct the instance.
     * @param args Parameters to use to construct the instance.
     * @return A meta any containing the new instance, if any.
     */
    template<typename... Args>
    any invoke(Args &&... args) const {
        std::array<any, sizeof...(Args)> arguments{{std::forward<Args>(args)...}};
        any any{};

        if(sizeof...(Args) == size()) {
            any = node->invoke(arguments.data());
        }

        return any;
    }

    /**
     * @brief Iterates all the properties assigned to a meta constructor.
     * @tparam Op Type of the function object to invoke.
     * @param op A valid function object.
     */
    template<typename Op>
    std::enable_if_t<std::is_invocable_v<Op, meta::prop>, void>
    prop(Op op) const noexcept {
        internal::iterate([op = std::move(op)](auto *curr) {
            op(curr->clazz());
        }, node->prop);
    }

    /**
     * @brief Returns the property associated with a given key.
     * @tparam Key Type of key to use to search for a property.
     * @param key The key to use to search for a property.
     * @return The property associated with the given key, if any.
     */
    template<typename Key>
    std::enable_if_t<!std::is_invocable_v<Key, meta::prop>, meta::prop>
    prop(Key &&key) const noexcept {
        const auto *curr = internal::find_if([key = any{std::forward<Key>(key)}](auto *candidate) {
            return candidate->key() == key;
        }, node->prop);

        return curr ? curr->clazz() : meta::prop{};
    }

    /**
     * @brief Returns true if a meta object is valid, false otherwise.
     * @return True if the meta object is valid, false otherwise.
     */
    explicit operator bool() const noexcept {
        return node;
    }

    /**
     * @brief Checks if two meta objects refer to the same node.
     * @param other The meta object with which to compare.
     * @return True if the two meta objects refer to the same node, false
     * otherwise.
     */
    bool operator==(const ctor &other) const noexcept {
        return node == other.node;
    }

private:
    const internal::ctor_node *node;
};


/**
 * @brief Checks if two meta objects refer to the same node.
 * @param lhs A meta object, either valid or not.
 * @param rhs A meta object, either valid or not.
 * @return True if the two meta objects refer to the same node, false otherwise.
 */
inline bool operator!=(const ctor &lhs, const ctor &rhs) noexcept {
    return !(lhs == rhs);
}


/**
 * @brief Meta destructor object.
 *
 * A meta destructor is an opaque container for a function to be used to
 * destroy instances of a given type.
 */
class dtor {
    /*! @brief A meta factory is allowed to create meta objects. */
    template<typename> friend class factory;

    dtor(const internal::dtor_node *curr) noexcept
        : node{curr}
    {}

public:
    /*! @brief Default constructor. */
    dtor() noexcept
        : node{nullptr}
    {}

    /**
     * @brief Returns the meta type to which a meta destructor belongs.
     * @return The meta type to which the meta destructor belongs.
     */
    inline meta::type parent() const noexcept;

    /**
     * @brief Destroys an instance of the underlying type.
     *
     * It must be possible to cast the instance to the parent type of the meta
     * destructor. Otherwise, invoking the meta destructor results in an
     * undefined behavior.
     *
     * @param handle An opaque pointer to an instance of the underlying type.
     * @return True in case of success, false otherwise.
     */
    bool invoke(handle handle) const {
        return node->invoke(handle);
    }

    /**
     * @brief Returns true if a meta object is valid, false otherwise.
     * @return True if the meta object is valid, false otherwise.
     */
    explicit operator bool() const noexcept {
        return node;
    }

    /**
     * @brief Checks if two meta objects refer to the same node.
     * @param other The meta object with which to compare.
     * @return True if the two meta objects refer to the same node, false
     * otherwise.
     */
    bool operator==(const dtor &other) const noexcept {
        return node == other.node;
    }

private:
    const internal::dtor_node *node;
};


/**
 * @brief Checks if two meta objects refer to the same node.
 * @param lhs A meta object, either valid or not.
 * @param rhs A meta object, either valid or not.
 * @return True if the two meta objects refer to the same node, false otherwise.
 */
inline bool operator!=(const dtor &lhs, const dtor &rhs) noexcept {
    return !(lhs == rhs);
}


/**
 * @brief Meta data object.
 *
 * A meta data is an opaque container for a data member associated with a given
 * type.
 */
class data {
    /*! @brief A meta factory is allowed to create meta objects. */
    template<typename> friend class factory;

    data(const internal::data_node *curr) noexcept
        : node{curr}
    {}

public:
    /*! @brief Default constructor. */
    data() noexcept
        : node{nullptr}
    {}

    /**
     * @brief Returns the meta type to which a meta data belongs.
     * @return The meta type to which the meta data belongs.
     */
    inline meta::type parent() const noexcept;

    /**
     * @brief Indicates whether a given meta data is constant or not.
     * @return True if the meta data is constant, false otherwise.
     */
    bool is_const() const noexcept {
        return node->is_const;
    }

    /**
     * @brief Indicates whether a given meta data is static or not.
     *
     * A static meta data is such that it can be accessed using a null pointer
     * as an instance.
     *
     * @return True if the meta data is static, false otherwise.
     */
    bool is_static() const noexcept {
        return node->is_static;
    }

    /**
     * @brief Returns the meta type of a given meta data.
     * @return The meta type of the meta data.
     */
    inline meta::type type() const noexcept;

    /**
     * @brief Sets the value of the variable enclosed by a given meta type.
     *
     * It must be possible to cast the instance to the parent type of the meta
     * data. Otherwise, invoking the setter results in an undefined
     * behavior.<br/>
     * The type of the value must coincide exactly with that of the variable
     * enclosed by the meta data. Otherwise, invoking the setter does nothing.
     *
     * @tparam Type Type of value to assign.
     * @param handle An opaque pointer to an instance of the underlying type.
     * @param value Parameter to use to set the underlying variable.
     * @return True in case of success, false otherwise.
     */
    template<typename Type>
    bool set(handle handle, Type &&value) const {
        return node->set(handle, any{}, std::forward<Type>(value));
    }

    /**
     * @brief Sets the i-th element of an array enclosed by a given meta type.
     *
     * It must be possible to cast the instance to the parent type of the meta
     * data. Otherwise, invoking the setter results in an undefined
     * behavior.<br/>
     * The type of the value must coincide exactly with that of the array type
     * enclosed by the meta data. Otherwise, invoking the setter does nothing.
     *
     * @tparam Type Type of value to assign.
     * @param handle An opaque pointer to an instance of the underlying type.
     * @param index Position of the underlying element to set.
     * @param value Parameter to use to set the underlying element.
     * @return True in case of success, false otherwise.
     */
    template<typename Type>
    bool set(handle handle, std::size_t index, Type &&value) const {
        assert(index < node->ref()->extent);
        return node->set(handle, index, std::forward<Type>(value));
    }

    /**
     * @brief Gets the value of the variable enclosed by a given meta type.
     *
     * It must be possible to cast the instance to the parent type of the meta
     * data. Otherwise, invoking the getter results in an undefined behavior.
     *
     * @param handle An opaque pointer to an instance of the underlying type.
     * @return A meta any containing the value of the underlying variable.
     */
    any get(handle handle) const noexcept {
        return node->get(handle, any{});
    }

    /**
     * @brief Gets the i-th element of an array enclosed by a given meta type.
     *
     * It must be possible to cast the instance to the parent type of the meta
     * data. Otherwise, invoking the getter results in an undefined behavior.
     *
     * @param handle An opaque pointer to an instance of the underlying type.
     * @param index Position of the underlying element to get.
     * @return A meta any containing the value of the underlying element.
     */
    any get(handle handle, std::size_t index) const noexcept {
        assert(index < node->ref()->extent);
        return node->get(handle, index);
    }

    /**
     * @brief Iterates all the properties assigned to a meta data.
     * @tparam Op Type of the function object to invoke.
     * @param op A valid function object.
     */
    template<typename Op>
    std::enable_if_t<std::is_invocable_v<Op, meta::prop>, void>
    prop(Op op) const noexcept {
        internal::iterate([op = std::move(op)](auto *curr) {
            op(curr->clazz());
        }, node->prop);
    }

    /**
     * @brief Returns the property associated with a given key.
     * @tparam Key Type of key to use to search for a property.
     * @param key The key to use to search for a property.
     * @return The property associated with the given key, if any.
     */
    template<typename Key>
    std::enable_if_t<!std::is_invocable_v<Key, meta::prop>, meta::prop>
    prop(Key &&key) const noexcept {
        const auto *curr = internal::find_if([key = any{std::forward<Key>(key)}](auto *candidate) {
            return candidate->key() == key;
        }, node->prop);

        return curr ? curr->clazz() : meta::prop{};
    }

    /**
     * @brief Returns true if a meta object is valid, false otherwise.
     * @return True if the meta object is valid, false otherwise.
     */
    explicit operator bool() const noexcept {
        return node;
    }

    /**
     * @brief Checks if two meta objects refer to the same node.
     * @param other The meta object with which to compare.
     * @return True if the two meta objects refer to the same node, false
     * otherwise.
     */
    bool operator==(const data &other) const noexcept {
        return node == other.node;
    }

private:
    const internal::data_node *node;
};


/**
 * @brief Checks if two meta objects refer to the same node.
 * @param lhs A meta object, either valid or not.
 * @param rhs A meta object, either valid or not.
 * @return True if the two meta objects refer to the same node, false otherwise.
 */
inline bool operator!=(const data &lhs, const data &rhs) noexcept {
    return !(lhs == rhs);
}


/**
 * @brief Meta function object.
 *
 * A meta function is an opaque container for a member function associated with
 * a given type.
 */
class func {
    /*! @brief A meta factory is allowed to create meta objects. */
    template<typename> friend class factory;

    func(const internal::func_node *curr) noexcept
        : node{curr}
    {}

public:
    /*! @brief Unsigned integer type. */
    using size_type = typename internal::func_node::size_type;

    /*! @brief Default constructor. */
    func() noexcept
        : node{nullptr}
    {}

    /**
     * @brief Returns the meta type to which a meta function belongs.
     * @return The meta type to which the meta function belongs.
     */
    inline meta::type parent() const noexcept;

    /**
     * @brief Returns the number of arguments accepted by a meta function.
     * @return The number of arguments accepted by the meta function.
     */
    size_type size() const noexcept {
        return node->size;
    }

    /**
     * @brief Indicates whether a given meta function is constant or not.
     * @return True if the meta function is constant, false otherwise.
     */
    bool is_const() const noexcept {
        return node->is_const;
    }

    /**
     * @brief Indicates whether a given meta function is static or not.
     *
     * A static meta function is such that it can be invoked using a null
     * pointer as an instance.
     *
     * @return True if the meta function is static, false otherwise.
     */
    bool is_static() const noexcept {
        return node->is_static;
    }

    /**
     * @brief Returns the meta type of the return type of a meta function.
     * @return The meta type of the return type of the meta function.
     */
    inline meta::type ret() const noexcept;

    /**
     * @brief Returns the meta type of the i-th argument of a meta function.
     * @param index The index of the argument of which to return the meta type.
     * @return The meta type of the i-th argument of a meta function, if any.
     */
    inline meta::type arg(size_type index) const noexcept;

    /**
     * @brief Invokes the underlying function, if possible.
     *
     * To invoke a meta function, the types of the parameters must coincide
     * exactly with those required by the underlying function. Otherwise, an
     * empty and then invalid container is returned.<br/>
     * It must be possible to cast the instance to the parent type of the meta
     * function. Otherwise, invoking the underlying function results in an
     * undefined behavior.
     *
     * @tparam Args Types of arguments to use to invoke the function.
     * @param handle An opaque pointer to an instance of the underlying type.
     * @param args Parameters to use to invoke the function.
     * @return A meta any containing the returned value, if any.
     */
    template<typename... Args>
    any invoke(handle handle, Args &&... args) const {
        // makes aliasing on the values and passes forward references if any
        std::array<any, sizeof...(Args)> arguments{{meta::handle{args}...}};
        any any{};

        if(sizeof...(Args) == size()) {
            any = node->invoke(handle, arguments.data());
        }

        return any;
    }

    /**
     * @brief Iterates all the properties assigned to a meta function.
     * @tparam Op Type of the function object to invoke.
     * @param op A valid function object.
     */
    template<typename Op>
    std::enable_if_t<std::is_invocable_v<Op, meta::prop>, void>
    prop(Op op) const noexcept {
        internal::iterate([op = std::move(op)](auto *curr) {
            op(curr->clazz());
        }, node->prop);
    }

    /**
     * @brief Returns the property associated with a given key.
     * @tparam Key Type of key to use to search for a property.
     * @param key The key to use to search for a property.
     * @return The property associated with the given key, if any.
     */
    template<typename Key>
    std::enable_if_t<!std::is_invocable_v<Key, meta::prop>, meta::prop>
    prop(Key &&key) const noexcept {
        const auto *curr = internal::find_if([key = any{std::forward<Key>(key)}](auto *candidate) {
            return candidate->key() == key;
        }, node->prop);

        return curr ? curr->clazz() : meta::prop{};
    }

    /**
     * @brief Returns true if a meta object is valid, false otherwise.
     * @return True if the meta object is valid, false otherwise.
     */
    explicit operator bool() const noexcept {
        return node;
    }

    /**
     * @brief Checks if two meta objects refer to the same node.
     * @param other The meta object with which to compare.
     * @return True if the two meta objects refer to the same node, false
     * otherwise.
     */
    bool operator==(const func &other) const noexcept {
        return node == other.node;
    }

private:
    const internal::func_node *node;
};


/**
 * @brief Checks if two meta objects refer to the same node.
 * @param lhs A meta object, either valid or not.
 * @param rhs A meta object, either valid or not.
 * @return True if the two meta objects refer to the same node, false otherwise.
 */
inline bool operator!=(const func &lhs, const func &rhs) noexcept {
    return !(lhs == rhs);
}


/**
 * @brief Meta type object.
 *
 * A meta type is the starting point for accessing a reflected type, thus being
 * able to work through it on real objects.
 */
class type {
    /*! @brief A meta node is allowed to create meta objects. */
    template<typename...> friend struct internal::info_node;

    type(const internal::type_node *curr) noexcept
        : node{curr}
    {}

public:
    /*! @brief Unsigned integer type. */
    using size_type = typename internal::type_node::size_type;

    /*! @brief Default constructor. */
    type() noexcept
        : node{nullptr}
    {}

    /**
     * @brief Indicates whether a given meta type refers to void or not.
     * @return True if the underlying type is void, false otherwise.
     */
    bool is_void() const noexcept {
        return node->is_void;
    }

    /**
     * @brief Indicates whether a given meta type refers to an integral type or
     * not.
     * @return True if the underlying type is an integral type, false otherwise.
     */
    bool is_integral() const noexcept {
        return node->is_integral;
    }

    /**
     * @brief Indicates whether a given meta type refers to a floating-point
     * type or not.
     * @return True if the underlying type is a floating-point type, false
     * otherwise.
     */
    bool is_floating_point() const noexcept {
        return node->is_floating_point;
    }

    /**
     * @brief Indicates whether a given meta type refers to an array type or
     * not.
     * @return True if the underlying type is an array type, false otherwise.
     */
    bool is_array() const noexcept {
        return node->is_array;
    }

    /**
     * @brief Indicates whether a given meta type refers to an enum or not.
     * @return True if the underlying type is an enum, false otherwise.
     */
    bool is_enum() const noexcept {
        return node->is_enum;
    }

    /**
     * @brief Indicates whether a given meta type refers to an union or not.
     * @return True if the underlying type is an union, false otherwise.
     */
    bool is_union() const noexcept {
        return node->is_union;
    }

    /**
     * @brief Indicates whether a given meta type refers to a class or not.
     * @return True if the underlying type is a class, false otherwise.
     */
    bool is_class() const noexcept {
        return node->is_class;
    }

    /**
     * @brief Indicates whether a given meta type refers to a pointer or not.
     * @return True if the underlying type is a pointer, false otherwise.
     */
    bool is_pointer() const noexcept {
        return node->is_pointer;
    }

    /**
     * @brief Indicates whether a given meta type refers to a function pointer
     * or not.
     * @return True if the underlying type is a function pointer, false
     * otherwise.
     */
    bool is_function_pointer() const noexcept {
        return node->is_function_pointer;
    }

    /**
     * @brief Indicates whether a given meta type refers to a pointer to data
     * member or not.
     * @return True if the underlying type is a pointer to data member, false
     * otherwise.
     */
    bool is_member_object_pointer() const noexcept {
        return node->is_member_object_pointer;
    }

    /**
     * @brief Indicates whether a given meta type refers to a pointer to member
     * function or not.
     * @return True if the underlying type is a pointer to member function,
     * false otherwise.
     */
    bool is_member_function_pointer() const noexcept {
        return node->is_member_function_pointer;
    }

    /**
     * @brief If a given meta type refers to an array type, provides the number
     * of elements of the array.
     * @return The number of elements of the array if the underlying type is an
     * array type, 0 otherwise.
     */
    size_type extent() const noexcept {
        return node->extent;
    }

    /**
     * @brief Provides the meta type for which the pointer is defined.
     * @return The meta type for which the pointer is defined or this meta type
     * if it doesn't refer to a pointer type.
     */
    meta::type remove_pointer() const noexcept {
        return node->remove_pointer();
    }

    /**
     * @brief Iterates all the meta base of a meta type.
     *
     * Iteratively returns **all** the base classes of the given type.
     *
     * @tparam Op Type of the function object to invoke.
     * @param op A valid function object.
     */
    template<typename Op>
    std::enable_if_t<std::is_invocable_v<Op, meta::base>, void>
    base(Op op) const noexcept {
        internal::iterate<&internal::type_node::base>([op = std::move(op)](auto *curr) {
            op(curr->clazz());
        }, node);
    }

    /**
     * @brief Returns the meta base associated with a given identifier.
     *
     * Searches recursively among **all** the base classes of the given type.
     *
     * @param identifier Unique identifier.
     * @return The meta base associated with the given identifier, if any.
     */
    meta::base base(const std::size_t identifier) const noexcept {
        const auto *curr = internal::find_if<&internal::type_node::base>([identifier](auto *candidate) {
            return candidate->ref()->identifier == identifier;
        }, node);

        return curr ? curr->clazz() : meta::base{};
    }

    /**
     * @brief Iterates all the meta conversion functions of a meta type.
     *
     * Iteratively returns **all** the meta conversion functions of the given
     * type.
     *
     * @tparam Op Type of the function object to invoke.
     * @param op A valid function object.
     */
    template<typename Op>
    void conv(Op op) const noexcept {
        internal::iterate<&internal::type_node::conv>([op = std::move(op)](auto *curr) {
            op(curr->clazz());
        }, node);
    }

    /**
     * @brief Returns the meta conversion function associated with a given type.
     *
     * Searches recursively among **all** the conversion functions of the given
     * type.
     *
     * @tparam Type The type to use to search for a meta conversion function.
     * @return The meta conversion function associated with the given type, if
     * any.
     */
    template<typename Type>
    meta::conv conv() const noexcept {
        const auto *curr = internal::find_if<&internal::type_node::conv>([type = internal::type_info<Type>::resolve()](auto *candidate) {
            return candidate->ref() == type;
        }, node);

        return curr ? curr->clazz() : meta::conv{};
    }

    /**
     * @brief Iterates all the meta constructors of a meta type.
     * @tparam Op Type of the function object to invoke.
     * @param op A valid function object.
     */
    template<typename Op>
    void ctor(Op op) const noexcept {
        internal::iterate([op = std::move(op)](auto *curr) {
            op(curr->clazz());
        }, node->ctor);
    }

    /**
     * @brief Returns the meta constructor that accepts a given list of types of
     * arguments.
     * @return The requested meta constructor, if any.
     */
    template<typename... Args>
    meta::ctor ctor() const noexcept {
        const auto *curr = internal::ctor<Args...>(std::make_index_sequence<sizeof...(Args)>{}, node);
        return curr ? curr->clazz() : meta::ctor{};
    }

    /**
     * @brief Returns the meta destructor associated with a given type.
     * @return The meta destructor associated with the given type, if any.
     */
    meta::dtor dtor() const noexcept {
        return node->dtor ? node->dtor->clazz() : meta::dtor{};
    }

    /**
     * @brief Iterates all the meta data of a meta type.
     *
     * Iteratively returns **all** the meta data of the given type. This means
     * that the meta data of the base classes will also be returned, if any.
     *
     * @tparam Op Type of the function object to invoke.
     * @param op A valid function object.
     */
    template<typename Op>
    std::enable_if_t<std::is_invocable_v<Op, meta::data>, void>
    data(Op op) const noexcept {
        internal::iterate<&internal::type_node::data>([op = std::move(op)](auto *curr) {
            op(curr->clazz());
        }, node);
    }

    /**
     * @brief Returns the meta data associated with a given identifier.
     *
     * Searches recursively among **all** the meta data of the given type. This
     * means that the meta data of the base classes will also be inspected, if
     * any.
     *
     * @param identifier Unique identifier.
     * @return The meta data associated with the given identifier, if any.
     */
    meta::data data(const std::size_t identifier) const noexcept {
        const auto *curr = internal::find_if<&internal::type_node::data>([identifier](auto *candidate) {
            return candidate->identifier == identifier;
        }, node);

        return curr ? curr->clazz() : meta::data{};
    }

    /**
     * @brief Iterates all the meta functions of a meta type.
     *
     * Iteratively returns **all** the meta functions of the given type. This
     * means that the meta functions of the base classes will also be returned,
     * if any.
     *
     * @tparam Op Type of the function object to invoke.
     * @param op A valid function object.
     */
    template<typename Op>
    std::enable_if_t<std::is_invocable_v<Op, meta::func>, void>
    func(Op op) const noexcept {
        internal::iterate<&internal::type_node::func>([op = std::move(op)](auto *curr) {
            op(curr->clazz());
        }, node);
    }

    /**
     * @brief Returns the meta function associated with a given identifier.
     *
     * Searches recursively among **all** the meta functions of the given type.
     * This means that the meta functions of the base classes will also be
     * inspected, if any.
     *
     * @param identifier Unique identifier.
     * @return The meta function associated with the given identifier, if any.
     */
    meta::func func(const std::size_t identifier) const noexcept {
        const auto *curr = internal::find_if<&internal::type_node::func>([identifier](auto *candidate) {
            return candidate->identifier == identifier;
        }, node);

        return curr ? curr->clazz() : meta::func{};
    }

    /**
     * @brief Creates an instance of the underlying type, if possible.
     *
     * To create a valid instance, the types of the parameters must coincide
     * exactly with those required by the underlying meta constructor.
     * Otherwise, an empty and then invalid container is returned.
     *
     * @tparam Args Types of arguments to use to construct the instance.
     * @param args Parameters to use to construct the instance.
     * @return A meta any containing the new instance, if any.
     */
    template<typename... Args>
    any construct(Args &&... args) const {
        std::array<any, sizeof...(Args)> arguments{{std::forward<Args>(args)...}};
        any any{};

        internal::find_if<&internal::type_node::ctor>([data = arguments.data(), &any](auto *curr) -> bool {
            if(curr->size == sizeof...(args)) {
                any = curr->invoke(data);
            }

            return static_cast<bool>(any);
        }, node);

        return any;
    }

    /**
     * @brief Destroys an instance of the underlying type.
     *
     * It must be possible to cast the instance to the underlying type.
     * Otherwise, invoking the meta destructor results in an undefined
     * behavior.<br/>
     * If no destructor has been set, this function returns true without doing
     * anything.
     *
     * @param handle An opaque pointer to an instance of the underlying type.
     * @return True in case of success, false otherwise.
     */
    bool destroy(handle handle) const {
        return (handle.type() == node->clazz()) && (!node->dtor || node->dtor->invoke(handle));
    }

    /**
     * @brief Iterates all the properties assigned to a meta type.
     *
     * Iteratively returns **all** the properties of the given type. This means
     * that the properties of the base classes will also be returned, if any.
     *
     * @tparam Op Type of the function object to invoke.
     * @param op A valid function object.
     */
    template<typename Op>
    std::enable_if_t<std::is_invocable_v<Op, meta::prop>, void>
    prop(Op op) const noexcept {
        internal::iterate<&internal::type_node::prop>([op = std::move(op)](auto *curr) {
            op(curr->clazz());
        }, node);
    }

    /**
     * @brief Returns the property associated with a given key.
     *
     * Searches recursively among **all** the properties of the given type. This
     * means that the properties of the base classes will also be inspected, if
     * any.
     *
     * @tparam Key Type of key to use to search for a property.
     * @param key The key to use to search for a property.
     * @return The property associated with the given key, if any.
     */
    template<typename Key>
    std::enable_if_t<!std::is_invocable_v<Key, meta::prop>, meta::prop>
    prop(Key &&key) const noexcept {
        const auto *curr = internal::find_if<&internal::type_node::prop>([key = any{std::forward<Key>(key)}](auto *candidate) {
            return candidate->key() == key;
        }, node);

        return curr ? curr->clazz() : meta::prop{};
    }

    /**
     * @brief Returns true if a meta object is valid, false otherwise.
     * @return True if the meta object is valid, false otherwise.
     */
    explicit operator bool() const noexcept {
        return node;
    }

    /**
     * @brief Checks if two meta objects refer to the same node.
     * @param other The meta object with which to compare.
     * @return True if the two meta objects refer to the same node, false
     * otherwise.
     */
    bool operator==(const type &other) const noexcept {
        return node == other.node;
    }

private:
    const internal::type_node *node;
};


/**
 * @brief Checks if two meta objects refer to the same node.
 * @param lhs A meta object, either valid or not.
 * @param rhs A meta object, either valid or not.
 * @return True if the two meta objects refer to the same node, false otherwise.
 */
inline bool operator!=(const type &lhs, const type &rhs) noexcept {
    return !(lhs == rhs);
}


inline any::any(handle handle) noexcept
    : any{}
{
    node = handle.node;
    instance = handle.instance;
}


inline meta::type any::type() const noexcept {
    return node ? node->clazz() : meta::type{};
}


inline meta::type handle::type() const noexcept {
    return node ? node->clazz() : meta::type{};
}


inline meta::type base::parent() const noexcept {
    return node->parent->clazz();
}


inline meta::type base::type() const noexcept {
    return node->ref()->clazz();
}


inline meta::type conv::parent() const noexcept {
    return node->parent->clazz();
}


inline meta::type conv::type() const noexcept {
    return node->ref()->clazz();
}


inline meta::type ctor::parent() const noexcept {
    return node->parent->clazz();
}


inline meta::type ctor::arg(size_type index) const noexcept {
    return index < size() ? node->arg(index)->clazz() : meta::type{};
}


inline meta::type dtor::parent() const noexcept {
    return node->parent->clazz();
}


inline meta::type data::parent() const noexcept {
    return node->parent->clazz();
}


inline meta::type data::type() const noexcept {
    return node->ref()->clazz();
}


inline meta::type func::parent() const noexcept {
    return node->parent->clazz();
}


inline meta::type func::ret() const noexcept {
    return node->ret()->clazz();
}


inline meta::type func::arg(size_type index) const noexcept {
    return index < size() ? node->arg(index)->clazz() : meta::type{};
}


/**
 * @cond TURN_OFF_DOXYGEN
 * Internal details not to be documented.
 */


namespace internal {


template<typename Type, typename = std::enable_if_t<!std::is_void_v<Type> && !std::is_function_v<Type>>>
static auto compare(int, const void *lhs, const void *rhs)
-> decltype(std::declval<Type>() == std::declval<Type>(), bool{}) {
    return *static_cast<const Type *>(lhs) == *static_cast<const Type *>(rhs);
}

template<typename>
static bool compare(char, const void *lhs, const void *rhs) {
    return lhs == rhs;
}


template<typename Type>
inline type_node * info_node<Type>::resolve() noexcept {
    if(!type) {
        static type_node node{
            {},
            nullptr,
            nullptr,
            std::is_void_v<Type>,
            std::is_integral_v<Type>,
            std::is_floating_point_v<Type>,
            std::is_array_v<Type>,
            std::is_enum_v<Type>,
            std::is_union_v<Type>,
            std::is_class_v<Type>,
            std::is_pointer_v<Type>,
            std::is_pointer_v<Type> && std::is_function_v<std::remove_pointer_t<Type>>,
            std::is_member_object_pointer_v<Type>,
            std::is_member_function_pointer_v<Type>,
            std::extent_v<Type>,
            [](const void *lhs, const void *rhs) {
                return compare<Type>(0, lhs, rhs);
            },
            []() noexcept -> meta::type {
                return internal::type_info<std::remove_pointer_t<Type>>::resolve();
            },
            []() noexcept -> meta::type {
                return &node;
            }
        };

        type = &node;
    }

    return type;
}


}


/**
 * Internal details not to be documented.
 * @endcond TURN_OFF_DOXYGEN
 */


}


#endif // META_META_HPP
