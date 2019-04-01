#ifndef META_META_HPP
#define META_META_HPP


#include <tuple>
#include <array>
#include <memory>
#include <string>
#include <cstring>
#include <cassert>
#include <cstddef>
#include <utility>
#include <functional>
#include <type_traits>


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


struct prop_node final {
    prop_node * next;
    any(* const key)();
    any(* const value)();
    prop(* const clazz)();
};


struct base_node final {
    base_node ** const underlying;
    type_node * const parent;
    base_node * next;
    type_node *(* const ref)();
    void *(* const cast)(void *);
    base(* const clazz)();
};


struct conv_node final {
    conv_node ** const underlying;
    type_node * const parent;
    conv_node * next;
    type_node *(* const ref)();
    any(* const convert)(void *);
    conv(* const clazz)();
};


struct ctor_node final {
    using size_type = std::size_t;
    ctor_node ** const underlying;
    type_node * const parent;
    ctor_node * next;
    prop_node * prop;
    const size_type size;
    type_node *(* const arg)(size_type);
    any(* const invoke)(any * const);
    ctor(* const clazz)();
};


struct dtor_node final {
    dtor_node ** const underlying;
    type_node * const parent;
    bool(* const invoke)(handle);
    dtor(* const clazz)();
};


struct data_node final {
    data_node ** const underlying;
    const char *name;
    std::size_t id;
    type_node * const parent;
    data_node * next;
    prop_node * prop;
    const bool is_const;
    const bool is_static;
    type_node *(* const ref)();
    bool(* const set)(handle, any &);
    any(* const get)(handle);
    data(* const clazz)();
};


struct func_node final {
    using size_type = std::size_t;
    func_node ** const underlying;
    const char *name;
    std::size_t id;
    type_node * const parent;
    func_node * next;
    prop_node * prop;
    const size_type size;
    const bool is_const;
    const bool is_static;
    type_node *(* const ret)();
    type_node *(* const arg)(size_type);
    any(* const invoke)(handle, any *);
    func(* const clazz)();
};


struct type_node final {
    const char *name;
    std::size_t id;
    type_node * next;
    prop_node * prop;
    const bool is_void;
    const bool is_integral;
    const bool is_floating_point;
    const bool is_enum;
    const bool is_union;
    const bool is_class;
    const bool is_pointer;
    const bool is_function;
    const bool is_member_object_pointer;
    const bool is_member_function_pointer;
    type(* const remove_pointer)();
    bool(* const destroy)(handle);
    type(* const clazz)();
    base_node *base;
    conv_node *conv;
    ctor_node *ctor;
    dtor_node *dtor;
    data_node *data;
    func_node *func;
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
-> decltype(find_if(op, node->*Member))
{
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
        const auto *base = find_if<&type_node::base>([type](auto *node) {
            return node->ref() == type;
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
    return internal::find_if([](auto *node) {
        return node->size == sizeof...(Args) &&
                (([](auto *from, auto *to) {
                    return internal::can_cast_or_convert<&internal::type_node::base>(from, to)
                            || internal::can_cast_or_convert<&internal::type_node::conv>(from, to);
                }(internal::type_info<Args>::resolve(), node->arg(Indexes))) && ...);
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
class any final {
    /*! @brief A meta handle is allowed to _inherit_ from a meta any. */
    friend class handle;

    using storage_type = std::aligned_storage_t<sizeof(void *), alignof(void *)>;
    using compare_fn_type = bool(*)(const void *, const void *);
    using copy_fn_type = void *(*)(storage_type &, const void *);
    using destroy_fn_type = void(*)(storage_type &);

    template<typename Type>
    inline static auto compare(int, const Type &lhs, const Type &rhs)
    -> decltype(lhs == rhs, bool{})
    {
        return lhs == rhs;
    }

    template<typename Type>
    inline static bool compare(char, const Type &lhs, const Type &rhs) {
        return &lhs == &rhs;
    }

public:
    /*! @brief Default constructor. */
    any() noexcept
        : storage{},
          instance{nullptr},
          destroy{nullptr},
          node{nullptr},
          comparator{nullptr}
    {}

    /**
     * @brief Constructs a meta any from a given value.
     *
     * This class uses a technique called small buffer optimization (SBO) to
     * completely eliminate the need to allocate memory, where possible.<br/>
     * From the user's point of view, nothing will change, but the elimination
     * of allocations will reduce the jumps in memory and therefore will avoid
     * chasing of pointers. This will greatly improve the use of the cache, thus
     * increasing the overall performance.
     *
     * @tparam Type Type of object to use to initialize the container.
     * @param type An instance of an object to use to initialize the container.
     */
    template<typename Type, typename = std::enable_if_t<!std::is_same_v<std::decay_t<Type>, any>>>
    any(Type &&type) {
        using actual_type = std::decay_t<Type>;
        node = internal::type_info<Type>::resolve();

        comparator = [](const void *lhs, const void *rhs) {
            return compare(0, *static_cast<const actual_type *>(lhs), *static_cast<const actual_type *>(rhs));
        };

        if constexpr(sizeof(actual_type) <= sizeof(void *)) {
            new (&storage) actual_type{std::forward<Type>(type)};
            instance = &storage;

            copy = [](storage_type &storage, const void *instance) -> void * {
                return new (&storage) actual_type{*static_cast<const actual_type *>(instance)};
            };

            destroy = [](storage_type &storage) {
                auto *node = internal::type_info<Type>::resolve();
                auto *instance = reinterpret_cast<actual_type *>(&storage);
                node->dtor ? node->dtor->invoke(*instance) : node->destroy(*instance);
            };
        } else {
            using chunk_type = std::aligned_storage_t<sizeof(actual_type), alignof(actual_type)>;
            auto *chunk = new chunk_type;
            instance = new (chunk) actual_type{std::forward<Type>(type)};
            new (&storage) chunk_type *{chunk};

            copy = [](storage_type &storage, const void *instance) -> void * {
                auto *chunk = new chunk_type;
                new (&storage) chunk_type *{chunk};
                return new (chunk) actual_type{*static_cast<const actual_type *>(instance)};
            };

            destroy = [](storage_type &storage) {
                auto *node = internal::type_info<Type>::resolve();
                auto *chunk = *reinterpret_cast<chunk_type **>(&storage);
                auto *instance = reinterpret_cast<actual_type *>(chunk);
                node->dtor ? node->dtor->invoke(*instance) : node->destroy(*instance);
                delete chunk;
            };
        }
    }

    /**
     * @brief Copy constructor.
     * @param other The instance to copy from.
     */
    any(const any &other)
        : any{}
    {
        if(other) {
            instance = other.copy(storage, other.instance);
            destroy = other.destroy;
            node = other.node;
            comparator = other.comparator;
            copy = other.copy;
        }
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
        if(destroy) {
            destroy(storage);
        }
    }

    /**
     * @brief Assignment operator.
     * @param other The instance to assign.
     * @return This meta any object.
     */
    any & operator=(any other) {
        swap(*this, other);
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
    inline const void * data() const noexcept {
        return instance;
    }

    /**
     * @brief Returns an opaque pointer to the contained instance.
     * @return An opaque pointer the contained instance, if any.
     */
    inline void * data() noexcept {
        return const_cast<void *>(std::as_const(*this).data());
    }

    /**
     * @brief Checks if it's possible to cast an instance to a given type.
     * @tparam Type Type to which to cast the instance.
     * @return True if the cast is viable, false otherwise.
     */
    template<typename Type>
    inline bool can_cast() const noexcept {
        const auto *type = internal::type_info<Type>::resolve();
        return internal::can_cast_or_convert<&internal::type_node::base>(node, type);
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
    inline const Type & cast() const noexcept {
        assert(can_cast<Type>());
        return *internal::try_cast<Type>(node, instance);
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
    inline Type & cast() noexcept {
        return const_cast<Type &>(std::as_const(*this).cast<Type>());
    }

    /**
     * @brief Checks if it's possible to convert an instance to a given type.
     * @tparam Type Type to which to convert the instance.
     * @return True if the conversion is viable, false otherwise.
     */
    template<typename Type>
    inline bool can_convert() const noexcept {
        const auto *type = internal::type_info<Type>::resolve();
        return internal::can_cast_or_convert<&internal::type_node::conv>(node, type);
    }

    /**
     * @brief Tries to convert an instance to a given type and returns it.
     * @tparam Type Type to which to convert the instance.
     * @return A valid meta any object if the conversion is possible, an invalid
     * one otherwise.
     */
    template<typename Type>
    inline any convert() const noexcept {
        const auto *type = internal::type_info<Type>::resolve();
        any any{};

        if(node == type) {
            any = *static_cast<const Type *>(instance);
        } else {
            const auto *conv = internal::find_if<&internal::type_node::conv>([type](auto *node) {
                return node->ref() == type;
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
    inline bool convert() noexcept {
        bool valid = (node == internal::type_info<Type>::resolve());

        if(!valid) {
            auto any = std::as_const(*this).convert<Type>();

            if(any) {
                std::swap(*this, any);
                valid = true;
            }
        }

        return valid;
    }

    /**
     * @brief Returns false if a container is empty, true otherwise.
     * @return False if the container is empty, true otherwise.
     */
    inline explicit operator bool() const noexcept {
        return destroy;
    }

    /**
     * @brief Checks if two containers differ in their content.
     * @param other Container with which to compare.
     * @return False if the two containers differ in their content, true
     * otherwise.
     */
    inline bool operator==(const any &other) const noexcept {
        return (!instance && !other.instance) || (instance && other.instance && node == other.node && comparator(instance, other.instance));
    }

    /**
     * @brief Swaps two meta any objects.
     * @param lhs A valid meta any object.
     * @param rhs A valid meta any object.
     */
    friend void swap(any &lhs, any &rhs) {
        using std::swap;

        std::swap(lhs.storage, rhs.storage);
        std::swap(lhs.instance, rhs.instance);
        std::swap(lhs.destroy, rhs.destroy);
        std::swap(lhs.node, rhs.node);
        std::swap(lhs.comparator, rhs.comparator);
        std::swap(lhs.copy, rhs.copy);

        if(lhs.instance == &rhs.storage) {
            lhs.instance = &lhs.storage;
        }

        if(rhs.instance == &lhs.storage) {
            rhs.instance = &rhs.storage;
        }
    }

private:
    storage_type storage;
    void *instance;
    destroy_fn_type destroy;
    internal::type_node *node;
    compare_fn_type comparator;
    copy_fn_type copy;
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
class handle final {
    handle(int, any &any) noexcept
        : node{any.node},
          instance{any.instance}
    {}

    template<typename Type>
    handle(char, Type &&instance) noexcept
        : node{internal::type_info<Type>::resolve()},
          instance{&instance}
    {}

public:
    /*! @brief Default constructor. */
    handle() noexcept
        : node{nullptr},
          instance{nullptr}
    {}

    /**
     * @brief Constructs a meta handle from a given instance.
     * @tparam Type Type of object to use to initialize the handle.
     * @param instance A reference to an object to use to initialize the handle.
     */
    template<typename Type, typename = std::enable_if_t<!std::is_same_v<std::decay_t<Type>, handle>>>
    handle(Type &&instance) noexcept
        : handle{0, std::forward<Type>(instance)}
    {}

    /**
     * @brief Returns the meta type of the underlying object.
     * @return The meta type of the underlying object, if any.
     */
    inline meta::type type() const noexcept;

    /**
     * @brief Tries to cast an instance to a given type.
     *
     * The type of the instance must be such that the conversion is possible.
     *
     * @warning
     * Attempting to perform a conversion that isn't viable results in undefined
     * behavior.<br/>
     * An assertion will abort the execution at runtime in debug mode in case
     * the conversion is not feasible.
     *
     * @tparam Type Type to which to cast the instance.
     * @return A pointer to the contained instance.
     */
    template<typename Type>
    inline const Type * try_cast() const noexcept {
        return internal::try_cast<Type>(node, instance);
    }

    /**
     * @brief Tries to cast an instance to a given type.
     *
     * The type of the instance must be such that the conversion is possible.
     *
     * @warning
     * Attempting to perform a conversion that isn't viable results in undefined
     * behavior.<br/>
     * An assertion will abort the execution at runtime in debug mode in case
     * the conversion is not feasible.
     *
     * @tparam Type Type to which to cast the instance.
     * @return A pointer to the contained instance.
     */
    template<typename Type>
    inline Type * try_cast() noexcept {
        return const_cast<Type *>(std::as_const(*this).try_cast<Type>());
    }

    /**
     * @brief Returns an opaque pointer to the contained instance.
     * @return An opaque pointer the contained instance, if any.
     */
    inline const void * data() const noexcept {
        return instance;
    }

    /**
     * @brief Returns an opaque pointer to the contained instance.
     * @return An opaque pointer the contained instance, if any.
     */
    inline void * data() noexcept {
        return const_cast<void *>(std::as_const(*this).data());
    }

    /**
     * @brief Returns false if a handle is empty, true otherwise.
     * @return False if the handle is empty, true otherwise.
     */
    inline explicit operator bool() const noexcept {
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
class prop final {
    /*! @brief A meta factory is allowed to create meta objects. */
    template<typename> friend class factory;

    inline prop(const internal::prop_node *node) noexcept
        : node{node}
    {}

public:
    /*! @brief Default constructor. */
    inline prop() noexcept
        : node{nullptr}
    {}

    /**
     * @brief Returns the stored key.
     * @return A meta any containing the key stored with the given property.
     */
    inline any key() const noexcept {
        return node->key();
    }

    /**
     * @brief Returns the stored value.
     * @return A meta any containing the value stored with the given property.
     */
    inline any value() const noexcept {
        return node->value();
    }

    /**
     * @brief Returns true if a meta object is valid, false otherwise.
     * @return True if the meta object is valid, false otherwise.
     */
    inline explicit operator bool() const noexcept {
        return node;
    }

    /**
     * @brief Checks if two meta objects refer to the same node.
     * @param other The meta object with which to compare.
     * @return True if the two meta objects refer to the same node, false
     * otherwise.
     */
    inline bool operator==(const prop &other) const noexcept {
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
class base final {
    /*! @brief A meta factory is allowed to create meta objects. */
    template<typename> friend class factory;

    inline base(const internal::base_node *node) noexcept
        : node{node}
    {}

public:
    /*! @brief Default constructor. */
    inline base() noexcept
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
    inline void * cast(void *instance) const noexcept {
        return node->cast(instance);
    }

    /**
     * @brief Returns true if a meta object is valid, false otherwise.
     * @return True if the meta object is valid, false otherwise.
     */
    inline explicit operator bool() const noexcept {
        return node;
    }

    /**
     * @brief Checks if two meta objects refer to the same node.
     * @param other The meta object with which to compare.
     * @return True if the two meta objects refer to the same node, false
     * otherwise.
     */
    inline bool operator==(const base &other) const noexcept {
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
class conv final {
    /*! @brief A meta factory is allowed to create meta objects. */
    template<typename> friend class factory;

    inline conv(const internal::conv_node *node) noexcept
        : node{node}
    {}

public:
    /*! @brief Default constructor. */
    inline conv() noexcept
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
    inline any convert(void *instance) const noexcept {
        return node->convert(instance);
    }

    /**
     * @brief Returns true if a meta object is valid, false otherwise.
     * @return True if the meta object is valid, false otherwise.
     */
    inline explicit operator bool() const noexcept {
        return node;
    }

    /**
     * @brief Checks if two meta objects refer to the same node.
     * @param other The meta object with which to compare.
     * @return True if the two meta objects refer to the same node, false
     * otherwise.
     */
    inline bool operator==(const conv &other) const noexcept {
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
class ctor final {
    /*! @brief A meta factory is allowed to create meta objects. */
    template<typename> friend class factory;

    inline ctor(const internal::ctor_node *node) noexcept
        : node{node}
    {}

public:
    /*! @brief Unsigned integer type. */
    using size_type = typename internal::ctor_node::size_type;

    /*! @brief Default constructor. */
    inline ctor() noexcept
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
    inline size_type size() const noexcept {
        return node->size;
    }

    /**
     * @brief Returns the meta type of the i-th argument of a meta constructor.
     * @param index The index of the argument of which to return the meta type.
     * @return The meta type of the i-th argument of a meta constructor, if any.
     */
    inline meta::type arg(size_type index) const noexcept;

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
    inline std::enable_if_t<std::is_invocable_v<Op, meta::prop>, void>
    prop(Op op) const noexcept {
        internal::iterate([op = std::move(op)](auto *node) {
            op(node->clazz());
        }, node->prop);
    }

    /**
     * @brief Returns the property associated with a given key.
     * @tparam Key Type of key to use to search for a property.
     * @param key The key to use to search for a property.
     * @return The property associated with the given key, if any.
     */
    template<typename Key>
    inline std::enable_if_t<!std::is_invocable_v<Key, meta::prop>, meta::prop>
    prop(Key &&key) const noexcept {
        const auto *curr = internal::find_if([key = any{std::forward<Key>(key)}](auto *curr) {
            return curr->key() == key;
        }, node->prop);

        return curr ? curr->clazz() : meta::prop{};
    }

    /**
     * @brief Returns true if a meta object is valid, false otherwise.
     * @return True if the meta object is valid, false otherwise.
     */
    inline explicit operator bool() const noexcept {
        return node;
    }

    /**
     * @brief Checks if two meta objects refer to the same node.
     * @param other The meta object with which to compare.
     * @return True if the two meta objects refer to the same node, false
     * otherwise.
     */
    inline bool operator==(const ctor &other) const noexcept {
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
class dtor final {
    /*! @brief A meta factory is allowed to create meta objects. */
    template<typename> friend class factory;

    inline dtor(const internal::dtor_node *node) noexcept
        : node{node}
    {}

public:
    /*! @brief Default constructor. */
    inline dtor() noexcept
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
    inline bool invoke(handle handle) const {
        return node->invoke(handle);
    }

    /**
     * @brief Returns true if a meta object is valid, false otherwise.
     * @return True if the meta object is valid, false otherwise.
     */
    inline explicit operator bool() const noexcept {
        return node;
    }

    /**
     * @brief Checks if two meta objects refer to the same node.
     * @param other The meta object with which to compare.
     * @return True if the two meta objects refer to the same node, false
     * otherwise.
     */
    inline bool operator==(const dtor &other) const noexcept {
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
class data final {
    /*! @brief A meta factory is allowed to create meta objects. */
    template<typename> friend class factory;

    inline data(const internal::data_node *node) noexcept
        : node{node}
    {}

public:
    /*! @brief Default constructor. */
    inline data() noexcept
        : node{nullptr}
    {}

    /**
     * @brief Returns the name assigned to a given meta data.
     * @return The name assigned to the meta data.
     */
    inline const char * name() const noexcept {
        return node->name;
    }

    /**
     * @brief Returns the meta type to which a meta data belongs.
     * @return The meta type to which the meta data belongs.
     */
    inline meta::type parent() const noexcept;

    /**
     * @brief Indicates whether a given meta data is constant or not.
     * @return True if the meta data is constant, false otherwise.
     */
    inline bool is_const() const noexcept {
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
    inline bool is_static() const noexcept {
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
    inline bool set(handle handle, Type &&value) const {
        any any{std::forward<Type>(value)};
        return node->set(handle, any);
    }

    /**
     * @brief Gets the value of the variable enclosed by a given meta type.
     *
     * It must be possible to cast the instance to the parent type of the meta
     * function. Otherwise, invoking the getter results in an undefined
     * behavior.
     *
     * @param handle An opaque pointer to an instance of the underlying type.
     * @return A meta any containing the value of the underlying variable.
     */
    inline any get(handle handle) const noexcept {
        return node->get(handle);
    }

    /**
     * @brief Iterates all the properties assigned to a meta data.
     * @tparam Op Type of the function object to invoke.
     * @param op A valid function object.
     */
    template<typename Op>
    inline std::enable_if_t<std::is_invocable_v<Op, meta::prop>, void>
    prop(Op op) const noexcept {
        internal::iterate([op = std::move(op)](auto *node) {
            op(node->clazz());
        }, node->prop);
    }

    /**
     * @brief Returns the property associated with a given key.
     * @tparam Key Type of key to use to search for a property.
     * @param key The key to use to search for a property.
     * @return The property associated with the given key, if any.
     */
    template<typename Key>
    inline std::enable_if_t<!std::is_invocable_v<Key, meta::prop>, meta::prop>
    prop(Key &&key) const noexcept {
        const auto *curr = internal::find_if([key = any{std::forward<Key>(key)}](auto *curr) {
            return curr->key() == key;
        }, node->prop);

        return curr ? curr->clazz() : meta::prop{};
    }

    /**
     * @brief Returns true if a meta object is valid, false otherwise.
     * @return True if the meta object is valid, false otherwise.
     */
    inline explicit operator bool() const noexcept {
        return node;
    }

    /**
     * @brief Checks if two meta objects refer to the same node.
     * @param other The meta object with which to compare.
     * @return True if the two meta objects refer to the same node, false
     * otherwise.
     */
    inline bool operator==(const data &other) const noexcept {
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
class func final {
    /*! @brief A meta factory is allowed to create meta objects. */
    template<typename> friend class factory;

    inline func(const internal::func_node *node) noexcept
        : node{node}
    {}

public:
    /*! @brief Unsigned integer type. */
    using size_type = typename internal::func_node::size_type;

    /*! @brief Default constructor. */
    inline func() noexcept
        : node{nullptr}
    {}

    /**
     * @brief Returns the name assigned to a given meta function.
     * @return The name assigned to the meta function.
     */
    inline const char * name() const noexcept {
        return node->name;
    }

    /**
     * @brief Returns the meta type to which a meta function belongs.
     * @return The meta type to which the meta function belongs.
     */
    inline meta::type parent() const noexcept;

    /**
     * @brief Returns the number of arguments accepted by a meta function.
     * @return The number of arguments accepted by the meta function.
     */
    inline size_type size() const noexcept {
        return node->size;
    }

    /**
     * @brief Indicates whether a given meta function is constant or not.
     * @return True if the meta function is constant, false otherwise.
     */
    inline bool is_const() const noexcept {
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
    inline bool is_static() const noexcept {
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
        std::array<any, sizeof...(Args)> arguments{{std::forward<Args>(args)...}};
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
    inline std::enable_if_t<std::is_invocable_v<Op, meta::prop>, void>
    prop(Op op) const noexcept {
        internal::iterate([op = std::move(op)](auto *node) {
            op(node->clazz());
        }, node->prop);
    }

    /**
     * @brief Returns the property associated with a given key.
     * @tparam Key Type of key to use to search for a property.
     * @param key The key to use to search for a property.
     * @return The property associated with the given key, if any.
     */
    template<typename Key>
    inline std::enable_if_t<!std::is_invocable_v<Key, meta::prop>, meta::prop>
    prop(Key &&key) const noexcept {
        const auto *curr = internal::find_if([key = any{std::forward<Key>(key)}](auto *curr) {
            return curr->key() == key;
        }, node->prop);

        return curr ? curr->clazz() : meta::prop{};
    }

    /**
     * @brief Returns true if a meta object is valid, false otherwise.
     * @return True if the meta object is valid, false otherwise.
     */
    inline explicit operator bool() const noexcept {
        return node;
    }

    /**
     * @brief Checks if two meta objects refer to the same node.
     * @param other The meta object with which to compare.
     * @return True if the two meta objects refer to the same node, false
     * otherwise.
     */
    inline bool operator==(const func &other) const noexcept {
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
class type final {
    /*! @brief A meta factory is allowed to create meta objects. */
    template<typename> friend class factory;

    /*! @brief A meta node is allowed to create meta objects. */
    template<typename...> friend struct internal::info_node;

    inline type(const internal::type_node *node) noexcept
        : node{node}
    {}

public:
    /*! @brief Default constructor. */
    inline type() noexcept
        : node{nullptr}
    {}

    /**
     * @brief Returns the name assigned to a given meta type.
     * @return The name assigned to the meta type.
     */
    inline const char * name() const noexcept {
        return node->name;
    }

    /**
     * @brief Indicates whether a given meta type refers to void or not.
     * @return True if the underlying type is void, false otherwise.
     */
    inline bool is_void() const noexcept {
        return node->is_void;
    }

    /**
     * @brief Indicates whether a given meta type refers to an integral type or
     * not.
     * @return True if the underlying type is an integral type, false otherwise.
     */
    inline bool is_integral() const noexcept {
        return node->is_integral;
    }

    /**
     * @brief Indicates whether a given meta type refers to a floating-point
     * type or not.
     * @return True if the underlying type is a floating-point type, false
     * otherwise.
     */
    inline bool is_floating_point() const noexcept {
        return node->is_floating_point;
    }

    /**
     * @brief Indicates whether a given meta type refers to an enum or not.
     * @return True if the underlying type is an enum, false otherwise.
     */
    inline bool is_enum() const noexcept {
        return node->is_enum;
    }

    /**
     * @brief Indicates whether a given meta type refers to an union or not.
     * @return True if the underlying type is an union, false otherwise.
     */
    inline bool is_union() const noexcept {
        return node->is_union;
    }

    /**
     * @brief Indicates whether a given meta type refers to a class or not.
     * @return True if the underlying type is a class, false otherwise.
     */
    inline bool is_class() const noexcept {
        return node->is_class;
    }

    /**
     * @brief Indicates whether a given meta type refers to a pointer or not.
     * @return True if the underlying type is a pointer, false otherwise.
     */
    inline bool is_pointer() const noexcept {
        return node->is_pointer;
    }

    /**
     * @brief Indicates whether a given meta type refers to a function type or
     * not.
     * @return True if the underlying type is a function, false otherwise.
     */
    inline bool is_function() const noexcept {
        return node->is_function;
    }

    /**
     * @brief Indicates whether a given meta type refers to a pointer to data
     * member or not.
     * @return True if the underlying type is a pointer to data member, false
     * otherwise.
     */
    inline bool is_member_object_pointer() const noexcept {
        return node->is_member_object_pointer;
    }

    /**
     * @brief Indicates whether a given meta type refers to a pointer to member
     * function or not.
     * @return True if the underlying type is a pointer to member function,
     * false otherwise.
     */
    inline bool is_member_function_pointer() const noexcept {
        return node->is_member_function_pointer;
    }

    /**
     * @brief Provides the meta type for which the pointer is defined.
     * @return The meta type for which the pointer is defined or this meta type
     * if it doesn't refer to a pointer type.
     */
    inline meta::type remove_pointer() const noexcept {
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
    inline void base(Op op) const noexcept {
        internal::iterate<&internal::type_node::base>([op = std::move(op)](auto *node) {
            op(node->clazz());
        }, node);
    }

    /**
     * @brief Returns the meta base associated with a given name.
     *
     * Searches recursively among **all** the base classes of the given type.
     *
     * @param str The name to use to search for a meta base.
     * @return The meta base associated with the given name, if any.
     */
    inline meta::base base(const char *str) const noexcept {
        const auto *curr = internal::find_if<&internal::type_node::base>([id = std::hash<std::string>{}(str)](auto *node) {
            return node->ref()->id == id;
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
    inline void conv(Op op) const noexcept {
        internal::iterate<&internal::type_node::conv>([op = std::move(op)](auto *node) {
            op(node->clazz());
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
    inline meta::conv conv() const noexcept {
        const auto *curr = internal::find_if<&internal::type_node::conv>([type = internal::type_info<Type>::resolve()](auto *node) {
            return node->ref() == type;
        }, node);

        return curr ? curr->clazz() : meta::conv{};
    }

    /**
     * @brief Iterates all the meta constructors of a meta type.
     * @tparam Op Type of the function object to invoke.
     * @param op A valid function object.
     */
    template<typename Op>
    inline void ctor(Op op) const noexcept {
        internal::iterate([op = std::move(op)](auto *node) {
            op(node->clazz());
        }, node->ctor);
    }

    /**
     * @brief Returns the meta constructor that accepts a given list of types of
     * arguments.
     * @return The requested meta constructor, if any.
     */
    template<typename... Args>
    inline meta::ctor ctor() const noexcept {
        const auto *curr = internal::ctor<Args...>(std::make_index_sequence<sizeof...(Args)>{}, node);
        return curr ? curr->clazz() : meta::ctor{};
    }

    /**
     * @brief Returns the meta destructor associated with a given type.
     * @return The meta destructor associated with the given type, if any.
     */
    inline meta::dtor dtor() const noexcept {
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
    inline void data(Op op) const noexcept {
        internal::iterate<&internal::type_node::data>([op = std::move(op)](auto *node) {
            op(node->clazz());
        }, node);
    }

    /**
     * @brief Returns the meta data associated with a given name.
     *
     * Searches recursively among **all** the meta data of the given type. This
     * means that the meta data of the base classes will also be inspected, if
     * any.
     *
     * @param str The name to use to search for a meta data.
     * @return The meta data associated with the given name, if any.
     */
    inline meta::data data(const char *str) const noexcept {
        const auto *curr = internal::find_if<&internal::type_node::data>([id = std::hash<std::string>{}(str)](auto *node) {
            return node->id == id;
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
    inline void func(Op op) const noexcept {
        internal::iterate<&internal::type_node::func>([op = std::move(op)](auto *node) {
            op(node->clazz());
        }, node);
    }

    /**
     * @brief Returns the meta function associated with a given name.
     *
     * Searches recursively among **all** the meta functions of the given type.
     * This means that the meta functions of the base classes will also be
     * inspected, if any.
     *
     * @param str The name to use to search for a meta function.
     * @return The meta function associated with the given name, if any.
     */
    inline meta::func func(const char *str) const noexcept {
        const auto *curr = internal::find_if<&internal::type_node::func>([id = std::hash<std::string>{}(str)](auto *node) {
            return node->id == id;
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

        internal::iterate<&internal::type_node::ctor>([data = arguments.data(), &any](auto *node) -> bool {
            any = node->invoke(data);
            return static_cast<bool>(any);
        }, node);

        return any;
    }

    /**
     * @brief Destroys an instance of the underlying type.
     *
     * It must be possible to cast the instance to the underlying type.
     * Otherwise, invoking the meta destructor results in an undefined behavior.
     *
     * @param handle An opaque pointer to an instance of the underlying type.
     * @return True in case of success, false otherwise.
     */
    inline bool destroy(handle handle) const {
        return node->dtor ? node->dtor->invoke(handle) : node->destroy(handle);
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
    inline std::enable_if_t<std::is_invocable_v<Op, meta::prop>, void>
    prop(Op op) const noexcept {
        internal::iterate<&internal::type_node::prop>([op = std::move(op)](auto *node) {
            op(node->clazz());
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
    inline std::enable_if_t<!std::is_invocable_v<Key, meta::prop>, meta::prop>
    prop(Key &&key) const noexcept {
        const auto *curr = internal::find_if<&internal::type_node::prop>([key = any{std::forward<Key>(key)}](auto *curr) {
            return curr->key() == key;
        }, node);

        return curr ? curr->clazz() : meta::prop{};
    }

    /**
     * @brief Returns true if a meta object is valid, false otherwise.
     * @return True if the meta object is valid, false otherwise.
     */
    inline explicit operator bool() const noexcept {
        return node;
    }

    /**
     * @brief Checks if two meta objects refer to the same node.
     * @param other The meta object with which to compare.
     * @return True if the two meta objects refer to the same node, false
     * otherwise.
     */
    inline bool operator==(const type &other) const noexcept {
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


template<typename...>
struct function_helper;


template<typename Ret, typename... Args>
struct function_helper<Ret(Args...)> {
    using return_type = Ret;
    using args_type = std::tuple<Args...>;

    template<std::size_t Index>
    using arg_type = std::decay_t<std::tuple_element_t<Index, args_type>>;

    static constexpr auto size = sizeof...(Args);

    inline static auto arg(typename internal::func_node::size_type index) {
        return std::array<type_node *, sizeof...(Args)>{{type_info<Args>::resolve()...}}[index];
    }
};


template<typename Class, typename Ret, typename... Args, bool Const, bool Static>
struct function_helper<Class, Ret(Args...), std::bool_constant<Const>, std::bool_constant<Static>>: function_helper<Ret(Args...)> {
    using class_type = Class;
    static constexpr auto is_const = Const;
    static constexpr auto is_static = Static;
};


template<typename Ret, typename... Args, typename Class>
constexpr function_helper<Class, Ret(Args...), std::bool_constant<false>, std::bool_constant<false>>
to_function_helper(Ret(Class:: *)(Args...));


template<typename Ret, typename... Args, typename Class>
constexpr function_helper<Class, Ret(Args...), std::bool_constant<true>, std::bool_constant<false>>
to_function_helper(Ret(Class:: *)(Args...) const);


template<typename Ret, typename... Args>
constexpr function_helper<void, Ret(Args...), std::bool_constant<false>, std::bool_constant<true>>
to_function_helper(Ret(*)(Args...));


template<auto Func>
struct function_helper<std::integral_constant<decltype(Func), Func>>: decltype(to_function_helper(Func)) {};


template<typename Type>
inline bool destroy([[maybe_unused]] handle handle) {
    if constexpr(std::is_object_v<Type>) {
        return handle.type() == type_info<Type>::resolve()->clazz()
                ? (static_cast<Type *>(handle.data())->~Type(), true)
                : false;
    } else {
        return false;
    }
}


template<typename Type, typename... Args, std::size_t... Indexes>
inline any construct(any * const args, std::index_sequence<Indexes...>) {
    [[maybe_unused]] std::array<bool, sizeof...(Args)> can_cast{{(args+Indexes)->can_cast<std::decay_t<Args>>()...}};
    [[maybe_unused]] std::array<bool, sizeof...(Args)> can_convert{{(std::get<Indexes>(can_cast) ? false : (args+Indexes)->can_convert<std::decay_t<Args>>())...}};
    any any{};

    if(((std::get<Indexes>(can_cast) || std::get<Indexes>(can_convert)) && ...)) {
        ((std::get<Indexes>(can_convert) ? void((args+Indexes)->convert<std::decay_t<Args>>()) : void()), ...);
        any = Type{(args+Indexes)->cast<std::decay_t<Args>>()...};
    }

    return any;
}


template<bool Const, typename Type, auto Data>
bool setter([[maybe_unused]] handle handle, [[maybe_unused]] any &any) {
    bool accepted = false;

    if constexpr(Const) {
        return accepted;
    } else {
        if constexpr(std::is_function_v<std::remove_pointer_t<decltype(Data)>> || std::is_member_function_pointer_v<decltype(Data)>) {
            using helper_type = function_helper<std::integral_constant<decltype(Data), Data>>;
            using data_type = std::decay_t<std::tuple_element_t<!std::is_member_function_pointer_v<decltype(Data)>, typename helper_type::args_type>>;
            static_assert(std::is_invocable_v<decltype(Data), Type *, data_type>);
            accepted = any.can_cast<data_type>() || any.convert<data_type>();
            auto *clazz = handle.try_cast<Type>();

            if(accepted && clazz) {
                std::invoke(Data, clazz, any.cast<data_type>());
            }
        } else if constexpr(std::is_member_object_pointer_v<decltype(Data)>) {
            using data_type = std::decay_t<decltype(std::declval<Type>().*Data)>;
            static_assert(std::is_invocable_v<decltype(Data), Type>);
            accepted = any.can_cast<data_type>() || any.convert<data_type>();
            auto *clazz = handle.try_cast<Type>();

            if(accepted && clazz) {
                std::invoke(Data, clazz) = any.cast<data_type>();
            }
        } else {
            static_assert(std::is_pointer_v<decltype(Data)>);
            using data_type = std::decay_t<decltype(*Data)>;
            accepted = any.can_cast<data_type>() || any.convert<data_type>();

            if(accepted) {
                *Data = any.cast<data_type>();
            }
        }

        return accepted;
    }
}


template<typename Type, auto Data>
inline any getter([[maybe_unused]] handle handle) {
    if constexpr(std::is_function_v<std::remove_pointer_t<decltype(Data)>> || std::is_member_pointer_v<decltype(Data)>) {
        static_assert(std::is_invocable_v<decltype(Data), Type *>);
        auto *clazz = handle.try_cast<Type>();
        return clazz ? std::invoke(Data, clazz) : any{};
    } else {
        static_assert(std::is_pointer_v<decltype(Data)>);
        return any{*Data};
    }
}


template<typename Type, auto Func, std::size_t... Indexes>
std::enable_if_t<std::is_function_v<std::remove_pointer_t<decltype(Func)>>, any>
invoke(const handle &, any *args, std::index_sequence<Indexes...>) {
    using helper_type = function_helper<std::integral_constant<decltype(Func), Func>>;
    any ret{};

    if((((args+Indexes)->can_cast<typename helper_type::template arg_type<Indexes>>()
            || (args+Indexes)->convert<typename helper_type::template arg_type<Indexes>>()) && ...))
    {
        if constexpr(std::is_void_v<typename helper_type::return_type>) {
            std::invoke(Func, (args+Indexes)->cast<typename helper_type::template arg_type<Indexes>>()...);
        } else {
            ret = any{std::invoke(Func, (args+Indexes)->cast<typename helper_type::template arg_type<Indexes>>()...)};
        }
    }

    return ret;
}


template<typename Type, auto Member, std::size_t... Indexes>
std::enable_if_t<std::is_member_function_pointer_v<decltype(Member)>, any>
invoke(handle &handle, any *args, std::index_sequence<Indexes...>) {
    using helper_type = function_helper<std::integral_constant<decltype(Member), Member>>;
    static_assert(std::is_base_of_v<typename helper_type::class_type, Type>);
    auto *clazz = handle.try_cast<Type>();
    any ret{};

    if(clazz && (((args+Indexes)->can_cast<typename helper_type::template arg_type<Indexes>>()
                  || (args+Indexes)->convert<typename helper_type::template arg_type<Indexes>>()) && ...))
    {
        if constexpr(std::is_void_v<typename helper_type::return_type>) {
            std::invoke(Member, clazz, (args+Indexes)->cast<typename helper_type::template arg_type<Indexes>>()...);
        } else {
            ret = any{std::invoke(Member, clazz, (args+Indexes)->cast<typename helper_type::template arg_type<Indexes>>()...)};
        }
    }

    return ret;
}


template<typename Type>
type_node * info_node<Type>::resolve() noexcept {
    if(!type) {
        static type_node node{
            {},
            {},
            nullptr,
            nullptr,
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
            []() -> meta::type { return internal::type_info<std::remove_pointer_t<Type>>::resolve(); },
            &destroy<Type>,
            []() -> meta::type { return &node; }
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
