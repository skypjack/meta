#include <type_traits>
#include <gtest/gtest.h>
#include <meta/factory.hpp>
#include <meta/meta.hpp>

enum class properties {
    prop_int,
    prop_bool
};

struct empty_type {
    virtual ~empty_type() = default;

    static void destroy(empty_type *) {
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

bool operator!=(const fat_type &lhs, const fat_type &rhs) {
    return !(lhs == rhs);
}

struct base_type {
    virtual ~base_type() = default;
};

struct derived_type: base_type {
    derived_type() = default;
    derived_type(const base_type &, int i, char c): i{i}, c{c} {}

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
};

struct func_type {
    int f(const base_type &, int a, int b) { return f(a, b); }
    int f(int a, int b) { value = a; return b*b; }
    int f(int v) const { return v*v; }
    void g(int v) { value = v*v; }

    static int h(int v) { return v; }
    static void k(int v) { value = v; }

    inline static int value = 0;
};

struct setter_getter_type {
    int value{};

    int setter(int value) { return this->value = value; }
    int getter() { return value; }

    int setter_with_ref(const int &value) { return this->value = value; }
    const int & getter_with_ref() { return value; }

    static int static_setter(setter_getter_type *type, int value) { return type->value = value; }
    static int static_getter(const setter_getter_type *type) { return type->value; }
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

        meta::reflect<char>("char", std::make_pair(properties::prop_int, 42));

        meta::reflect<properties>()
                .data<properties::prop_bool>("prop_bool")
                .data<properties::prop_int>("prop_int");

        meta::reflect<unsigned int>().data<0u>("min").data<100u>("max");

        meta::reflect<base_type>("base");

        meta::reflect<derived_type>("derived", std::make_pair(properties::prop_int, 99))
                .base<base_type>()
                .ctor<const base_type &, int, char>(std::make_pair(properties::prop_bool, false))
                .ctor<&derived_factory>(std::make_pair(properties::prop_int, 42));

        meta::reflect<empty_type>("empty")
                .dtor<&empty_type::destroy>();

        meta::reflect<fat_type>("fat")
                .base<empty_type>()
                .dtor<&fat_type::destroy>();

        meta::reflect<data_type>("data")
                .data<&data_type::i>("i", std::make_pair(properties::prop_int, 0))
                .data<&data_type::j>("j", std::make_pair(properties::prop_int, 1))
                .data<&data_type::h>("h", std::make_pair(properties::prop_int, 2))
                .data<&data_type::k>("k", std::make_pair(properties::prop_int, 3))
                .data<&data_type::empty>("empty");

        meta::reflect<func_type>("func")
                .func<static_cast<int(func_type:: *)(const base_type &, int, int)>(&func_type::f)>("f3")
                .func<static_cast<int(func_type:: *)(int, int)>(&func_type::f)>("f2", std::make_pair(properties::prop_bool, false))
                .func<static_cast<int(func_type:: *)(int) const>(&func_type::f)>("f1", std::make_pair(properties::prop_bool, false))
                .func<&func_type::g>("g", std::make_pair(properties::prop_bool, false))
                .func<&func_type::h>("h", std::make_pair(properties::prop_bool, false))
                .func<&func_type::k>("k", std::make_pair(properties::prop_bool, false));

        meta::reflect<setter_getter_type>("setter_getter")
                .data<&setter_getter_type::static_setter, &setter_getter_type::static_getter>("x")
                .data<&setter_getter_type::setter, &setter_getter_type::getter>("y")
                .data<&setter_getter_type::static_setter, &setter_getter_type::getter>("z")
                .data<&setter_getter_type::setter_with_ref, &setter_getter_type::getter_with_ref>("w");

        meta::reflect<an_abstract_type>("an_abstract_type", std::make_pair(properties::prop_bool, false))
                .data<&an_abstract_type::i>("i")
                .func<&an_abstract_type::f>("f")
                .func<&an_abstract_type::g>("g");

        meta::reflect<another_abstract_type>("another_abstract_type", std::make_pair(properties::prop_int, 42))
                .data<&another_abstract_type::j>("j")
                .func<&another_abstract_type::h>("h");

        meta::reflect<concrete_type>("concrete")
                .base<an_abstract_type>()
                .base<another_abstract_type>()
                .func<&concrete_type::f>("f");
    }

    static void SetUpAfterUnregistration() {
        meta::reflect<double>().conv<float>();

        meta::reflect<derived_type>("my_type", std::make_pair(properties::prop_bool, false))
                .ctor<>();

        meta::reflect<another_abstract_type>("your_type")
                .data<&another_abstract_type::j>("a_data_member")
                .func<&another_abstract_type::h>("a_member_function");
    }

    void SetUp() override {
        empty_type::counter = 0;
        func_type::value = 0;
    }
};

TEST_F(Meta, Resolve) {
    ASSERT_EQ(meta::resolve<derived_type>(), meta::resolve("derived"));

    bool found = false;

    meta::resolve([&found](auto type) {
        found = found || type == meta::resolve<derived_type>();
    });

    ASSERT_TRUE(found);
}

TEST_F(Meta, MetaAnySBO) {
    meta::any any{'c'};

    ASSERT_TRUE(any);
    ASSERT_FALSE(any.can_cast<void>());
    ASSERT_TRUE(any.can_cast<char>());
    ASSERT_EQ(any.cast<char>(), 'c');
    ASSERT_EQ(std::as_const(any).cast<char>(), 'c');
    ASSERT_NE(any.data(), nullptr);
    ASSERT_NE(std::as_const(any).data(), nullptr);
    ASSERT_EQ(any, meta::any{'c'});
    ASSERT_NE(any, meta::any{'h'});
}

TEST_F(Meta, MetaAnyNoSBO) {
    int value = 42;
    fat_type instance{&value};
    meta::any any{instance};

    ASSERT_TRUE(any);
    ASSERT_FALSE(any.can_cast<void>());
    ASSERT_TRUE(any.can_cast<fat_type>());
    ASSERT_EQ(any.cast<fat_type>(), instance);
    ASSERT_EQ(std::as_const(any).cast<fat_type>(), instance);
    ASSERT_NE(any.data(), nullptr);
    ASSERT_NE(std::as_const(any).data(), nullptr);
    ASSERT_EQ(any, meta::any{instance});
    ASSERT_NE(any, fat_type{});
}

TEST_F(Meta, MetaAnyEmpty) {
    meta::any any{};

    ASSERT_FALSE(any);
    ASSERT_FALSE(any.type());
    ASSERT_FALSE(any.can_cast<void>());
    ASSERT_FALSE(any.can_cast<empty_type>());
    ASSERT_EQ(any.data(), nullptr);
    ASSERT_EQ(std::as_const(any).data(), nullptr);
    ASSERT_EQ(any, meta::any{});
    ASSERT_NE(any, meta::any{'c'});
}

TEST_F(Meta, MetaAnySBOCopyConstruction) {
    meta::any any{42};
    meta::any other{any};

    ASSERT_TRUE(any);
    ASSERT_TRUE(other);
    ASSERT_FALSE(other.can_cast<void>());
    ASSERT_TRUE(other.can_cast<int>());
    ASSERT_EQ(other.cast<int>(), 42);
    ASSERT_EQ(std::as_const(other).cast<int>(), 42);
    ASSERT_EQ(other, meta::any{42});
    ASSERT_NE(other, meta::any{0});
}

TEST_F(Meta, MetaAnySBOCopyAssignment) {
    meta::any any{42};
    meta::any other{};

    other = any;

    ASSERT_TRUE(any);
    ASSERT_TRUE(other);
    ASSERT_FALSE(other.can_cast<void>());
    ASSERT_TRUE(other.can_cast<int>());
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
    ASSERT_FALSE(other.can_cast<void>());
    ASSERT_TRUE(other.can_cast<int>());
    ASSERT_EQ(other.cast<int>(), 42);
    ASSERT_EQ(std::as_const(other).cast<int>(), 42);
    ASSERT_EQ(other, meta::any{42});
    ASSERT_NE(other, meta::any{0});
}

TEST_F(Meta, MetaAnySBOMoveAssignment) {
    meta::any any{42};
    meta::any other{};

    other = std::move(any);

    ASSERT_FALSE(any);
    ASSERT_TRUE(other);
    ASSERT_FALSE(other.can_cast<void>());
    ASSERT_TRUE(other.can_cast<int>());
    ASSERT_EQ(other.cast<int>(), 42);
    ASSERT_EQ(std::as_const(other).cast<int>(), 42);
    ASSERT_EQ(other, meta::any{42});
    ASSERT_NE(other, meta::any{0});
}

TEST_F(Meta, MetaAnyNoSBOCopyConstruction) {
    int value = 42;
    fat_type instance{&value};
    meta::any any{instance};
    meta::any other{any};

    ASSERT_TRUE(any);
    ASSERT_TRUE(other);
    ASSERT_FALSE(other.can_cast<void>());
    ASSERT_TRUE(other.can_cast<fat_type>());
    ASSERT_EQ(other.cast<fat_type>(), instance);
    ASSERT_EQ(std::as_const(other).cast<fat_type>(), instance);
    ASSERT_EQ(other, meta::any{instance});
    ASSERT_NE(other, fat_type{});
}

TEST_F(Meta, MetaAnyNoSBOCopyAssignment) {
    int value = 42;
    fat_type instance{&value};
    meta::any any{instance};
    meta::any other{};

    other = any;

    ASSERT_TRUE(any);
    ASSERT_TRUE(other);
    ASSERT_FALSE(other.can_cast<void>());
    ASSERT_TRUE(other.can_cast<fat_type>());
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
    ASSERT_FALSE(other.can_cast<void>());
    ASSERT_TRUE(other.can_cast<fat_type>());
    ASSERT_EQ(other.cast<fat_type>(), instance);
    ASSERT_EQ(std::as_const(other).cast<fat_type>(), instance);
    ASSERT_EQ(other, meta::any{instance});
    ASSERT_NE(other, fat_type{});
}

TEST_F(Meta, MetaAnyNoSBOMoveAssignment) {
    int value = 42;
    fat_type instance{&value};
    meta::any any{instance};
    meta::any other{};

    other = std::move(any);

    ASSERT_FALSE(any);
    ASSERT_TRUE(other);
    ASSERT_FALSE(other.can_cast<void>());
    ASSERT_TRUE(other.can_cast<fat_type>());
    ASSERT_EQ(other.cast<fat_type>(), instance);
    ASSERT_EQ(std::as_const(other).cast<fat_type>(), instance);
    ASSERT_EQ(other, meta::any{instance});
    ASSERT_NE(other, fat_type{});
}

TEST_F(Meta, MetaAnySBODestruction) {
    ASSERT_EQ(empty_type::counter, 0);
    meta::any any{empty_type{}};
    any = {};
    ASSERT_EQ(empty_type::counter, 1);
}

TEST_F(Meta, MetaAnyNoSBODestruction) {
    ASSERT_EQ(fat_type::counter, 0);
    meta::any any{fat_type{}};
    any = {};
    ASSERT_EQ(fat_type::counter, 1);
}

TEST_F(Meta, MetaAnySBOSwap) {
    meta::any lhs{'c'};
    meta::any rhs{42};

    std::swap(lhs, rhs);

    ASSERT_TRUE(lhs.can_cast<int>());
    ASSERT_EQ(lhs.cast<int>(), 42);
    ASSERT_TRUE(rhs.can_cast<char>());
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

TEST_F(Meta, MetaAnySBOWithNoSBOSwap) {
    int value = 42;
    meta::any lhs{fat_type{&value}};
    meta::any rhs{'c'};

    std::swap(lhs, rhs);

    ASSERT_TRUE(lhs.can_cast<char>());
    ASSERT_EQ(lhs.cast<char>(), 'c');
    ASSERT_TRUE(rhs.can_cast<fat_type>());
    ASSERT_EQ(rhs.cast<fat_type>().foo, &value);
    ASSERT_EQ(rhs.cast<fat_type>().bar, &value);
}

TEST_F(Meta, MetaAnyComparable) {
    meta::any any{'c'};

    ASSERT_EQ(any, any);
    ASSERT_EQ(any, meta::any{'c'});
    ASSERT_NE(any, meta::any{'a'});
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
    ASSERT_NE(any, meta::any{});

    ASSERT_TRUE(any == any);
    ASSERT_FALSE(any == meta::any{not_comparable_type{}});
    ASSERT_TRUE(any != meta::any{});
}

TEST_F(Meta, MetaAnyCast) {
    meta::any any{derived_type{}};
    meta::handle handle{any};

    ASSERT_TRUE(any);
    ASSERT_EQ(any.type(), meta::resolve<derived_type>());
    ASSERT_FALSE(any.can_cast<void>());
    ASSERT_TRUE(any.can_cast<base_type>());
    ASSERT_TRUE(any.can_cast<derived_type>());
    ASSERT_EQ(&any.cast<base_type>(), handle.try_cast<base_type>());
    ASSERT_EQ(&any.cast<derived_type>(), handle.try_cast<derived_type>());
    ASSERT_EQ(&std::as_const(any).cast<base_type>(), handle.try_cast<base_type>());
    ASSERT_EQ(&std::as_const(any).cast<derived_type>(), handle.try_cast<derived_type>());
}

TEST_F(Meta, MetaAnyConvert) {
    meta::any any{42.};

    ASSERT_TRUE(any);
    ASSERT_EQ(any.type(), meta::resolve<double>());
    ASSERT_FALSE(any.can_convert<char>());
    ASSERT_TRUE(any.can_convert<double>());
    ASSERT_TRUE(any.can_convert<int>());

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
    ASSERT_FALSE(any.can_convert<char>());
    ASSERT_TRUE(any.can_convert<double>());
    ASSERT_TRUE(any.can_convert<int>());

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
    ASSERT_EQ(handle.try_cast<void>(), nullptr);
    ASSERT_EQ(handle.try_cast<empty_type>(), &empty);
    ASSERT_EQ(std::as_const(handle).try_cast<empty_type>(), &empty);
    ASSERT_EQ(handle.data(), &empty);
    ASSERT_EQ(std::as_const(handle).data(), &empty);
}

TEST_F(Meta, MetaHandleFromMetaAny) {
    meta::any any{42};
    meta::handle handle{any};

    ASSERT_TRUE(handle);
    ASSERT_EQ(handle.type(), meta::resolve<int>());
    ASSERT_EQ(handle.try_cast<void>(), nullptr);
    ASSERT_EQ(handle.try_cast<int>(), any.data());
    ASSERT_EQ(std::as_const(handle).try_cast<int>(), any.data());
    ASSERT_EQ(handle.data(), any.data());
    ASSERT_EQ(std::as_const(handle).data(), any.data());
}

TEST_F(Meta, MetaHandleEmpty) {
    meta::handle handle{};

    ASSERT_FALSE(handle);
    ASSERT_FALSE(handle.type());
    ASSERT_EQ(handle.try_cast<void>(), nullptr);
    ASSERT_EQ(handle.try_cast<empty_type>(), nullptr);
    ASSERT_EQ(handle.data(), nullptr);
    ASSERT_EQ(std::as_const(handle).data(), nullptr);
}

TEST_F(Meta, MetaHandleTryCast) {
    derived_type derived{};
    base_type *base = &derived;
    meta::handle handle{derived};

    ASSERT_TRUE(handle);
    ASSERT_EQ(handle.type(), meta::resolve<derived_type>());
    ASSERT_EQ(handle.try_cast<void>(), nullptr);
    ASSERT_EQ(handle.try_cast<base_type>(), base);
    ASSERT_EQ(handle.try_cast<derived_type>(), &derived);
    ASSERT_EQ(std::as_const(handle).try_cast<base_type>(), base);
    ASSERT_EQ(std::as_const(handle).try_cast<derived_type>(), &derived);
    ASSERT_EQ(handle.data(), &derived);
    ASSERT_EQ(std::as_const(handle).data(), &derived);
}

TEST_F(Meta, MetaProp) {
    auto prop = meta::resolve<char>().prop(properties::prop_int);

    ASSERT_TRUE(prop);
    ASSERT_NE(prop, meta::prop{});
    ASSERT_EQ(prop.key(), properties::prop_int);
    ASSERT_EQ(prop.value(), 42);
}

TEST_F(Meta, MetaBase) {
    auto base = meta::resolve<derived_type>().base("base");
    derived_type derived{};

    ASSERT_TRUE(base);
    ASSERT_NE(base, meta::base{});
    ASSERT_EQ(base.parent(), meta::resolve("derived"));
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

TEST_F(Meta, MetaCtor) {
    auto ctor = meta::resolve<derived_type>().ctor<const base_type &, int, char>();

    ASSERT_TRUE(ctor);
    ASSERT_NE(ctor, meta::ctor{});
    ASSERT_EQ(ctor.parent(), meta::resolve("derived"));
    ASSERT_EQ(ctor.size(), meta::ctor::size_type{3});
    ASSERT_EQ(ctor.arg(meta::ctor::size_type{0}), meta::resolve<base_type>());
    ASSERT_EQ(ctor.arg(meta::ctor::size_type{1}), meta::resolve<int>());
    ASSERT_EQ(ctor.arg(meta::ctor::size_type{2}), meta::resolve<char>());
    ASSERT_FALSE(ctor.arg(meta::ctor::size_type{3}));

    auto any = ctor.invoke(base_type{}, 42, 'c');
    auto empty = ctor.invoke();

    ASSERT_FALSE(empty);
    ASSERT_TRUE(any);
    ASSERT_TRUE(any.can_cast<derived_type>());
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

    ASSERT_TRUE(ctor);
    ASSERT_EQ(ctor.parent(), meta::resolve("derived"));
    ASSERT_EQ(ctor.size(), meta::ctor::size_type{2});
    ASSERT_EQ(ctor.arg(meta::ctor::size_type{0}), meta::resolve<base_type>());
    ASSERT_EQ(ctor.arg(meta::ctor::size_type{1}), meta::resolve<int>());
    ASSERT_FALSE(ctor.arg(meta::ctor::size_type{2}));

    auto any = ctor.invoke(derived_type{}, 42);
    auto empty = ctor.invoke(3, 'c');

    ASSERT_FALSE(empty);
    ASSERT_TRUE(any);
    ASSERT_TRUE(any.can_cast<derived_type>());
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
    ASSERT_TRUE(any.can_cast<derived_type>());
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
    ASSERT_TRUE(any.can_cast<derived_type>());
    ASSERT_EQ(any.cast<derived_type>().i, 42);
    ASSERT_EQ(any.cast<derived_type>().c, 'c');
}

TEST_F(Meta, MetaCtorFuncMetaAnyArgs) {
    auto ctor = meta::resolve<derived_type>().ctor<const base_type &, int>();
    auto any = ctor.invoke(base_type{}, meta::any{42});

    ASSERT_TRUE(any);
    ASSERT_TRUE(any.can_cast<derived_type>());
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
    ASSERT_TRUE(any.can_cast<derived_type>());
    ASSERT_EQ(any.cast<derived_type>().i, 42);
    ASSERT_EQ(any.cast<derived_type>().c, 'c');
}

TEST_F(Meta, MetaDtor) {
    auto dtor = meta::resolve<empty_type>().dtor();
    empty_type empty{};

    ASSERT_TRUE(dtor);
    ASSERT_NE(dtor, meta::dtor{});
    ASSERT_EQ(dtor.parent(), meta::resolve("empty"));
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
    ASSERT_FALSE(meta::resolve<empty_type>().dtor().invoke(int{}));
}


TEST_F(Meta, MetaData) {
    auto data = meta::resolve<data_type>().data("i");
    data_type instance{};

    ASSERT_TRUE(data);
    ASSERT_NE(data, meta::data{});
    ASSERT_EQ(data.parent(), meta::resolve("data"));
    ASSERT_EQ(data.type(), meta::resolve<int>());
    ASSERT_STREQ(data.name(), "i");
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
    auto data = meta::resolve<data_type>().data("j");
    data_type instance{};

    ASSERT_TRUE(data);
    ASSERT_EQ(data.parent(), meta::resolve("data"));
    ASSERT_EQ(data.type(), meta::resolve<int>());
    ASSERT_STREQ(data.name(), "j");
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
    auto data = meta::resolve<data_type>().data("h");

    ASSERT_TRUE(data);
    ASSERT_EQ(data.parent(), meta::resolve("data"));
    ASSERT_EQ(data.type(), meta::resolve<int>());
    ASSERT_STREQ(data.name(), "h");
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
    auto data = meta::resolve<data_type>().data("k");

    ASSERT_TRUE(data);
    ASSERT_EQ(data.parent(), meta::resolve("data"));
    ASSERT_EQ(data.type(), meta::resolve<int>());
    ASSERT_STREQ(data.name(), "k");
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
    auto data = meta::resolve<data_type>().data("i");
    meta::any any{data_type{}};
    any.cast<data_type>().i = 99;
    const auto value = data.get(any);

    ASSERT_TRUE(value);
    ASSERT_TRUE(value.can_cast<int>());
    ASSERT_EQ(value.cast<int>(), 99);
}

TEST_F(Meta, MetaDataGetInvalidArg) {
    ASSERT_FALSE(meta::resolve<data_type>().data("i").get(0));
}

TEST_F(Meta, MetaDataSetMetaAnyArg) {
    auto data = meta::resolve<data_type>().data("i");
    meta::any any{data_type{}};
    meta::any value{42};

    ASSERT_EQ(any.cast<data_type>().i, 0);
    ASSERT_TRUE(data.set(any, value));
    ASSERT_EQ(any.cast<data_type>().i, 42);
}

TEST_F(Meta, MetaDataSetInvalidArg) {
    ASSERT_FALSE(meta::resolve<data_type>().data("i").set({}, 'c'));
}

TEST_F(Meta, MetaDataSetCast) {
    auto data = meta::resolve<data_type>().data("empty");
    data_type instance{};

    ASSERT_EQ(empty_type::counter, 0);
    ASSERT_TRUE(data.set(instance, fat_type{}));
    ASSERT_EQ(empty_type::counter, 1);
}

TEST_F(Meta, MetaDataSetConvert) {
    auto data = meta::resolve<data_type>().data("i");
    data_type instance{};

    ASSERT_EQ(instance.i, 0);
    ASSERT_TRUE(data.set(instance, 3.));
    ASSERT_EQ(instance.i, 3);
}

TEST_F(Meta, MetaDataSetterGetterAsFreeFunctions) {
    auto data = meta::resolve<setter_getter_type>().data("x");
    setter_getter_type instance{};

    ASSERT_TRUE(data);
    ASSERT_NE(data, meta::data{});
    ASSERT_EQ(data.parent(), meta::resolve("setter_getter"));
    ASSERT_EQ(data.type(), meta::resolve<int>());
    ASSERT_STREQ(data.name(), "x");
    ASSERT_FALSE(data.is_const());
    ASSERT_FALSE(data.is_static());
    ASSERT_EQ(data.get(instance).cast<int>(), 0);
    ASSERT_TRUE(data.set(instance, 42));
    ASSERT_EQ(data.get(instance).cast<int>(), 42);
}

TEST_F(Meta, MetaDataSetterGetterAsMemberFunctions) {
    auto data = meta::resolve<setter_getter_type>().data("y");
    setter_getter_type instance{};

    ASSERT_TRUE(data);
    ASSERT_NE(data, meta::data{});
    ASSERT_EQ(data.parent(), meta::resolve("setter_getter"));
    ASSERT_EQ(data.type(), meta::resolve<int>());
    ASSERT_STREQ(data.name(), "y");
    ASSERT_FALSE(data.is_const());
    ASSERT_FALSE(data.is_static());
    ASSERT_EQ(data.get(instance).cast<int>(), 0);
    ASSERT_TRUE(data.set(instance, 42));
    ASSERT_EQ(data.get(instance).cast<int>(), 42);
}

TEST_F(Meta, MetaDataSetterGetterWithRefAsMemberFunctions) {
    auto data = meta::resolve<setter_getter_type>().data("w");
    setter_getter_type instance{};

    ASSERT_TRUE(data);
    ASSERT_NE(data, meta::data{});
    ASSERT_EQ(data.parent(), meta::resolve("setter_getter"));
    ASSERT_EQ(data.type(), meta::resolve<int>());
    ASSERT_STREQ(data.name(), "w");
    ASSERT_FALSE(data.is_const());
    ASSERT_FALSE(data.is_static());
    ASSERT_EQ(data.get(instance).cast<int>(), 0);
    ASSERT_TRUE(data.set(instance, 42));
    ASSERT_EQ(data.get(instance).cast<int>(), 42);
}

TEST_F(Meta, MetaDataSetterGetterMixed) {
    auto data = meta::resolve<setter_getter_type>().data("z");
    setter_getter_type instance{};

    ASSERT_TRUE(data);
    ASSERT_NE(data, meta::data{});
    ASSERT_EQ(data.parent(), meta::resolve("setter_getter"));
    ASSERT_EQ(data.type(), meta::resolve<int>());
    ASSERT_STREQ(data.name(), "z");
    ASSERT_FALSE(data.is_const());
    ASSERT_FALSE(data.is_static());
    ASSERT_EQ(data.get(instance).cast<int>(), 0);
    ASSERT_TRUE(data.set(instance, 42));
    ASSERT_EQ(data.get(instance).cast<int>(), 42);
}

TEST_F(Meta, MetaFunc) {
    auto func = meta::resolve<func_type>().func("f2");
    func_type instance{};

    ASSERT_TRUE(func);
    ASSERT_NE(func, meta::func{});
    ASSERT_EQ(func.parent(), meta::resolve("func"));
    ASSERT_STREQ(func.name(), "f2");
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
    auto func = meta::resolve<func_type>().func("f1");
    func_type instance{};

    ASSERT_TRUE(func);
    ASSERT_EQ(func.parent(), meta::resolve("func"));
    ASSERT_STREQ(func.name(), "f1");
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
    auto func = meta::resolve<func_type>().func("g");
    func_type instance{};

    ASSERT_TRUE(func);
    ASSERT_EQ(func.parent(), meta::resolve("func"));
    ASSERT_STREQ(func.name(), "g");
    ASSERT_EQ(func.size(), meta::func::size_type{1});
    ASSERT_FALSE(func.is_const());
    ASSERT_FALSE(func.is_static());
    ASSERT_EQ(func.ret(), meta::resolve<void>());
    ASSERT_EQ(func.arg(meta::func::size_type{0}), meta::resolve<int>());
    ASSERT_FALSE(func.arg(meta::func::size_type{1}));

    auto any = func.invoke(instance, 5);

    ASSERT_FALSE(any);
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
    auto func = meta::resolve<func_type>().func("h");

    ASSERT_TRUE(func);
    ASSERT_EQ(func.parent(), meta::resolve("func"));
    ASSERT_STREQ(func.name(), "h");
    ASSERT_EQ(func.size(), meta::func::size_type{1});
    ASSERT_FALSE(func.is_const());
    ASSERT_TRUE(func.is_static());
    ASSERT_EQ(func.ret(), meta::resolve<int>());
    ASSERT_EQ(func.arg(meta::func::size_type{0}), meta::resolve<int>());
    ASSERT_FALSE(func.arg(meta::func::size_type{1}));

    auto any = func.invoke({}, 42);
    auto empty = func.invoke({}, 'c');

    ASSERT_FALSE(empty);
    ASSERT_TRUE(any);
    ASSERT_EQ(any.type(), meta::resolve<int>());
    ASSERT_EQ(any.cast<int>(), 42);

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
    auto func = meta::resolve<func_type>().func("k");

    ASSERT_TRUE(func);
    ASSERT_EQ(func.parent(), meta::resolve("func"));
    ASSERT_STREQ(func.name(), "k");
    ASSERT_EQ(func.size(), meta::func::size_type{1});
    ASSERT_FALSE(func.is_const());
    ASSERT_TRUE(func.is_static());
    ASSERT_EQ(func.ret(), meta::resolve<void>());
    ASSERT_EQ(func.arg(meta::func::size_type{0}), meta::resolve<int>());
    ASSERT_FALSE(func.arg(meta::func::size_type{1}));

    auto any = func.invoke({}, 42);

    ASSERT_FALSE(any);
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
    auto func = meta::resolve<func_type>().func("f1");
    auto any = func.invoke(func_type{}, meta::any{3});

    ASSERT_TRUE(any);
    ASSERT_EQ(any.type(), meta::resolve<int>());
    ASSERT_EQ(any.cast<int>(), 9);
}

TEST_F(Meta, MetaFuncInvalidArgs) {
    auto func = meta::resolve<func_type>().func("f1");
    ASSERT_FALSE(func.invoke(empty_type{}, meta::any{'c'}));
}

TEST_F(Meta, MetaFuncCastAndConvert) {
    auto func = meta::resolve<func_type>().func("f3");
    auto any = func.invoke(func_type{}, derived_type{}, 0, 3.);

    ASSERT_TRUE(any);
    ASSERT_EQ(any.type(), meta::resolve<int>());
    ASSERT_EQ(any.cast<int>(), 9);
}

TEST_F(Meta, MetaType) {
    auto type = meta::resolve<derived_type>();

    ASSERT_TRUE(type);
    ASSERT_NE(type, meta::type{});
    ASSERT_STREQ(type.name(), "derived");

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
    ASSERT_TRUE(meta::resolve<decltype(empty_type::destroy)>().is_function());
    ASSERT_TRUE(meta::resolve<decltype(&data_type::i)>().is_member_object_pointer());
    ASSERT_TRUE(meta::resolve<decltype(&func_type::g)>().is_member_function_pointer());
}

TEST_F(Meta, MetaTypeRemovePointer) {
    ASSERT_EQ(meta::resolve<void *>().remove_pointer(), meta::resolve<void>());
    ASSERT_EQ(meta::resolve<int(*)(char, double)>().remove_pointer(), meta::resolve<int(char, double)>());
    ASSERT_EQ(meta::resolve<derived_type>().remove_pointer(), meta::resolve<derived_type>());
}

TEST_F(Meta, MetaTypeBase) {
    auto type = meta::resolve<derived_type>();
    bool iterate = false;

    type.base([&iterate](auto base) {
        ASSERT_EQ(base.type(), meta::resolve<base_type>());
        iterate = true;
    });

    ASSERT_TRUE(iterate);
    ASSERT_EQ(type.base("base").type(), meta::resolve<base_type>());
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
    auto type = meta::resolve<data_type>();
    int counter{};

    type.data([&counter](auto) {
        ++counter;
    });

    ASSERT_EQ(counter, 5);
    ASSERT_TRUE(type.data("i"));
}

TEST_F(Meta, MetaTypeFunc) {
    auto type = meta::resolve<func_type>();
    int counter{};

    type.func([&counter](auto) {
        ++counter;
    });

    ASSERT_EQ(counter, 6);
    ASSERT_TRUE(type.func("f1"));
}

TEST_F(Meta, MetaTypeConstruct) {
    auto type = meta::resolve<derived_type>();
    auto any = type.construct(base_type{}, 42, 'c');

    ASSERT_TRUE(any);
    ASSERT_TRUE(any.can_cast<derived_type>());
    ASSERT_EQ(any.cast<derived_type>().i, 42);
    ASSERT_EQ(any.cast<derived_type>().c, 'c');
}

TEST_F(Meta, MetaTypeConstructMetaAnyArgs) {
    auto type = meta::resolve<derived_type>();
    auto any = type.construct(meta::any{base_type{}}, meta::any{42}, meta::any{'c'});

    ASSERT_TRUE(any);
    ASSERT_TRUE(any.can_cast<derived_type>());
    ASSERT_EQ(any.cast<derived_type>().i, 42);
    ASSERT_EQ(any.cast<derived_type>().c, 'c');
}


TEST_F(Meta, MetaTypeConstructInvalidArgs) {
    auto type = meta::resolve<derived_type>();
    auto any = type.construct(meta::any{base_type{}}, meta::any{'c'}, meta::any{42});
    ASSERT_FALSE(any);
}


TEST_F(Meta, MetaTypeConstructCastAndConvert) {
    auto type = meta::resolve<derived_type>();
    auto any = type.construct(meta::any{derived_type{}}, meta::any{42.}, meta::any{'c'});

    ASSERT_TRUE(any);
    ASSERT_TRUE(any.can_cast<derived_type>());
    ASSERT_EQ(any.cast<derived_type>().i, 42);
    ASSERT_EQ(any.cast<derived_type>().c, 'c');
}


TEST_F(Meta, MetaTypeDestroyDtor) {
    auto type = meta::resolve<empty_type>();

    ASSERT_EQ(empty_type::counter, 0);
    ASSERT_TRUE(type.destroy(empty_type{}));
    ASSERT_EQ(empty_type::counter, 1);
}

TEST_F(Meta, MetaTypeDestroyDtorInvalidArg) {
    auto type = meta::resolve<empty_type>();

    ASSERT_EQ(empty_type::counter, 0);
    ASSERT_FALSE(type.destroy('c'));
    ASSERT_EQ(empty_type::counter, 0);
}

TEST_F(Meta, MetaTypeDestroyDtorCastAndConvert) {
    auto type = meta::resolve<empty_type>();

    ASSERT_EQ(empty_type::counter, 0);
    ASSERT_FALSE(type.destroy(fat_type{}));
    ASSERT_EQ(empty_type::counter, 0);
    ASSERT_FALSE(meta::resolve<int>().destroy(42.));
}

TEST_F(Meta, MetaTypeDestroyNoDtor) {
    ASSERT_TRUE(meta::resolve<char>().destroy('c'));
}

TEST_F(Meta, MetaTypeDestroyNoDtorInvalidArg) {
    ASSERT_FALSE(meta::resolve<char>().destroy(42));
}

TEST_F(Meta, MetaTypeDestroyNoDtorVoid) {
    ASSERT_FALSE(meta::resolve<void>().destroy({}));
}

TEST_F(Meta, MetaTypeDestroyNoDtorCastAndConvert) {
    ASSERT_FALSE(meta::resolve<int>().destroy(42.));
}

TEST_F(Meta, MetaDataFromBase) {
    auto type = meta::resolve<concrete_type>();
    concrete_type instance;

    ASSERT_TRUE(type.data("i"));
    ASSERT_TRUE(type.data("j"));

    ASSERT_EQ(instance.i, 0);
    ASSERT_EQ(instance.j, char{});
    ASSERT_TRUE(type.data("i").set(instance, 3));
    ASSERT_TRUE(type.data("j").set(instance, 'c'));
    ASSERT_EQ(instance.i, 3);
    ASSERT_EQ(instance.j, 'c');
}

TEST_F(Meta, MetaFuncFromBase) {
    auto type = meta::resolve<concrete_type>();
    auto base = meta::resolve<an_abstract_type>();
    concrete_type instance;

    ASSERT_TRUE(type.func("f"));
    ASSERT_TRUE(type.func("g"));
    ASSERT_TRUE(type.func("h"));

    ASSERT_EQ(type.func("f").parent(), meta::resolve<concrete_type>());
    ASSERT_EQ(type.func("g").parent(), meta::resolve<an_abstract_type>());
    ASSERT_EQ(type.func("h").parent(), meta::resolve<another_abstract_type>());

    ASSERT_EQ(instance.i, 0);
    ASSERT_EQ(instance.j, char{});

    type.func("f").invoke(instance, 3);
    type.func("h").invoke(instance, 'c');

    ASSERT_EQ(instance.i, 9);
    ASSERT_EQ(instance.j, 'c');

    base.func("g").invoke(instance, 3);

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
    auto type = meta::resolve<an_abstract_type>();
    concrete_type instance;

    ASSERT_EQ(instance.i, 0);

    type.func("f").invoke(instance, 3);

    ASSERT_EQ(instance.i, 3);

    type.func("g").invoke(instance, 3);

    ASSERT_EQ(instance.i, -3);
}

TEST_F(Meta, EnumAndNamedConstants) {
    auto type = meta::resolve<properties>();

    ASSERT_TRUE(type.data("prop_bool"));
    ASSERT_TRUE(type.data("prop_int"));

    ASSERT_EQ(type.data("prop_bool").type(), type);
    ASSERT_EQ(type.data("prop_int").type(), type);

    ASSERT_FALSE(type.data("prop_bool").set({}, properties::prop_int));
    ASSERT_FALSE(type.data("prop_int").set({}, properties::prop_bool));

    ASSERT_EQ(type.data("prop_bool").get({}).cast<properties>(), properties::prop_bool);
    ASSERT_EQ(type.data("prop_int").get({}).cast<properties>(), properties::prop_int);
}

TEST_F(Meta, ArithmeticTypeAndNamedConstants) {
    auto type = meta::resolve<unsigned int>();

    ASSERT_TRUE(type.data("min"));
    ASSERT_TRUE(type.data("max"));

    ASSERT_EQ(type.data("min").type(), type);
    ASSERT_EQ(type.data("max").type(), type);

    ASSERT_FALSE(type.data("min").set({}, 100u));
    ASSERT_FALSE(type.data("max").set({}, 0u));

    ASSERT_EQ(type.data("min").get({}).cast<unsigned int>(), 0u);
    ASSERT_EQ(type.data("max").get({}).cast<unsigned int>(), 100u);
}

TEST_F(Meta, Unregister) {
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

    ASSERT_FALSE(meta::resolve("char"));
    ASSERT_FALSE(meta::resolve("base"));
    ASSERT_FALSE(meta::resolve("derived"));
    ASSERT_FALSE(meta::resolve("empty"));
    ASSERT_FALSE(meta::resolve("fat"));
    ASSERT_FALSE(meta::resolve("data"));
    ASSERT_FALSE(meta::resolve("func"));
    ASSERT_FALSE(meta::resolve("setter_getter"));
    ASSERT_FALSE(meta::resolve("an_abstract_type"));
    ASSERT_FALSE(meta::resolve("another_abstract_type"));
    ASSERT_FALSE(meta::resolve("concrete"));

    Meta::SetUpAfterUnregistration();
    meta::any any{42.};

    ASSERT_TRUE(any);
    ASSERT_FALSE(any.can_convert<int>());
    ASSERT_TRUE(any.can_convert<float>());

    ASSERT_FALSE(meta::resolve("derived"));
    ASSERT_TRUE(meta::resolve("my_type"));

    meta::resolve<derived_type>().prop([](auto prop) {
        ASSERT_TRUE(prop);
        ASSERT_EQ(prop.key(), properties::prop_bool);
        ASSERT_FALSE(prop.value().template cast<bool>());
    });

    ASSERT_FALSE((meta::resolve<derived_type>().ctor<const base_type &, int, char>()));
    ASSERT_TRUE((meta::resolve<derived_type>().ctor<>()));

    ASSERT_TRUE(meta::resolve("your_type").data("a_data_member"));
    ASSERT_FALSE(meta::resolve("your_type").data("another_data_member"));

    ASSERT_TRUE(meta::resolve("your_type").func("a_member_function"));
    ASSERT_FALSE(meta::resolve("your_type").func("another_member_function"));
}
