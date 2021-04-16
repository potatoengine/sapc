#include "test.h"

int main(int argc, char** argv) {
    [[maybe_unused]] st::default_ val_default_ = {};
    [[maybe_unused]] st::number val_number = {};
    [[maybe_unused]] st::derived val_derived = {};

    st::ns::test val_test = {};
    val_test.array.push_back("a string");
    int i = static_cast<int>(st::zero.x);
    val_test.iptr = &i;
    val_test.vecPtrs.push_back(std::make_unique<st::Vec>());

    [[maybe_unused]] st::ordered val_ordered = {};
    val_ordered.position = st::order::second;

    if (st::right.x != 1 || st::right.y != 0)
        return 1;
    if (st::up.x != 0 || st::up.y != 1)
        return 1;

    st::List<int> l;
    l.items.push_back(1);

    return 0;
}
