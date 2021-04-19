#include "attr_test.h"

#include <cassert>

int main() {
    st_attr::name name_attr;
    name_attr.description = "Test";

    [[maybe_unused]] auto const& attr = st::test::get_annotation<0>();
    assert(attr.description == "TestStruct");
}
