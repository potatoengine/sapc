sapc_test(
    TARGET sapc_test_include
    SOURCES include_main.cc
    SCHEMAS base.sap include_test.sap
)
add_test(NAME sapc_test_include COMMAND sapc_test_include)
