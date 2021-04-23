#include "custom_test.h"

#include <type_traits>

template <int N> struct is_seven {};
template <> struct is_seven<7> { static constexpr int value = 0; };
template <int N> constexpr int is_seven_v = is_seven<N>::value;

int main() {
    static_assert(std::is_class_v<st::LocalTest>);
    static_assert(std::is_class_v<st_custom::CustomTest>);
    static_assert(std::is_enum_v<st_flags::FlagsTest>);
    static_assert(std::is_same_v<int, st::Int>);
    static_assert(std::is_union_v<st::Types>);

    return is_seven_v<st::seven>;
}
