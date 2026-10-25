#include "move_only_function.h"
#include "doctest.h"

static_assert(!std::is_copy_constructible_v<mv::MoveOnlyFunction<void()>>);
static_assert(!std::is_copy_assignable_v<mv::MoveOnlyFunction<void()>>);
static_assert(std::is_nothrow_move_constructible_v<mv::MoveOnlyFunction<void()>>);
static_assert(std::is_nothrow_move_assignable_v<mv::MoveOnlyFunction<void()>>);
static_assert(!std::is_constructible_v<mv::MoveOnlyFunction<int()>, decltype([] {})>);

TEST_CASE("MoveOnlyFunction represents empty and null targets") {
    mv::MoveOnlyFunction<void()> empty;
    mv::MoveOnlyFunction<void()> null = nullptr;
    void (*pointer)() = nullptr;
    mv::MoveOnlyFunction<void()> null_pointer = pointer;
    CHECK_FALSE(empty);
    CHECK_FALSE(null);
    CHECK_FALSE(null_pointer);
}

TEST_CASE("MoveOnlyFunction moves a mutable non-copyable capture") {
    mv::MoveOnlyFunction<int(int)> source = [value = std::make_unique<int>(10)](int increment) mutable {
        return *value += increment;
    };
    auto destination = std::move(source);
    CHECK_FALSE(source);
    REQUIRE(destination);
    CHECK(destination(2) == 12);
    CHECK(destination(3) == 15);
}

TEST_CASE("MoveOnlyFunction forwards move-only arguments and results") {
    mv::MoveOnlyFunction<std::unique_ptr<int>(std::unique_ptr<int>)> function =
        [](std::unique_ptr<int> value) { ++*value; return value; };
    auto value = function(std::make_unique<int>(41));
    CHECK(*value == 42);

    mv::MoveOnlyFunction<int&(int&)> reference = [](int& value) -> int& { return value; };
    reference(*value) = 7;
    CHECK(*value == 7);
}

TEST_CASE("MoveOnlyFunction releases replaced and reset captures exactly once") {
    struct CountDeletion {
        int* count;
        void operator()(int* value) const { ++*count; delete value; }
    };
    int deleted = 0;
    {
        mv::MoveOnlyFunction<void()> first =
            [owned = std::unique_ptr<int, CountDeletion>(new int(1), CountDeletion{&deleted})] {};
        mv::MoveOnlyFunction<void()> second =
            [owned = std::unique_ptr<int, CountDeletion>(new int(2), CountDeletion{&deleted})] {};
        first = std::move(second);
        CHECK(deleted == 1);
        CHECK_FALSE(second);
        first = nullptr;
        CHECK(deleted == 2);
        CHECK_FALSE(first);
    }
    CHECK(deleted == 2);
}

TEST_CASE("MoveOnlyFunction accepts functions and discards unused return values") {
    int called = 0;
    mv::MoveOnlyFunction<void()> function = [&called] { return ++called; };
    function();
    CHECK(called == 1);

    auto increment = [](int value) { return value + 1; };
    mv::MoveOnlyFunction<int(int)> from_pointer = +increment;
    mv::MoveOnlyFunction<int(int)> from_lvalue = increment;
    CHECK(from_pointer(41) == 42);
    CHECK(from_lvalue(41) == 42);
}
