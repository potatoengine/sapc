#include "test.h"

int main(int argc, char** argv) {
    [[maybe_unused]] sapc_type::default_ val_default_ = {};
    [[maybe_unused]] sapc_type::number val_number = {};
    [[maybe_unused]] sapc_type::derived val_derived = {};

    sapc_type::test val_test = {};
    val_test.array.push_back("a string");
    int i = sapc_type::zero;
    val_test.iptr = &i;
    val_test.vecPtrs.push_back(std::make_unique<sapc_type::Vec>());

    sapc_type::ordered val_ordered = {};
    val_ordered.position = sapc_type::order::second;
}
