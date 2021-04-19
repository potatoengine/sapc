sapc_test(
    TARGET sapc_test_attribute
    SOURCES attr_main.cc
    SCHEMAS attr_test.sap
)
add_test(NAME sapc_test_attribute COMMAND sapc_test_attribute)
