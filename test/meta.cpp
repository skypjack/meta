#include <utility>
#include <functional>
#include <type_traits>
#include <string_view>
#include <gtest/gtest.h>
#include <meta/factory.hpp>
#include <meta/meta.hpp>
#include <meta/policy.hpp>

template<typename Type>
void set(Type &prop, Type value) {
    prop = value;
}

template<typename Type>
Type get(Type &prop) {
    return prop;
}

enum class properties {
    prop_int,
    prop_bool
};

struct empty_type {
    virtual ~empty_type() = default;

    static void destroy(empty_type &) {
        ++counter;
    }

    inline static int counter = 0;
};

struct fat_type: empty_type {
    fat_type() = default;

    fat_type(int *value)
        : foo{value}, bar{value}
    {}

    int *foo{nullptr};
    int *bar{nullptr};

    bool operator==(const fat_type &other) const {
        return foo == other.foo && bar == other.bar;
    }
};

union union_type {
    int i;
    double d;
};

struct base_type {
    virtual ~base_type() = default;
};

struct derived_type: base_type {
    derived_type() = default;

    derived_type(const base_type &, int value, char character)
        : i{value}, c{character}
    {}

    int f() const { return i; }
    static char g(const derived_type &type) { return type.c; }

    const int i{};
    const char c{};
};

derived_type derived_factory(const base_type &, int value) {
    return {derived_type{}, value, 'c'};
}

struct data_type {
    int i{0};
    const int j{1};
    inline static int h{2};
    inline static const int k{3};
    empty_type empty{};
    int v{0};
};

struct array_type {
    static inline int global[3];
    int local[3];
};

struct func_type {
    int f(const base_type &, int a, int b) { return f(a, b); }
    int f(int a, int b) { value = a; return b*b; }
    int f(int v) const { return v*v; }
    void g(int v) { value = v*v; }

    static int h(int &v) { return (v *= value); }
    static void k(int v) { value = v; }

    int v(int v) const { return (value = v); }
    int & a() const { return value; }

    inline static int value = 0;
};

struct setter_getter_type {
    int value{};

    int setter(int val) { return value = val; }
    int getter() { return value; }

    int setter_with_ref(const int &val) { return value = val; }
    const int & getter_with_ref() { return value; }

    static int static_setter(setter_getter_type &type, int value) { return type.value = value; }
    static int static_getter(const setter_getter_type &type) { return type.value; }
};

struct not_comparable_type {
    bool operator==(const not_comparable_type &) const = delete;
};

bool operator!=(const not_comparable_type &, const not_comparable_type &) = delete;

struct an_abstract_type {
    virtual ~an_abstract_type() = default;
    void f(int v) { i = v; }
    virtual void g(int) = 0;
    int i{};
};

struct another_abstract_type {
    virtual ~another_abstract_type() = default;
    virtual void h(char) = 0;
    char j{};
};

struct concrete_type: an_abstract_type, another_abstract_type {
    void f(int v) { i = v*v; } // hide, it's ok :-)
    void g(int v) override { i = -v; }
    void h(char c) override { j = c; }
};

struct Meta: public ::testing::Test {
    static void SetUpTestCase() {
        meta::reflect<double>().conv<int>();
        std::hash<std::string_view> hash{};

        meta::reflect<char>(hash("char"), std::make_pair(properties::prop_int, 42))
                .data<&set<char>, &get<char>>(hash("value"));

        meta::reflect<properties>()
                .data<properties::prop_bool>(hash("prop_bool"))
                .data<properties::prop_int>(hash("prop_int"))
                .data<&set<properties>, &get<properties>>(hash("value"));

        meta::reflect<unsigned int>().data<0u>(hash("min")).data<100u>(hash("max"));

        meta::reflect<base_type>(hash("base"));

        meta::reflect<derived_type>(hash("derived"), std::make_pair(properties::prop_int, 99))
                .base<base_type>()
                .ctor<const base_type &, int, char>(std::make_pair(properties::prop_bool, false))
                .ctor<&derived_factory>(std::make_pair(properties::prop_int, 42))
                .conv<&derived_type::f>()
                .conv<&derived_type::g>();

        meta::reflect<empty_type>(hash("empty"))
                .dtor<&empty_type::destroy>();

        meta::reflect<fat_type>(hash("fat"))
                .base<empty_type>()
                .dtor<&fat_type::destroy>();

        meta::reflect<data_type>(hash("data"))
                .data<&data_type::i, meta::as_alias_t>(hash("i"), std::make_pair(properties::prop_int, 0))
                .data<&data_type::j>(hash("j"), std::make_pair(properties::prop_int, 1))
                .data<&data_type::h>(hash("h"), std::make_pair(properties::prop_int, 2))
                .data<&data_type::k>(hash("k"), std::make_pair(properties::prop_int, 3))
                .data<&data_type::empty>(hash("empty"))
                .data<&data_type::v, meta::as_void_t>(hash("v"));

        meta::reflect<array_type>(hash("array"))
                .data<&array_type::global>(hash("global"))
                .data<&array_type::local>(hash("local"));

        meta::reflect<func_type>(hash("func"))
                .func<static_cast<int(func_type:: *)(const base_type &, int, int)>(&func_type::f)>(hash("f3"))
                .func<static_cast<int(func_type:: *)(int, int)>(&func_type::f)>(hash("f2"), std::make_pair(properties::prop_bool, false))
                .func<static_cast<int(func_type:: *)(int) const>(&func_type::f)>(hash("f1"), std::make_pair(properties::prop_bool, false))
                .func<&func_type::g>(hash("g"), std::make_pair(properties::prop_bool, false))
                .func<&func_type::h>(hash("h"), std::make_pair(properties::prop_bool, false))
                .func<&func_type::k>(hash("k"), std::make_pair(properties::prop_bool, false))
                .func<&func_type::v, meta::as_void_t>(hash("v"))
                .func<&func_type::a, meta::as_alias_t>(hash("a"));

        meta::reflect<setter_getter_type>(hash("setter_getter"))
                .data<&setter_getter_type::static_setter, &setter_getter_type::static_getter>(hash("x"))
                .data<&setter_getter_type::setter, &setter_getter_type::getter>(hash("y"))
                .data<&setter_getter_type::static_setter, &setter_getter_type::getter>(hash("z"))
                .data<&setter_getter_type::setter_with_ref, &setter_getter_type::getter_with_ref>(hash("w"));

        meta::reflect<an_abstract_type>(hash("an_abstract_type"), std::make_pair(properties::prop_bool, false))
                .data<&an_abstract_type::i>(hash("i"))
                .func<&an_abstract_type::f>(hash("f"))
                .func<&an_abstract_type::g>(hash("g"));

        meta::reflect<another_abstract_type>(hash("another_abstract_type"), std::make_pair(properties::prop_int, 42))
                .data<&another_abstract_type::j>(hash("j"))
                .func<&another_abstract_type::h>(hash("h"));

        meta::reflect<concrete_type>(hash("concrete"))
                .base<an_abstract_type>()
                .base<another_abstract_type>()
                .func<&concrete_type::f>(hash("f"));
    }

    static void SetUpAfterUnregistration() {
        meta::reflect<double>().conv<float>();
        std::hash<std::string_view> hash{};

        meta::reflect<derived_type>(hash("my_type"), std::make_pair(properties::prop_bool, false))
                .ctor<>();

        meta::reflect<another_abstract_type>(hash("your_type"))
                .data<&another_abstract_type::j>(hash("a_data_member"))
                .func<&another_abstract_type::h>(hash("a_member_function"));
    }

    void SetUp() override {
        empty_type::counter = 0;
        func_type::value = 0;
    }
};

TEST_F(Meta, Resolve) {
    std::hash<std::string_view> hash{};
    ASSERT_EQ(meta::resolve<derived_type>(), meta::resolve(hash("derived")));

    bool found = false;

    meta::resolve([&found](auto type) {
        found = found || type == meta::resolve<derived_type>();
    });

    ASSERT_TRUE(found);
}

TEST_F(Meta, MetaAnyFromMetaHandle) {
    int value = 42;
    meta::handle handle{value};
    meta::any any{handle};
    any.cast<int>() = 3;

    ASSERT_TRUE(any);
    ASSERT_EQ(any.type(), meta::resolve<int>());
    ASSERT_EQ(any.try_cast<std::size_t>(), nullptr);
    ASSERT_EQ(any.try_cast<int>(), handle.data());
    ASSERT_EQ(std::as_const(any).try_cast<int>(), handle.data());
    ASSERT_EQ(any.data(), handle.data());
    ASSERT_EQ(std::as_const(any).data(), handle.data());
    ASSERT_EQ(value, 3);
}

TEST_F(Meta, MetaAnySBO) {
    meta::any any{'c'};

    ASSERT_TRUE(any);
    ASSERT_FALSE(any.try_cast<std::size_t>());
    ASSERT_TRUE(any.try_cast<char>());
    ASSERT_EQ(any.cast<char>(), 'c');
    ASSERT_EQ(std::as_const(any).cast<char>(), 'c');
    ASSERT_NE(any.data(), nullptr);
    ASSERT_NE(std::as_const(any).data(), nullptr);
    ASSERT_EQ(any, meta::any{'c'});
    ASSERT_NE(meta::any{'h'}, any);
}

TEST_F(Meta, MetaAnyNoSBO) {
    int value = 42;
    fat_type instance{&value};
    meta::any any{instance};

    ASSERT_TRUE(any);
    ASSERT_FALSE(any.try_cast<std::size_t>());
    ASSERT_TRUE(any.try_cast<fat_type>());
    ASSERT_EQ(any.cast<fat_type>(), instance);
    ASSERT_EQ(std::as_const(any).cast<fat_type>(), instance);
    ASSERT_NE(any.data(), nullptr);
    ASSERT_NE(std::as_const(any).data(), nullptr);
    ASSERT_EQ(any, meta::any{instance});
    ASSERT_NE(fat_type{}, any);
}

TEST_F(Meta, MetaAnyEmpty) {
    meta::any any{};

    ASSERT_FALSE(any);
    ASSERT_FALSE(any.type());
    ASSERT_FALSE(any.try_cast<std::size_t>());
    ASSERT_FALSE(any.try_cast<empty_type>());
    ASSERT_EQ(any.data(), nullptr);
    ASSERT_EQ(std::as_const(any).data(), nullptr);
    ASSERT_EQ(any, meta::any{});
    ASSERT_NE(meta::any{'c'}, any);
}

TEST_F(Meta, MetaAnySBOInPlaceTypeConstruction) {
    meta::any any{std::in_place_type<int>, 42};

    ASSERT_TRUE(any);
    ASSERT_FALSE(any.try_cast<std::size_t>());
    ASSERT_TRUE(any.try_cast<int>());
    ASSERT_EQ(any.cast<int>(), 42);
    ASSERT_EQ(std::as_const(any).cast<int>(), 42);
    ASSERT_NE(any.data(), nullptr);
    ASSERT_NE(std::as_const(any).data(), nullptr);
    ASSERT_EQ(any, (meta::any{std::in_place_type<int>, 42}));
    ASSERT_EQ(any, meta::any{42});
    ASSERT_NE(meta::any{3}, any);
}

TEST_F(Meta, MetaAnySBOAsAliasConstruction) {
    int value = 3;
    int other = 42;
    meta::any any{std::ref(value)};

    ASSERT_TRUE(any);
    ASSERT_FALSE(any.try_cast<std::size_t>());
    ASSERT_TRUE(any.try_cast<int>());
    ASSERT_EQ(any.cast<int>(), 3);
    ASSERT_EQ(std::as_const(any).cast<int>(), 3);
    ASSERT_NE(any.data(), nullptr);
    ASSERT_NE(std::as_const(any).data(), nullptr);
    ASSERT_EQ(any, (meta::any{std::ref(value)}));
    ASSERT_NE(any, (meta::any{std::ref(other)}));
    ASSERT_NE(any, meta::any{42});
    ASSERT_EQ(meta::any{3}, any);
}

TEST_F(Meta, MetaAnySBOCopyConstruction) {
    meta::any any{42};
    meta::any other{any};

    ASSERT_TRUE(any);
    ASSERT_TRUE(other);
    ASSERT_FALSE(other.try_cast<std::size_t>());
    ASSERT_TRUE(other.try_cast<int>());
    ASSERT_EQ(other.cast<int>(), 42);
    ASSERT_EQ(std::as_const(other).cast<int>(), 42);
    ASSERT_EQ(other, meta::any{42});
    ASSERT_NE(other, meta::any{0});
}

TEST_F(Meta, MetaAnySBOCopyAssignment) {
    meta::any any{42};
    meta::any other{3};

    other = any;

    ASSERT_TRUE(any);
    ASSERT_TRUE(other);
    ASSERT_FALSE(other.try_cast<std::size_t>());
    ASSERT_TRUE(other.try_cast<int>());
    ASSERT_EQ(other.cast<int>(), 42);
    ASSERT_EQ(std::as_const(other).cast<int>(), 42);
    ASSERT_EQ(other, meta::any{42});
    ASSERT_NE(other, meta::any{0});
}

TEST_F(Meta, MetaAnySBOMoveConstruction) {
    meta::any any{42};
    meta::any other{std::move(any)};

    ASSERT_FALSE(any);
    ASSERT_TRUE(other);
    ASSERT_FALSE(other.try_cast<std::size_t>());
    ASSERT_TRUE(other.try_cast<int>());
    ASSERT_EQ(other.cast<int>(), 42);
    ASSERT_EQ(std::as_const(other).cast<int>(), 42);
    ASSERT_EQ(other, meta::any{42});
    ASSERT_NE(other, meta::any{0});
}

TEST_F(Meta, MetaAnySBOMoveAssignment) {
    meta::any any{42};
    meta::any other{3};

    other = std::move(any);

    ASSERT_FALSE(any);
    ASSERT_TRUE(other);
    ASSERT_FALSE(other.try_cast<std::size_t>());
    ASSERT_TRUE(other.try_cast<int>());
    ASSERT_EQ(other.cast<int>(), 42);
    ASSERT_EQ(std::as_const(other).cast<int>(), 42);
    ASSERT_EQ(other, meta::any{42});
    ASSERT_NE(other, meta::any{0});
}

TEST_F(Meta, MetaAnySBODirectAssignment) {
    meta::any any{};
    any = 42;

    ASSERT_FALSE(any.try_cast<std::size_t>());
    ASSERT_TRUE(any.try_cast<int>());
    ASSERT_EQ(any.cast<int>(), 42);
    ASSERT_EQ(std::as_const(any).cast<int>(), 42);
    ASSERT_EQ(any, meta::any{42});
    ASSERT_NE(meta::any{0}, any);
}

TEST_F(Meta, MetaAnyNoSBOInPlaceTypeConstruction) {
    int value = 42;
    fat_type instance{&value};
    meta::any any{std::in_place_type<fat_type>, instance};

    ASSERT_TRUE(any);
    ASSERT_FALSE(any.try_cast<std::size_t>());
    ASSERT_TRUE(any.try_cast<fat_type>());
    ASSERT_EQ(any.cast<fat_type>(), instance);
    ASSERT_EQ(std::as_const(any).cast<fat_type>(), instance);
    ASSERT_NE(any.data(), nullptr);
    ASSERT_NE(std::as_const(any).data(), nullptr);
    ASSERT_EQ(any, (meta::any{std::in_place_type<fat_type>, instance}));
    ASSERT_EQ(any, meta::any{instance});
    ASSERT_NE(meta::any{fat_type{}}, any);
}

TEST_F(Meta, MetaAnyNoSBOAsAliasConstruction) {
    int value = 3;
    fat_type instance{&value};
    meta::any any{std::ref(instance)};

    ASSERT_TRUE(any);
    ASSERT_FALSE(any.try_cast<std::size_t>());
    ASSERT_TRUE(any.try_cast<fat_type>());
    ASSERT_EQ(any.cast<fat_type>(), instance);
    ASSERT_EQ(std::as_const(any).cast<fat_type>(), instance);
    ASSERT_NE(any.data(), nullptr);
    ASSERT_NE(std::as_const(any).data(), nullptr);
    ASSERT_EQ(any, (meta::any{std::ref(instance)}));
    ASSERT_EQ(any, meta::any{instance});
    ASSERT_NE(meta::any{fat_type{}}, any);
}

TEST_F(Meta, MetaAnyNoSBOCopyConstruction) {
    int value = 42;
    fat_type instance{&value};
    meta::any any{instance};
    meta::any other{any};

    ASSERT_TRUE(any);
    ASSERT_TRUE(other);
    ASSERT_FALSE(other.try_cast<std::size_t>());
    ASSERT_TRUE(other.try_cast<fat_type>());
    ASSERT_EQ(other.cast<fat_type>(), instance);
    ASSERT_EQ(std::as_const(other).cast<fat_type>(), instance);
    ASSERT_EQ(other, meta::any{instance});
    ASSERT_NE(other, fat_type{});
}

TEST_F(Meta, MetaAnyNoSBOCopyAssignment) {
    int value = 42;
    fat_type instance{&value};
    meta::any any{instance};
    meta::any other{3};

    other = any;

    ASSERT_TRUE(any);
    ASSERT_TRUE(other);
    ASSERT_FALSE(other.try_cast<std::size_t>());
    ASSERT_TRUE(other.try_cast<fat_type>());
    ASSERT_EQ(other.cast<fat_type>(), instance);
    ASSERT_EQ(std::as_const(other).cast<fat_type>(), instance);
    ASSERT_EQ(other, meta::any{instance});
    ASSERT_NE(other, fat_type{});
}

TEST_F(Meta, MetaAnyNoSBOMoveConstruction) {
    int value = 42;
    fat_type instance{&value};
    meta::any any{instance};
    meta::any other{std::move(any)};

    ASSERT_FALSE(any);
    ASSERT_TRUE(other);
    ASSERT_FALSE(other.try_cast<std::size_t>());
    ASSERT_TRUE(other.try_cast<fat_type>());
    ASSERT_EQ(other.cast<fat_type>(), instance);
    ASSERT_EQ(std::as_const(other).cast<fat_type>(), instance);
    ASSERT_EQ(other, meta::any{instance});
    ASSERT_NE(other, fat_type{});
}

TEST_F(Meta, MetaAnyNoSBOMoveAssignment) {
    int value = 42;
    fat_type instance{&value};
    meta::any any{instance};
    meta::any other{3};

    other = std::move(any);

    ASSERT_FALSE(any);
    ASSERT_TRUE(other);
    ASSERT_FALSE(other.try_cast<std::size_t>());
    ASSERT_TRUE(other.try_cast<fat_type>());
    ASSERT_EQ(other.cast<fat_type>(), instance);
    ASSERT_EQ(std::as_const(other).cast<fat_type>(), instance);
    ASSERT_EQ(other, meta::any{instance});
    ASSERT_NE(other, fat_type{});
}

TEST_F(Meta, MetaAnyNoSBODirectAssignment) {
    int value = 42;
    meta::any any{};
    any = fat_type{&value};

    ASSERT_FALSE(any.try_cast<std::size_t>());
    ASSERT_TRUE(any.try_cast<fat_type>());
    ASSERT_EQ(any.cast<fat_type>(), fat_type{&value});
    ASSERT_EQ(std::as_const(any).cast<fat_type>(), fat_type{&value});
    ASSERT_EQ(any, meta::any{fat_type{&value}});
    ASSERT_NE(fat_type{}, any);
}

TEST_F(Meta, MetaAnyVoidInPlaceTypeConstruction) {
    meta::any any{std::in_place_type<void>};

    ASSERT_TRUE(any);
    ASSERT_FALSE(any.try_cast<char>());
    ASSERT_EQ(any.data(), nullptr);
    ASSERT_EQ(std::as_const(any).data(), nullptr);
    ASSERT_EQ(any.type(), meta::resolve<void>());
    ASSERT_EQ(any, meta::any{std::in_place_type<void>});
    ASSERT_NE(meta::any{3}, any);
}

TEST_F(Meta, MetaAnyVoidCopyConstruction) {
    meta::any any{std::in_place_type<void>};
    meta::any other{any};

    ASSERT_TRUE(any);
    ASSERT_TRUE(other);
    ASSERT_EQ(any.type(), meta::resolve<void>());
    ASSERT_EQ(other, meta::any{std::in_place_type<void>});
}

TEST_F(Meta, MetaAnyVoidCopyAssignment) {
    meta::any any{std::in_place_type<void>};
    meta::any other{std::in_place_type<void>};

    other = any;

    ASSERT_TRUE(any);
    ASSERT_TRUE(other);
    ASSERT_EQ(any.type(), meta::resolve<void>());
    ASSERT_EQ(other, meta::any{std::in_place_type<void>});
}

TEST_F(Meta, MetaAnyVoidMoveConstruction) {
    meta::any any{std::in_place_type<void>};
    meta::any other{std::move(any)};

    ASSERT_FALSE(any);
    ASSERT_TRUE(other);
    ASSERT_EQ(other.type(), meta::resolve<void>());
    ASSERT_EQ(other, meta::any{std::in_place_type<void>});
}

TEST_F(Meta, MetaAnyVoidMoveAssignment) {
    meta::any any{std::in_place_type<void>};
    meta::any other{std::in_place_type<void>};

    other = std::move(any);

    ASSERT_FALSE(any);
    ASSERT_TRUE(other);
    ASSERT_EQ(other.type(), meta::resolve<void>());
    ASSERT_EQ(other, meta::any{std::in_place_type<void>});
}

TEST_F(Meta, MetaAnySBOMoveInvalidate) {
    meta::any any{42};
    meta::any other{std::move(any)};
    meta::any valid = std::move(other);

    ASSERT_FALSE(any);
    ASSERT_FALSE(other);
    ASSERT_TRUE(valid);
}

TEST_F(Meta, MetaAnyNoSBOMoveInvalidate) {
    int value = 42;
    fat_type instance{&value};
    meta::any any{instance};
    meta::any other{std::move(any)};
    meta::any valid = std::move(other);

    ASSERT_FALSE(any);
    ASSERT_FALSE(other);
    ASSERT_TRUE(valid);
}

TEST_F(Meta, MetaAnyVoidMoveInvalidate) {
    meta::any any{std::in_place_type<void>};
    meta::any other{std::move(any)};
    meta::any valid = std::move(other);

    ASSERT_FALSE(any);
    ASSERT_FALSE(other);
    ASSERT_TRUE(valid);
}

TEST_F(Meta, MetaAnySBODestruction) {
    ASSERT_EQ(empty_type::counter, 0);
    { meta::any any{empty_type{}}; }
    ASSERT_EQ(empty_type::counter, 1);
}

TEST_F(Meta, MetaAnyNoSBODestruction) {
    ASSERT_EQ(fat_type::counter, 0);
    { meta::any any{fat_type{}}; }
    ASSERT_EQ(fat_type::counter, 1);
}

TEST_F(Meta, MetaAnyVoidDestruction) {
    // just let asan tell us if everything is ok here
    [[maybe_unused]] meta::any any{std::in_place_type<void>};
}

TEST_F(Meta, MetaAnyEmplace) {
    meta::any any{};
    any.emplace<int>(42);

    ASSERT_TRUE(any);
    ASSERT_FALSE(any.try_cast<std::size_t>());
    ASSERT_TRUE(any.try_cast<int>());
    ASSERT_EQ(any.cast<int>(), 42);
    ASSERT_EQ(std::as_const(any).cast<int>(), 42);
    ASSERT_NE(any.data(), nullptr);
    ASSERT_NE(std::as_const(any).data(), nullptr);
    ASSERT_EQ(any, (meta::any{std::in_place_type<int>, 42}));
    ASSERT_EQ(any, meta::any{42});
    ASSERT_NE(meta::any{3}, any);
}

TEST_F(Meta, MetaAnyEmplaceVoid) {
    meta::any any{};
    any.emplace<void>();

    ASSERT_TRUE(any);
    ASSERT_EQ(any.data(), nullptr);
    ASSERT_EQ(std::as_const(any).data(), nullptr);
    ASSERT_EQ(any.type(), meta::resolve<void>());
    ASSERT_EQ(any, (meta::any{std::in_place_type<void>}));
}

TEST_F(Meta, MetaAnySBOSwap) {
    meta::any lhs{'c'};
    meta::any rhs{42};

    std::swap(lhs, rhs);

    ASSERT_TRUE(lhs.try_cast<int>());
    ASSERT_EQ(lhs.cast<int>(), 42);
    ASSERT_TRUE(rhs.try_cast<char>());
    ASSERT_EQ(rhs.cast<char>(), 'c');
}

TEST_F(Meta, MetaAnyNoSBOSwap) {
    int i, j;
    meta::any lhs{fat_type{&i}};
    meta::any rhs{fat_type{&j}};

    std::swap(lhs, rhs);

    ASSERT_EQ(lhs.cast<fat_type>().foo, &j);
    ASSERT_EQ(rhs.cast<fat_type>().bar, &i);
}

TEST_F(Meta, MetaAnyVoidSwap) {
    meta::any lhs{std::in_place_type<void>};
    meta::any rhs{std::in_place_type<void>};
    const auto *pre = lhs.data();

    std::swap(lhs, rhs);

    ASSERT_EQ(pre, lhs.data());
}

TEST_F(Meta, MetaAnySBOWithNoSBOSwap) {
    int value = 42;
    meta::any lhs{fat_type{&value}};
    meta::any rhs{'c'};

    std::swap(lhs, rhs);

    ASSERT_TRUE(lhs.try_cast<char>());
    ASSERT_EQ(lhs.cast<char>(), 'c');
    ASSERT_TRUE(rhs.try_cast<fat_type>());
    ASSERT_EQ(rhs.cast<fat_type>().foo, &value);
    ASSERT_EQ(rhs.cast<fat_type>().bar, &value);
}

TEST_F(Meta, MetaAnySBOWithEmptySwap) {
    meta::any lhs{'c'};
    meta::any rhs{};

    std::swap(lhs, rhs);

    ASSERT_FALSE(lhs);
    ASSERT_TRUE(rhs.try_cast<char>());
    ASSERT_EQ(rhs.cast<char>(), 'c');

    std::swap(lhs, rhs);

    ASSERT_FALSE(rhs);
    ASSERT_TRUE(lhs.try_cast<char>());
    ASSERT_EQ(lhs.cast<char>(), 'c');
}

TEST_F(Meta, MetaAnySBOWithVoidSwap) {
    meta::any lhs{'c'};
    meta::any rhs{std::in_place_type<void>};

    std::swap(lhs, rhs);

    ASSERT_EQ(lhs.type(), meta::resolve<void>());
    ASSERT_TRUE(rhs.try_cast<char>());
    ASSERT_EQ(rhs.cast<char>(), 'c');
}

TEST_F(Meta, MetaAnyNoSBOWithEmptySwap) {
    int i;
    meta::any lhs{fat_type{&i}};
    meta::any rhs{};

    std::swap(lhs, rhs);

    ASSERT_EQ(rhs.cast<fat_type>().bar, &i);

    std::swap(lhs, rhs);

    ASSERT_EQ(lhs.cast<fat_type>().bar, &i);
}

TEST_F(Meta, MetaAnyNoSBOWithVoidSwap) {
    int i;
    meta::any lhs{fat_type{&i}};
    meta::any rhs{std::in_place_type<void>};

    std::swap(lhs, rhs);

    ASSERT_EQ(rhs.cast<fat_type>().bar, &i);

    std::swap(lhs, rhs);

    ASSERT_EQ(lhs.cast<fat_type>().bar, &i);
}

TEST_F(Meta, MetaAnyComparable) {
    meta::any any{'c'};

    ASSERT_EQ(any, any);
    ASSERT_EQ(any, meta::any{'c'});
    ASSERT_NE(meta::any{'a'}, any);
    ASSERT_NE(any, meta::any{});

    ASSERT_TRUE(any == any);
    ASSERT_TRUE(any == meta::any{'c'});
    ASSERT_FALSE(any == meta::any{'a'});
    ASSERT_TRUE(any != meta::any{'a'});
    ASSERT_TRUE(any != meta::any{});
}

TEST_F(Meta, MetaAnyNotComparable) {
    meta::any any{not_comparable_type{}};

    ASSERT_EQ(any, any);
    ASSERT_NE(any, meta::any{not_comparable_type{}});
    ASSERT_NE(meta::any{}, any);

    ASSERT_TRUE(any == any);
    ASSERT_FALSE(any == meta::any{not_comparable_type{}});
    ASSERT_TRUE(any != meta::any{});
}

TEST_F(Meta, MetaAnyCompareVoid) {
    meta::any any{std::in_place_type<void>};

    ASSERT_EQ(any, any);
    ASSERT_EQ(any, meta::any{std::in_place_type<void>});
    ASSERT_NE(meta::any{'a'}, any);
    ASSERT_NE(any, meta::any{});

    ASSERT_TRUE(any == any);
    ASSERT_TRUE(any == meta::any{std::in_place_type<void>});
    ASSERT_FALSE(any == meta::any{'a'});
    ASSERT_TRUE(any != meta::any{'a'});
    ASSERT_TRUE(any != meta::any{});
}

TEST_F(Meta, MetaAnyTryCast) {
    meta::any any{derived_type{}};

    ASSERT_TRUE(any);
    ASSERT_EQ(any.type(), meta::resolve<derived_type>());
    ASSERT_EQ(any.try_cast<void>(), nullptr);
    ASSERT_NE(any.try_cast<base_type>(), nullptr);
    ASSERT_EQ(any.try_cast<derived_type>(), any.data());
    ASSERT_EQ(std::as_const(any).try_cast<base_type>(), any.try_cast<base_type>());
    ASSERT_EQ(std::as_const(any).try_cast<derived_type>(), any.data());
}

TEST_F(Meta, MetaAnyCast) {
    meta::any any{derived_type{}};

    ASSERT_TRUE(any);
    ASSERT_EQ(any.type(), meta::resolve<derived_type>());
    ASSERT_EQ(any.try_cast<std::size_t>(), nullptr);
    ASSERT_NE(any.try_cast<base_type>(), nullptr);
    ASSERT_EQ(any.try_cast<derived_type>(), any.data());
    ASSERT_EQ(std::as_const(any).try_cast<base_type>(), any.try_cast<base_type>());
    ASSERT_EQ(std::as_const(any).try_cast<derived_type>(), any.data());
}

TEST_F(Meta, MetaAnyConvert) {
    meta::any any{42.};

    ASSERT_TRUE(any);
    ASSERT_EQ(any.type(), meta::resolve<double>());
    ASSERT_TRUE(any.convert<double>());
    ASSERT_FALSE(any.convert<char>());
    ASSERT_EQ(any.type(), meta::resolve<double>());
    ASSERT_EQ(any.cast<double>(), 42.);
    ASSERT_TRUE(any.convert<int>());
    ASSERT_EQ(any.type(), meta::resolve<int>());
    ASSERT_EQ(any.cast<int>(), 42);
}

TEST_F(Meta, MetaAnyConstConvert) {
    const meta::any any{42.};

    ASSERT_TRUE(any);
    ASSERT_EQ(any.type(), meta::resolve<double>());
    ASSERT_TRUE(any.convert<double>());
    ASSERT_FALSE(any.convert<char>());
    ASSERT_EQ(any.type(), meta::resolve<double>());
    ASSERT_EQ(any.cast<double>(), 42.);

    auto other = any.convert<int>();

    ASSERT_EQ(any.type(), meta::resolve<double>());
    ASSERT_EQ(any.cast<double>(), 42.);
    ASSERT_EQ(other.type(), meta::resolve<int>());
    ASSERT_EQ(other.cast<int>(), 42);
}

TEST_F(Meta, MetaHandleFromObject) {
    empty_type empty{};
    meta::handle handle{empty};

    ASSERT_TRUE(handle);
    ASSERT_EQ(handle.type(), meta::resolve<empty_type>());
    ASSERT_EQ(std::as_const(handle).data(), &empty);
    ASSERT_EQ(handle.data(), &empty);
}

TEST_F(Meta, MetaHandleFromMetaAny) {
    meta::any any{42};
    meta::handle handle{any};

    ASSERT_TRUE(handle);
    ASSERT_EQ(handle.type(), meta::resolve<int>());
    ASSERT_EQ(std::as_const(handle).data(), any.data());
    ASSERT_EQ(handle.data(), any.data());
}

TEST_F(Meta, MetaHandleEmpty) {
    meta::handle handle{};

    ASSERT_FALSE(handle);
    ASSERT_FALSE(handle.type());
    ASSERT_EQ(std::as_const(handle).data(), nullptr);
    ASSERT_EQ(handle.data(), nullptr);
}

TEST_F(Meta, MetaProp) {
    auto prop = meta::resolve<char>().prop(properties::prop_int);

    ASSERT_TRUE(prop);
    ASSERT_NE(prop, meta::prop{});
    ASSERT_EQ(prop.key(), properties::prop_int);
    ASSERT_EQ(prop.value(), 42);
}

TEST_F(Meta, MetaBase) {
    std::hash<std::string_view> hash{};
    auto base = meta::resolve<derived_type>().base(hash("base"));
    derived_type derived{};

    ASSERT_TRUE(base);
    ASSERT_NE(base, meta::base{});
    ASSERT_EQ(base.parent(), meta::resolve(hash("derived")));
    ASSERT_EQ(base.type(), meta::resolve<base_type>());
    ASSERT_EQ(base.cast(&derived), static_cast<base_type *>(&derived));
}

TEST_F(Meta, MetaConv) {
    auto conv = meta::resolve<double>().conv<int>();
    double value = 3.;

    ASSERT_TRUE(conv);
    ASSERT_NE(conv, meta::conv{});
    ASSERT_EQ(conv.parent(), meta::resolve<double>());
    ASSERT_EQ(conv.type(), meta::resolve<int>());

    auto any = conv.convert(&value);

    ASSERT_TRUE(any);
    ASSERT_EQ(any.type(), meta::resolve<int>());
    ASSERT_EQ(any.cast<int>(), 3);
}

TEST_F(Meta, MetaConvAsFreeFunctions) {
    auto conv = meta::resolve<derived_type>().conv<int>();
    derived_type derived{derived_type{}, 42, 'c'};

    ASSERT_TRUE(conv);
    ASSERT_NE(conv, meta::conv{});
    ASSERT_EQ(conv.parent(), meta::resolve<derived_type>());
    ASSERT_EQ(conv.type(), meta::resolve<int>());

    auto any = conv.convert(&derived);

    ASSERT_TRUE(any);
    ASSERT_EQ(any.type(), meta::resolve<int>());
    ASSERT_EQ(any.cast<int>(), 42);
}

TEST_F(Meta, MetaConvAsMemberFunctions) {
    auto conv = meta::resolve<derived_type>().conv<char>();
    derived_type derived{derived_type{}, 42, 'c'};

    ASSERT_TRUE(conv);
    ASSERT_NE(conv, meta::conv{});
    ASSERT_EQ(conv.parent(), meta::resolve<derived_type>());
    ASSERT_EQ(conv.type(), meta::resolve<char>());

    auto any = conv.convert(&derived);

    ASSERT_TRUE(any);
    ASSERT_EQ(any.type(), meta::resolve<char>());
    ASSERT_EQ(any.cast<char>(), 'c');
}

TEST_F(Meta, MetaCtor) {
    auto ctor = meta::resolve<derived_type>().ctor<const base_type &, int, char>();
    std::hash<std::string_view> hash{};

    ASSERT_TRUE(ctor);
    ASSERT_NE(ctor, meta::ctor{});
    ASSERT_EQ(ctor.parent(), meta::resolve(hash("derived")));
    ASSERT_EQ(ctor.size(), meta::ctor::size_type{3});
    ASSERT_EQ(ctor.arg(meta::ctor::size_type{0}), meta::resolve<base_type>());
    ASSERT_EQ(ctor.arg(meta::ctor::size_type{1}), meta::resolve<int>());
    ASSERT_EQ(ctor.arg(meta::ctor::size_type{2}), meta::resolve<char>());
    ASSERT_FALSE(ctor.arg(meta::ctor::size_type{3}));

    auto any = ctor.invoke(base_type{}, 42, 'c');
    auto empty = ctor.invoke();

    ASSERT_FALSE(empty);
    ASSERT_TRUE(any);
    ASSERT_TRUE(any.try_cast<derived_type>());
    ASSERT_EQ(any.cast<derived_type>().i, 42);
    ASSERT_EQ(any.cast<derived_type>().c, 'c');

    ctor.prop([](auto prop) {
        ASSERT_TRUE(prop);
        ASSERT_EQ(prop.key(), properties::prop_bool);
        ASSERT_FALSE(prop.value().template cast<bool>());
    });

    ASSERT_FALSE(ctor.prop(properties::prop_int));

    auto prop = ctor.prop(properties::prop_bool);

    ASSERT_TRUE(prop);
    ASSERT_EQ(prop.key(), properties::prop_bool);
    ASSERT_FALSE(prop.value().template cast<bool>());
}

TEST_F(Meta, MetaCtorFunc) {
    auto ctor = meta::resolve<derived_type>().ctor<const base_type &, int>();
    std::hash<std::string_view> hash{};

    ASSERT_TRUE(ctor);
    ASSERT_EQ(ctor.parent(), meta::resolve(hash("derived")));
    ASSERT_EQ(ctor.size(), meta::ctor::size_type{2});
    ASSERT_EQ(ctor.arg(meta::ctor::size_type{0}), meta::resolve<base_type>());
    ASSERT_EQ(ctor.arg(meta::ctor::size_type{1}), meta::resolve<int>());
    ASSERT_FALSE(ctor.arg(meta::ctor::size_type{2}));

    auto any = ctor.invoke(derived_type{}, 42);
    auto empty = ctor.invoke(3, 'c');

    ASSERT_FALSE(empty);
    ASSERT_TRUE(any);
    ASSERT_TRUE(any.try_cast<derived_type>());
    ASSERT_EQ(any.cast<derived_type>().i, 42);
    ASSERT_EQ(any.cast<derived_type>().c, 'c');

    ctor.prop([](auto prop) {
        ASSERT_TRUE(prop);
        ASSERT_EQ(prop.key(), properties::prop_int);
        ASSERT_EQ(prop.value(), 42);
    });

    ASSERT_FALSE(ctor.prop(properties::prop_bool));

    auto prop = ctor.prop(properties::prop_int);

    ASSERT_TRUE(prop);
    ASSERT_EQ(prop.key(), properties::prop_int);
    ASSERT_EQ(prop.value(), 42);
}

TEST_F(Meta, MetaCtorMetaAnyArgs) {
    auto ctor = meta::resolve<derived_type>().ctor<const base_type &, int, char>();
    auto any = ctor.invoke(base_type{}, meta::any{42}, meta::any{'c'});

    ASSERT_TRUE(any);
    ASSERT_TRUE(any.try_cast<derived_type>());
    ASSERT_EQ(any.cast<derived_type>().i, 42);
    ASSERT_EQ(any.cast<derived_type>().c, 'c');
}

TEST_F(Meta, MetaCtorInvalidArgs) {
    auto ctor = meta::resolve<derived_type>().ctor<const base_type &, int, char>();
    ASSERT_FALSE(ctor.invoke(base_type{}, meta::any{'c'}, meta::any{42}));
}

TEST_F(Meta, MetaCtorCastAndConvert) {
    auto ctor = meta::resolve<derived_type>().ctor<const base_type &, int, char>();
    auto any = ctor.invoke(meta::any{derived_type{}}, meta::any{42.}, meta::any{'c'});

    ASSERT_TRUE(any);
    ASSERT_TRUE(any.try_cast<derived_type>());
    ASSERT_EQ(any.cast<derived_type>().i, 42);
    ASSERT_EQ(any.cast<derived_type>().c, 'c');
}

TEST_F(Meta, MetaCtorFuncMetaAnyArgs) {
    auto ctor = meta::resolve<derived_type>().ctor<const base_type &, int>();
    auto any = ctor.invoke(base_type{}, meta::any{42});

    ASSERT_TRUE(any);
    ASSERT_TRUE(any.try_cast<derived_type>());
    ASSERT_EQ(any.cast<derived_type>().i, 42);
    ASSERT_EQ(any.cast<derived_type>().c, 'c');
}

TEST_F(Meta, MetaCtorFuncInvalidArgs) {
    auto ctor = meta::resolve<derived_type>().ctor<const base_type &, int>();
    ASSERT_FALSE(ctor.invoke(base_type{}, meta::any{'c'}));
}

TEST_F(Meta, MetaCtorFuncCastAndConvert) {
    auto ctor = meta::resolve<derived_type>().ctor<const base_type &, int>();
    auto any = ctor.invoke(meta::any{derived_type{}}, meta::any{42.});

    ASSERT_TRUE(any);
    ASSERT_TRUE(any.try_cast<derived_type>());
    ASSERT_EQ(any.cast<derived_type>().i, 42);
    ASSERT_EQ(any.cast<derived_type>().c, 'c');
}

TEST_F(Meta, MetaDtor) {
    std::hash<std::string_view> hash{};
    auto dtor = meta::resolve<empty_type>().dtor();
    empty_type empty{};

    ASSERT_TRUE(dtor);
    ASSERT_NE(dtor, meta::dtor{});
    ASSERT_EQ(dtor.parent(), meta::resolve(hash("empty")));
    ASSERT_EQ(empty_type::counter, 0);
    ASSERT_TRUE(dtor.invoke(empty));
    ASSERT_EQ(empty_type::counter, 1);
}

TEST_F(Meta, MetaDtorMetaAnyArg) {
    auto dtor = meta::resolve<empty_type>().dtor();
    meta::any any{empty_type{}};

    ASSERT_EQ(empty_type::counter, 0);
    ASSERT_TRUE(dtor.invoke(any));
    ASSERT_EQ(empty_type::counter, 1);
}

TEST_F(Meta, MetaDtorMetaAnyInvalidArg) {
    auto instance = 0;
    ASSERT_FALSE(meta::resolve<empty_type>().dtor().invoke(instance));
}


TEST_F(Meta, MetaData) {
    std::hash<std::string_view> hash{};
    auto data = meta::resolve<data_type>().data(hash("i"));
    data_type instance{};

    ASSERT_TRUE(data);
    ASSERT_NE(data, meta::data{});
    ASSERT_EQ(data.parent(), meta::resolve(hash("data")));
    ASSERT_EQ(data.type(), meta::resolve<int>());
    ASSERT_FALSE(data.is_const());
    ASSERT_FALSE(data.is_static());
    ASSERT_EQ(data.get(instance).cast<int>(), 0);
    ASSERT_TRUE(data.set(instance, 42));
    ASSERT_EQ(data.get(instance).cast<int>(), 42);

    data.prop([](auto prop) {
        ASSERT_TRUE(prop);
        ASSERT_EQ(prop.key(), properties::prop_int);
        ASSERT_EQ(prop.value(), 0);
    });

    ASSERT_FALSE(data.prop(properties::prop_bool));

    auto prop = data.prop(properties::prop_int);

    ASSERT_TRUE(prop);
    ASSERT_EQ(prop.key(), properties::prop_int);
    ASSERT_EQ(prop.value(), 0);
}

TEST_F(Meta, MetaDataConst) {
    std::hash<std::string_view> hash{};
    auto data = meta::resolve<data_type>().data(hash("j"));
    data_type instance{};

    ASSERT_TRUE(data);
    ASSERT_EQ(data.parent(), meta::resolve(hash("data")));
    ASSERT_EQ(data.type(), meta::resolve<int>());
    ASSERT_TRUE(data.is_const());
    ASSERT_FALSE(data.is_static());
    ASSERT_EQ(data.get(instance).cast<int>(), 1);
    ASSERT_FALSE(data.set(instance, 42));
    ASSERT_EQ(data.get(instance).cast<int>(), 1);

    data.prop([](auto prop) {
        ASSERT_TRUE(prop);
        ASSERT_EQ(prop.key(), properties::prop_int);
        ASSERT_EQ(prop.value(), 1);
    });

    ASSERT_FALSE(data.prop(properties::prop_bool));

    auto prop = data.prop(properties::prop_int);

    ASSERT_TRUE(prop);
    ASSERT_EQ(prop.key(), properties::prop_int);
    ASSERT_EQ(prop.value(), 1);
}

TEST_F(Meta, MetaDataStatic) {
    std::hash<std::string_view> hash{};
    auto data = meta::resolve<data_type>().data(hash("h"));

    ASSERT_TRUE(data);
    ASSERT_EQ(data.parent(), meta::resolve(hash("data")));
    ASSERT_EQ(data.type(), meta::resolve<int>());
    ASSERT_FALSE(data.is_const());
    ASSERT_TRUE(data.is_static());
    ASSERT_EQ(data.get({}).cast<int>(), 2);
    ASSERT_TRUE(data.set({}, 42));
    ASSERT_EQ(data.get({}).cast<int>(), 42);

    data.prop([](auto prop) {
        ASSERT_TRUE(prop);
        ASSERT_EQ(prop.key(), properties::prop_int);
        ASSERT_EQ(prop.value(), 2);
    });

    ASSERT_FALSE(data.prop(properties::prop_bool));

    auto prop = data.prop(properties::prop_int);

    ASSERT_TRUE(prop);
    ASSERT_EQ(prop.key(), properties::prop_int);
    ASSERT_EQ(prop.value(), 2);
}

TEST_F(Meta, MetaDataConstStatic) {
    std::hash<std::string_view> hash{};
    auto data = meta::resolve<data_type>().data(hash("k"));

    ASSERT_TRUE(data);
    ASSERT_EQ(data.parent(), meta::resolve(hash("data")));
    ASSERT_EQ(data.type(), meta::resolve<int>());
    ASSERT_TRUE(data.is_const());
    ASSERT_TRUE(data.is_static());
    ASSERT_EQ(data.get({}).cast<int>(), 3);
    ASSERT_FALSE(data.set({}, 42));
    ASSERT_EQ(data.get({}).cast<int>(), 3);

    data.prop([](auto prop) {
        ASSERT_TRUE(prop);
        ASSERT_EQ(prop.key(), properties::prop_int);
        ASSERT_EQ(prop.value(), 3);
    });

    ASSERT_FALSE(data.prop(properties::prop_bool));

    auto prop = data.prop(properties::prop_int);

    ASSERT_TRUE(prop);
    ASSERT_EQ(prop.key(), properties::prop_int);
    ASSERT_EQ(prop.value(), 3);
}

TEST_F(Meta, MetaDataGetMetaAnyArg) {
    std::hash<std::string_view> hash{};
    auto data = meta::resolve<data_type>().data(hash("i"));
    meta::any any{data_type{}};
    any.cast<data_type>().i = 99;
    const auto value = data.get(any);

    ASSERT_TRUE(value);
    ASSERT_TRUE(value.cast<int>());
    ASSERT_EQ(value.cast<int>(), 99);
}

TEST_F(Meta, MetaDataGetInvalidArg) {
    std::hash<std::string_view> hash{};
    auto instance = 0;
    ASSERT_FALSE(meta::resolve<data_type>().data(hash("i")).get(instance));
}

TEST_F(Meta, MetaDataSetMetaAnyArg) {
    std::hash<std::string_view> hash{};
    auto data = meta::resolve<data_type>().data(hash("i"));
    meta::any any{data_type{}};
    meta::any value{42};

    ASSERT_EQ(any.cast<data_type>().i, 0);
    ASSERT_TRUE(data.set(any, value));
    ASSERT_EQ(any.cast<data_type>().i, 42);
}

TEST_F(Meta, MetaDataSetInvalidArg) {
    std::hash<std::string_view> hash{};
    ASSERT_FALSE(meta::resolve<data_type>().data(hash("i")).set({}, 'c'));
}

TEST_F(Meta, MetaDataSetCast) {
    std::hash<std::string_view> hash{};
    auto data = meta::resolve<data_type>().data(hash("empty"));
    data_type instance{};

    ASSERT_EQ(empty_type::counter, 0);
    ASSERT_TRUE(data.set(instance, fat_type{}));
    ASSERT_EQ(empty_type::counter, 1);
}

TEST_F(Meta, MetaDataSetConvert) {
    std::hash<std::string_view> hash{};
    auto data = meta::resolve<data_type>().data(hash("i"));
    data_type instance{};

    ASSERT_EQ(instance.i, 0);
    ASSERT_TRUE(data.set(instance, 3.));
    ASSERT_EQ(instance.i, 3);
}

TEST_F(Meta, MetaDataSetterGetterAsFreeFunctions) {
    std::hash<std::string_view> hash{};
    auto data = meta::resolve<setter_getter_type>().data(hash("x"));
    setter_getter_type instance{};

    ASSERT_TRUE(data);
    ASSERT_NE(data, meta::data{});
    ASSERT_EQ(data.parent(), meta::resolve(hash("setter_getter")));
    ASSERT_EQ(data.type(), meta::resolve<int>());
    ASSERT_FALSE(data.is_const());
    ASSERT_FALSE(data.is_static());
    ASSERT_EQ(data.get(instance).cast<int>(), 0);
    ASSERT_TRUE(data.set(instance, 42));
    ASSERT_EQ(data.get(instance).cast<int>(), 42);
}

TEST_F(Meta, MetaDataSetterGetterAsMemberFunctions) {
    std::hash<std::string_view> hash{};
    auto data = meta::resolve<setter_getter_type>().data(hash("y"));
    setter_getter_type instance{};

    ASSERT_TRUE(data);
    ASSERT_NE(data, meta::data{});
    ASSERT_EQ(data.parent(), meta::resolve(hash("setter_getter")));
    ASSERT_EQ(data.type(), meta::resolve<int>());
    ASSERT_FALSE(data.is_const());
    ASSERT_FALSE(data.is_static());
    ASSERT_EQ(data.get(instance).cast<int>(), 0);
    ASSERT_TRUE(data.set(instance, 42));
    ASSERT_EQ(data.get(instance).cast<int>(), 42);
}

TEST_F(Meta, MetaDataSetterGetterWithRefAsMemberFunctions) {
    std::hash<std::string_view> hash{};
    auto data = meta::resolve<setter_getter_type>().data(hash("w"));
    setter_getter_type instance{};

    ASSERT_TRUE(data);
    ASSERT_NE(data, meta::data{});
    ASSERT_EQ(data.parent(), meta::resolve(hash("setter_getter")));
    ASSERT_EQ(data.type(), meta::resolve<int>());
    ASSERT_FALSE(data.is_const());
    ASSERT_FALSE(data.is_static());
    ASSERT_EQ(data.get(instance).cast<int>(), 0);
    ASSERT_TRUE(data.set(instance, 42));
    ASSERT_EQ(data.get(instance).cast<int>(), 42);
}

TEST_F(Meta, MetaDataSetterGetterMixed) {
    std::hash<std::string_view> hash{};
    auto data = meta::resolve<setter_getter_type>().data(hash("z"));
    setter_getter_type instance{};

    ASSERT_TRUE(data);
    ASSERT_NE(data, meta::data{});
    ASSERT_EQ(data.parent(), meta::resolve(hash("setter_getter")));
    ASSERT_EQ(data.type(), meta::resolve<int>());
    ASSERT_FALSE(data.is_const());
    ASSERT_FALSE(data.is_static());
    ASSERT_EQ(data.get(instance).cast<int>(), 0);
    ASSERT_TRUE(data.set(instance, 42));
    ASSERT_EQ(data.get(instance).cast<int>(), 42);
}

TEST_F(Meta, MetaDataArrayStatic) {
    std::hash<std::string_view> hash{};
    auto data = meta::resolve<array_type>().data(hash("global"));

    array_type::global[0] = 3;
    array_type::global[1] = 5;
    array_type::global[2] = 7;

    ASSERT_TRUE(data);
    ASSERT_NE(data, meta::data{});
    ASSERT_EQ(data.parent(), meta::resolve(hash("array")));
    ASSERT_EQ(data.type(), meta::resolve<int[3]>());
    ASSERT_FALSE(data.is_const());
    ASSERT_TRUE(data.is_static());
    ASSERT_TRUE(data.type().is_array());
    ASSERT_EQ(data.type().extent(), 3);
    ASSERT_EQ(data.get({}, 0).cast<int>(), 3);
    ASSERT_EQ(data.get({}, 1).cast<int>(), 5);
    ASSERT_EQ(data.get({}, 2).cast<int>(), 7);
    ASSERT_FALSE(data.set({}, 0, 'c'));
    ASSERT_EQ(data.get({}, 0).cast<int>(), 3);
    ASSERT_TRUE(data.set({}, 0, data.get({}, 0).cast<int>()+2));
    ASSERT_TRUE(data.set({}, 1, data.get({}, 1).cast<int>()+2));
    ASSERT_TRUE(data.set({}, 2, data.get({}, 2).cast<int>()+2));
    ASSERT_EQ(data.get({}, 0).cast<int>(), 5);
    ASSERT_EQ(data.get({}, 1).cast<int>(), 7);
    ASSERT_EQ(data.get({}, 2).cast<int>(), 9);
}

TEST_F(Meta, MetaDataArray) {
    std::hash<std::string_view> hash{};
    auto data = meta::resolve<array_type>().data(hash("local"));
    array_type instance;

    instance.local[0] = 3;
    instance.local[1] = 5;
    instance.local[2] = 7;

    ASSERT_TRUE(data);
    ASSERT_NE(data, meta::data{});
    ASSERT_EQ(data.parent(), meta::resolve(hash("array")));
    ASSERT_EQ(data.type(), meta::resolve<int[3]>());
    ASSERT_FALSE(data.is_const());
    ASSERT_FALSE(data.is_static());
    ASSERT_TRUE(data.type().is_array());
    ASSERT_EQ(data.type().extent(), 3);
    ASSERT_EQ(data.get(instance, 0).cast<int>(), 3);
    ASSERT_EQ(data.get(instance, 1).cast<int>(), 5);
    ASSERT_EQ(data.get(instance, 2).cast<int>(), 7);
    ASSERT_FALSE(data.set(instance, 0, 'c'));
    ASSERT_EQ(data.get(instance, 0).cast<int>(), 3);
    ASSERT_TRUE(data.set(instance, 0, data.get(instance, 0).cast<int>()+2));
    ASSERT_TRUE(data.set(instance, 1, data.get(instance, 1).cast<int>()+2));
    ASSERT_TRUE(data.set(instance, 2, data.get(instance, 2).cast<int>()+2));
    ASSERT_EQ(data.get(instance, 0).cast<int>(), 5);
    ASSERT_EQ(data.get(instance, 1).cast<int>(), 7);
    ASSERT_EQ(data.get(instance, 2).cast<int>(), 9);
}

TEST_F(Meta, MetaDataAsVoid) {
    std::hash<std::string_view> hash{};
    auto data = meta::resolve<data_type>().data(hash("v"));
    data_type instance{};

    ASSERT_TRUE(data.set(instance, 42));
    ASSERT_EQ(instance.v, 42);
    ASSERT_EQ(data.get(instance), meta::any{std::in_place_type<void>});
}

TEST_F(Meta, MetaDataAsAlias) {
    std::hash<std::string_view> hash{};
    data_type instance{};
    auto h_data = meta::resolve<data_type>().data(hash("h"));
    auto i_data = meta::resolve<data_type>().data(hash("i"));

    h_data.get(instance).cast<int>() = 3;
    i_data.get(instance).cast<int>() = 3;

    ASSERT_EQ(h_data.type(), meta::resolve<int>());
    ASSERT_EQ(i_data.type(), meta::resolve<int>());
    ASSERT_NE(instance.h, 3);
    ASSERT_EQ(instance.i, 3);
}

TEST_F(Meta, MetaFunc) {
    std::hash<std::string_view> hash{};
    auto func = meta::resolve<func_type>().func(hash("f2"));
    func_type instance{};

    ASSERT_TRUE(func);
    ASSERT_NE(func, meta::func{});
    ASSERT_EQ(func.parent(), meta::resolve(hash("func")));
    ASSERT_EQ(func.size(), meta::func::size_type{2});
    ASSERT_FALSE(func.is_const());
    ASSERT_FALSE(func.is_static());
    ASSERT_EQ(func.ret(), meta::resolve<int>());
    ASSERT_EQ(func.arg(meta::func::size_type{0}), meta::resolve<int>());
    ASSERT_EQ(func.arg(meta::func::size_type{1}), meta::resolve<int>());
    ASSERT_FALSE(func.arg(meta::func::size_type{2}));

    auto any = func.invoke(instance, 3, 2);
    auto empty = func.invoke(instance);

    ASSERT_FALSE(empty);
    ASSERT_TRUE(any);
    ASSERT_EQ(any.type(), meta::resolve<int>());
    ASSERT_EQ(any.cast<int>(), 4);
    ASSERT_EQ(func_type::value, 3);

    func.prop([](auto prop) {
        ASSERT_TRUE(prop);
        ASSERT_EQ(prop.key(), properties::prop_bool);
        ASSERT_FALSE(prop.value().template cast<bool>());
    });

    ASSERT_FALSE(func.prop(properties::prop_int));

    auto prop = func.prop(properties::prop_bool);

    ASSERT_TRUE(prop);
    ASSERT_EQ(prop.key(), properties::prop_bool);
    ASSERT_FALSE(prop.value().cast<bool>());
}

TEST_F(Meta, MetaFuncConst) {
    std::hash<std::string_view> hash{};
    auto func = meta::resolve<func_type>().func(hash("f1"));
    func_type instance{};

    ASSERT_TRUE(func);
    ASSERT_EQ(func.parent(), meta::resolve(hash("func")));
    ASSERT_EQ(func.size(), meta::func::size_type{1});
    ASSERT_TRUE(func.is_const());
    ASSERT_FALSE(func.is_static());
    ASSERT_EQ(func.ret(), meta::resolve<int>());
    ASSERT_EQ(func.arg(meta::func::size_type{0}), meta::resolve<int>());
    ASSERT_FALSE(func.arg(meta::func::size_type{1}));

    auto any = func.invoke(instance, 4);
    auto empty = func.invoke(instance, 'c');

    ASSERT_FALSE(empty);
    ASSERT_TRUE(any);
    ASSERT_EQ(any.type(), meta::resolve<int>());
    ASSERT_EQ(any.cast<int>(), 16);

    func.prop([](auto prop) {
        ASSERT_TRUE(prop);
        ASSERT_EQ(prop.key(), properties::prop_bool);
        ASSERT_FALSE(prop.value().template cast<bool>());
    });

    ASSERT_FALSE(func.prop(properties::prop_int));

    auto prop = func.prop(properties::prop_bool);

    ASSERT_TRUE(prop);
    ASSERT_EQ(prop.key(), properties::prop_bool);
    ASSERT_FALSE(prop.value().cast<bool>());
}

TEST_F(Meta, MetaFuncRetVoid) {
    std::hash<std::string_view> hash{};
    auto func = meta::resolve<func_type>().func(hash("g"));
    func_type instance{};

    ASSERT_TRUE(func);
    ASSERT_EQ(func.parent(), meta::resolve(hash("func")));
    ASSERT_EQ(func.size(), meta::func::size_type{1});
    ASSERT_FALSE(func.is_const());
    ASSERT_FALSE(func.is_static());
    ASSERT_EQ(func.ret(), meta::resolve<void>());
    ASSERT_EQ(func.arg(meta::func::size_type{0}), meta::resolve<int>());
    ASSERT_FALSE(func.arg(meta::func::size_type{1}));

    auto any = func.invoke(instance, 5);

    ASSERT_TRUE(any);
    ASSERT_EQ(any.type(), meta::resolve<void>());
    ASSERT_EQ(func_type::value, 25);

    func.prop([](auto prop) {
        ASSERT_TRUE(prop);
        ASSERT_EQ(prop.key(), properties::prop_bool);
        ASSERT_FALSE(prop.value().template cast<bool>());
    });

    ASSERT_FALSE(func.prop(properties::prop_int));

    auto prop = func.prop(properties::prop_bool);

    ASSERT_TRUE(prop);
    ASSERT_EQ(prop.key(), properties::prop_bool);
    ASSERT_FALSE(prop.value().cast<bool>());
}

TEST_F(Meta, MetaFuncStatic) {
    std::hash<std::string_view> hash{};
    auto func = meta::resolve<func_type>().func(hash("h"));
    func_type::value = 2;

    ASSERT_TRUE(func);
    ASSERT_EQ(func.parent(), meta::resolve(hash("func")));
    ASSERT_EQ(func.size(), meta::func::size_type{1});
    ASSERT_FALSE(func.is_const());
    ASSERT_TRUE(func.is_static());
    ASSERT_EQ(func.ret(), meta::resolve<int>());
    ASSERT_EQ(func.arg(meta::func::size_type{0}), meta::resolve<int>());
    ASSERT_FALSE(func.arg(meta::func::size_type{1}));

    auto any = func.invoke({}, 3);
    auto empty = func.invoke({}, 'c');

    ASSERT_FALSE(empty);
    ASSERT_TRUE(any);
    ASSERT_EQ(any.type(), meta::resolve<int>());
    ASSERT_EQ(any.cast<int>(), 6);

    func.prop([](auto prop) {
        ASSERT_TRUE(prop);
        ASSERT_EQ(prop.key(), properties::prop_bool);
        ASSERT_FALSE(prop.value().template cast<bool>());
    });

    ASSERT_FALSE(func.prop(properties::prop_int));

    auto prop = func.prop(properties::prop_bool);

    ASSERT_TRUE(prop);
    ASSERT_EQ(prop.key(), properties::prop_bool);
    ASSERT_FALSE(prop.value().cast<bool>());
}

TEST_F(Meta, MetaFuncStaticRetVoid) {
    std::hash<std::string_view> hash{};
    auto func = meta::resolve<func_type>().func(hash("k"));

    ASSERT_TRUE(func);
    ASSERT_EQ(func.parent(), meta::resolve(hash("func")));
    ASSERT_EQ(func.size(), meta::func::size_type{1});
    ASSERT_FALSE(func.is_const());
    ASSERT_TRUE(func.is_static());
    ASSERT_EQ(func.ret(), meta::resolve<void>());
    ASSERT_EQ(func.arg(meta::func::size_type{0}), meta::resolve<int>());
    ASSERT_FALSE(func.arg(meta::func::size_type{1}));

    auto any = func.invoke({}, 42);

    ASSERT_TRUE(any);
    ASSERT_EQ(any.type(), meta::resolve<void>());
    ASSERT_EQ(func_type::value, 42);

    func.prop([](auto *prop) {
        ASSERT_TRUE(prop);
        ASSERT_EQ(prop->key(), properties::prop_bool);
        ASSERT_FALSE(prop->value().template cast<bool>());
    });

    ASSERT_FALSE(func.prop(properties::prop_int));

    auto prop = func.prop(properties::prop_bool);

    ASSERT_TRUE(prop);
    ASSERT_EQ(prop.key(), properties::prop_bool);
    ASSERT_FALSE(prop.value().cast<bool>());
}

TEST_F(Meta, MetaFuncMetaAnyArgs) {
    std::hash<std::string_view> hash{};
    auto func = meta::resolve<func_type>().func(hash("f1"));
    func_type instance;

    auto any = func.invoke(instance, meta::any{3});

    ASSERT_TRUE(any);
    ASSERT_EQ(any.type(), meta::resolve<int>());
    ASSERT_EQ(any.cast<int>(), 9);
}

TEST_F(Meta, MetaFuncInvalidArgs) {
    std::hash<std::string_view> hash{};
    auto func = meta::resolve<func_type>().func(hash("f1"));
    empty_type instance;

    ASSERT_FALSE(func.invoke(instance, meta::any{'c'}));
}

TEST_F(Meta, MetaFuncCastAndConvert) {
    std::hash<std::string_view> hash{};
    auto func = meta::resolve<func_type>().func(hash("f3"));
    func_type instance;

    auto any = func.invoke(instance, derived_type{}, 0, 3.);

    ASSERT_TRUE(any);
    ASSERT_EQ(any.type(), meta::resolve<int>());
    ASSERT_EQ(any.cast<int>(), 9);
}

TEST_F(Meta, MetaFuncAsVoid) {
    std::hash<std::string_view> hash{};
    auto func = meta::resolve<func_type>().func(hash("v"));
    func_type instance{};

    ASSERT_EQ(func.invoke(instance, 42), meta::any{std::in_place_type<void>});
    ASSERT_EQ(func.ret(), meta::resolve<void>());
    ASSERT_EQ(instance.value, 42);
}

TEST_F(Meta, MetaFuncAsAlias) {
    std::hash<std::string_view> hash{};
    func_type instance{};
    auto func = meta::resolve<func_type>().func(hash("a"));
    func.invoke(instance).cast<int>() = 3;

    ASSERT_EQ(func.ret(), meta::resolve<int>());
    ASSERT_EQ(instance.value, 3);
}

TEST_F(Meta, MetaFuncByReference) {
    std::hash<std::string_view> hash{};
    auto func = meta::resolve<func_type>().func(hash("h"));
    func_type::value = 2;
    meta::any any{3};
    int value = 4;

    ASSERT_EQ(func.invoke({}, value).cast<int>(), 8);
    ASSERT_EQ(func.invoke({}, any).cast<int>(), 6);
    ASSERT_EQ(any.cast<int>(), 6);
    ASSERT_EQ(value, 8);
}

TEST_F(Meta, MetaType) {
    auto type = meta::resolve<derived_type>();

    ASSERT_TRUE(type);
    ASSERT_NE(type, meta::type{});

    type.prop([](auto prop) {
        ASSERT_TRUE(prop);
        ASSERT_EQ(prop.key(), properties::prop_int);
        ASSERT_EQ(prop.value(), 99);
    });

    ASSERT_FALSE(type.prop(properties::prop_bool));

    auto prop = type.prop(properties::prop_int);

    ASSERT_TRUE(prop);
    ASSERT_EQ(prop.key(), properties::prop_int);
    ASSERT_EQ(prop.value(), 99);
}

TEST_F(Meta, MetaTypeTraits) {
    ASSERT_TRUE(meta::resolve<void>().is_void());
    ASSERT_TRUE(meta::resolve<bool>().is_integral());
    ASSERT_TRUE(meta::resolve<double>().is_floating_point());
    ASSERT_TRUE(meta::resolve<properties>().is_enum());
    ASSERT_TRUE(meta::resolve<union_type>().is_union());
    ASSERT_TRUE(meta::resolve<derived_type>().is_class());
    ASSERT_TRUE(meta::resolve<int *>().is_pointer());
    ASSERT_TRUE(meta::resolve<decltype(&empty_type::destroy)>().is_function_pointer());
    ASSERT_TRUE(meta::resolve<decltype(&data_type::i)>().is_member_object_pointer());
    ASSERT_TRUE(meta::resolve<decltype(&func_type::g)>().is_member_function_pointer());
}

TEST_F(Meta, MetaTypeRemovePointer) {
    ASSERT_EQ(meta::resolve<void *>().remove_pointer(), meta::resolve<void>());
    ASSERT_EQ(meta::resolve<int(*)(char, double)>().remove_pointer(), meta::resolve<int(char, double)>());
    ASSERT_EQ(meta::resolve<derived_type>().remove_pointer(), meta::resolve<derived_type>());
}

TEST_F(Meta, MetaTypeBase) {
    std::hash<std::string_view> hash{};
    auto type = meta::resolve<derived_type>();
    bool iterate = false;

    type.base([&iterate](auto base) {
        ASSERT_EQ(base.type(), meta::resolve<base_type>());
        iterate = true;
    });

    ASSERT_TRUE(iterate);
    ASSERT_EQ(type.base(hash("base")).type(), meta::resolve<base_type>());
}

TEST_F(Meta, MetaTypeConv) {
    auto type = meta::resolve<double>();
    bool iterate = false;

    type.conv([&iterate](auto conv) {
        ASSERT_EQ(conv.type(), meta::resolve<int>());
        iterate = true;
    });

    ASSERT_TRUE(iterate);

    auto conv = type.conv<int>();

    ASSERT_EQ(conv.type(), meta::resolve<int>());
    ASSERT_FALSE(type.conv<char>());
}

TEST_F(Meta, MetaTypeCtor) {
    auto type = meta::resolve<derived_type>();
    int counter{};

    type.ctor([&counter](auto) {
        ++counter;
    });

    ASSERT_EQ(counter, 2);
    ASSERT_TRUE((type.ctor<const base_type &, int>()));
    ASSERT_TRUE((type.ctor<const derived_type &, double>()));
}

TEST_F(Meta, MetaTypeDtor) {
    ASSERT_TRUE(meta::resolve<fat_type>().dtor());
    ASSERT_FALSE(meta::resolve<int>().dtor());
}

TEST_F(Meta, MetaTypeData) {
    std::hash<std::string_view> hash{};
    auto type = meta::resolve<data_type>();
    int counter{};

    type.data([&counter](auto) {
        ++counter;
    });

    ASSERT_EQ(counter, 6);
    ASSERT_TRUE(type.data(hash("i")));
}

TEST_F(Meta, MetaTypeFunc) {
    std::hash<std::string_view> hash{};
    auto type = meta::resolve<func_type>();
    int counter{};

    type.func([&counter](auto) {
        ++counter;
    });

    ASSERT_EQ(counter, 8);
    ASSERT_TRUE(type.func(hash("f1")));
}

TEST_F(Meta, MetaTypeConstruct) {
    auto type = meta::resolve<derived_type>();
    auto any = type.construct(base_type{}, 42, 'c');

    ASSERT_TRUE(any);
    ASSERT_TRUE(any.try_cast<derived_type>());
    ASSERT_EQ(any.cast<derived_type>().i, 42);
    ASSERT_EQ(any.cast<derived_type>().c, 'c');
}

TEST_F(Meta, MetaTypeConstructMetaAnyArgs) {
    auto type = meta::resolve<derived_type>();
    auto any = type.construct(meta::any{base_type{}}, meta::any{42}, meta::any{'c'});

    ASSERT_TRUE(any);
    ASSERT_TRUE(any.try_cast<derived_type>());
    ASSERT_EQ(any.cast<derived_type>().i, 42);
    ASSERT_EQ(any.cast<derived_type>().c, 'c');
}

TEST_F(Meta, MetaTypeConstructInvalidArgs) {
    auto type = meta::resolve<derived_type>();
    auto any = type.construct(meta::any{base_type{}}, meta::any{'c'}, meta::any{42});
    ASSERT_FALSE(any);
}

TEST_F(Meta, MetaTypeLessArgs) {
    auto type = meta::resolve<derived_type>();
    auto any = type.construct(base_type{});
    ASSERT_FALSE(any);
}

TEST_F(Meta, MetaTypeConstructCastAndConvert) {
    auto type = meta::resolve<derived_type>();
    auto any = type.construct(meta::any{derived_type{}}, meta::any{42.}, meta::any{'c'});

    ASSERT_TRUE(any);
    ASSERT_TRUE(any.try_cast<derived_type>());
    ASSERT_EQ(any.cast<derived_type>().i, 42);
    ASSERT_EQ(any.cast<derived_type>().c, 'c');
}

TEST_F(Meta, MetaTypeDestroyDtor) {
    auto type = meta::resolve<empty_type>();
    empty_type instance;

    ASSERT_EQ(empty_type::counter, 0);
    ASSERT_TRUE(type.destroy(instance));
    ASSERT_EQ(empty_type::counter, 1);
}

TEST_F(Meta, MetaTypeDestroyDtorInvalidArg) {
    auto type = meta::resolve<empty_type>();
    auto instance = 'c';

    ASSERT_EQ(empty_type::counter, 0);
    ASSERT_FALSE(type.destroy(instance));
    ASSERT_EQ(empty_type::counter, 0);
}

TEST_F(Meta, MetaTypeDestroyDtorCastAndConvert) {
    auto type = meta::resolve<empty_type>();
    fat_type instance{};

    ASSERT_EQ(empty_type::counter, 0);
    ASSERT_FALSE(type.destroy(instance));
    ASSERT_EQ(empty_type::counter, 0);
}

TEST_F(Meta, MetaTypeDestroyNoDtor) {
    auto instance = 'c';
    ASSERT_TRUE(meta::resolve<char>().destroy(instance));
}

TEST_F(Meta, MetaTypeDestroyNoDtorInvalidArg) {
    auto instance = 42;
    ASSERT_FALSE(meta::resolve<char>().destroy(instance));
}

TEST_F(Meta, MetaTypeDestroyNoDtorVoid) {
    ASSERT_FALSE(meta::resolve<void>().destroy({}));
}

TEST_F(Meta, MetaTypeDestroyNoDtorCastAndConvert) {
    auto instance = 42.;
    ASSERT_FALSE(meta::resolve<int>().destroy(instance));
}

TEST_F(Meta, MetaDataFromBase) {
    std::hash<std::string_view> hash{};
    auto type = meta::resolve<concrete_type>();
    concrete_type instance;

    ASSERT_TRUE(type.data(hash("i")));
    ASSERT_TRUE(type.data(hash("j")));

    ASSERT_EQ(instance.i, 0);
    ASSERT_EQ(instance.j, char{});
    ASSERT_TRUE(type.data(hash("i")).set(instance, 3));
    ASSERT_TRUE(type.data(hash("j")).set(instance, 'c'));
    ASSERT_EQ(instance.i, 3);
    ASSERT_EQ(instance.j, 'c');
}

TEST_F(Meta, MetaFuncFromBase) {
    std::hash<std::string_view> hash{};
    auto type = meta::resolve<concrete_type>();
    auto base = meta::resolve<an_abstract_type>();
    concrete_type instance;

    ASSERT_TRUE(type.func(hash("f")));
    ASSERT_TRUE(type.func(hash("g")));
    ASSERT_TRUE(type.func(hash("h")));

    ASSERT_EQ(type.func(hash("f")).parent(), meta::resolve<concrete_type>());
    ASSERT_EQ(type.func(hash("g")).parent(), meta::resolve<an_abstract_type>());
    ASSERT_EQ(type.func(hash("h")).parent(), meta::resolve<another_abstract_type>());

    ASSERT_EQ(instance.i, 0);
    ASSERT_EQ(instance.j, char{});

    type.func(hash("f")).invoke(instance, 3);
    type.func(hash("h")).invoke(instance, 'c');

    ASSERT_EQ(instance.i, 9);
    ASSERT_EQ(instance.j, 'c');

    base.func(hash("g")).invoke(instance, 3);

    ASSERT_EQ(instance.i, -3);
}

TEST_F(Meta, MetaPropFromBase) {
    auto type = meta::resolve<concrete_type>();
    auto prop_bool = type.prop(properties::prop_bool);
    auto prop_int = type.prop(properties::prop_int);

    ASSERT_TRUE(prop_bool);
    ASSERT_TRUE(prop_int);

    ASSERT_FALSE(prop_bool.value().cast<bool>());
    ASSERT_EQ(prop_int.value().cast<int>(), 42);
}

TEST_F(Meta, AbstractClass) {
    std::hash<std::string_view> hash{};
    auto type = meta::resolve<an_abstract_type>();
    concrete_type instance;

    ASSERT_EQ(instance.i, 0);

    type.func(hash("f")).invoke(instance, 3);

    ASSERT_EQ(instance.i, 3);

    type.func(hash("g")).invoke(instance, 3);

    ASSERT_EQ(instance.i, -3);
}

TEST_F(Meta, EnumAndNamedConstants) {
    std::hash<std::string_view> hash{};
    auto type = meta::resolve<properties>();

    ASSERT_TRUE(type.data(hash("prop_bool")));
    ASSERT_TRUE(type.data(hash("prop_int")));

    ASSERT_EQ(type.data(hash("prop_bool")).type(), type);
    ASSERT_EQ(type.data(hash("prop_int")).type(), type);

    ASSERT_FALSE(type.data(hash("prop_bool")).set({}, properties::prop_int));
    ASSERT_FALSE(type.data(hash("prop_int")).set({}, properties::prop_bool));

    ASSERT_EQ(type.data(hash("prop_bool")).get({}).cast<properties>(), properties::prop_bool);
    ASSERT_EQ(type.data(hash("prop_int")).get({}).cast<properties>(), properties::prop_int);
}

TEST_F(Meta, ArithmeticTypeAndNamedConstants) {
    std::hash<std::string_view> hash{};
    auto type = meta::resolve<unsigned int>();

    ASSERT_TRUE(type.data(hash("min")));
    ASSERT_TRUE(type.data(hash("max")));

    ASSERT_EQ(type.data(hash("min")).type(), type);
    ASSERT_EQ(type.data(hash("max")).type(), type);

    ASSERT_FALSE(type.data(hash("min")).set({}, 100u));
    ASSERT_FALSE(type.data(hash("max")).set({}, 0u));

    ASSERT_EQ(type.data(hash("min")).get({}).cast<unsigned int>(), 0u);
    ASSERT_EQ(type.data(hash("max")).get({}).cast<unsigned int>(), 100u);
}

TEST_F(Meta, Variables) {
    std::hash<std::string_view> hash{};
    auto p_data = meta::resolve<properties>().data(hash("value"));
    auto c_data = meta::resolve(hash("char")).data(hash("value"));

    properties prop{properties::prop_int};
    char c = 'c';

    p_data.set(prop, properties::prop_bool);
    c_data.set(c, 'x');

    ASSERT_EQ(p_data.get(prop).cast<properties>(), properties::prop_bool);
    ASSERT_EQ(c_data.get(c).cast<char>(), 'x');
    ASSERT_EQ(prop, properties::prop_bool);
    ASSERT_EQ(c, 'x');
}

TEST_F(Meta, Unregister) {
    std::hash<std::string_view> hash{};

    ASSERT_FALSE(meta::unregister<float>());
    ASSERT_TRUE(meta::unregister<double>());
    ASSERT_TRUE(meta::unregister<char>());
    ASSERT_TRUE(meta::unregister<properties>());
    ASSERT_TRUE(meta::unregister<unsigned int>());
    ASSERT_TRUE(meta::unregister<base_type>());
    ASSERT_TRUE(meta::unregister<derived_type>());
    ASSERT_TRUE(meta::unregister<empty_type>());
    ASSERT_TRUE(meta::unregister<fat_type>());
    ASSERT_TRUE(meta::unregister<data_type>());
    ASSERT_TRUE(meta::unregister<func_type>());
    ASSERT_TRUE(meta::unregister<setter_getter_type>());
    ASSERT_TRUE(meta::unregister<an_abstract_type>());
    ASSERT_TRUE(meta::unregister<another_abstract_type>());
    ASSERT_TRUE(meta::unregister<concrete_type>());
    ASSERT_FALSE(meta::unregister<double>());

    ASSERT_FALSE(meta::resolve(hash("char")));
    ASSERT_FALSE(meta::resolve(hash("base")));
    ASSERT_FALSE(meta::resolve(hash("derived")));
    ASSERT_FALSE(meta::resolve(hash("empty")));
    ASSERT_FALSE(meta::resolve(hash("fat")));
    ASSERT_FALSE(meta::resolve(hash("data")));
    ASSERT_FALSE(meta::resolve(hash("func")));
    ASSERT_FALSE(meta::resolve(hash("setter_getter")));
    ASSERT_FALSE(meta::resolve(hash("an_abstract_type")));
    ASSERT_FALSE(meta::resolve(hash("another_abstract_type")));
    ASSERT_FALSE(meta::resolve(hash("concrete")));

    Meta::SetUpAfterUnregistration();
    meta::any any{42.};

    ASSERT_TRUE(any);
    ASSERT_FALSE(any.convert<int>());
    ASSERT_TRUE(any.convert<float>());

    ASSERT_FALSE(meta::resolve(hash("derived")));
    ASSERT_TRUE(meta::resolve(hash("my_type")));

    meta::resolve<derived_type>().prop([](auto prop) {
        ASSERT_TRUE(prop);
        ASSERT_EQ(prop.key(), properties::prop_bool);
        ASSERT_FALSE(prop.value().template cast<bool>());
    });

    ASSERT_FALSE((meta::resolve<derived_type>().ctor<const base_type &, int, char>()));
    ASSERT_TRUE((meta::resolve<derived_type>().ctor<>()));

    ASSERT_TRUE(meta::resolve(hash("your_type")).data(hash("a_data_member")));
    ASSERT_FALSE(meta::resolve(hash("your_type")).data(hash("another_data_member")));

    ASSERT_TRUE(meta::resolve(hash("your_type")).func(hash("a_member_function")));
    ASSERT_FALSE(meta::resolve(hash("your_type")).func(hash("another_member_function")));
}
