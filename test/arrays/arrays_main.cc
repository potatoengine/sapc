#include "arrays_test.h"

#include <type_traits>

int main() {
    static_assert(std::is_same_v<std::vector<int>, decltype(st::sample::vector)>);
    static_assert(std::is_same_v<std::array<int, 5>, decltype(st::sample::array)>);
}
