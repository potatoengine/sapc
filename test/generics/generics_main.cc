#include "generics_test.h"

#include <type_traits>

int main() {
    [[maybe_unused]] st::Map<std::string, int> kv;

    static_assert(std::is_same_v<decltype(kv), decltype(st::Test::namesToValues)>);
}
