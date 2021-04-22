sapc_test(
    TARGET sapc_test_custom
    SOURCES custom_main.cc
    SCHEMAS custom_base.sap custom_test.sap
)
add_test(NAME sapc_test_custom COMMAND sapc_test_custom)
