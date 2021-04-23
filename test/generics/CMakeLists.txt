sapc_test(
    TARGET sapc_test_generics
    SOURCES generics_main.cc
    SCHEMAS generics_test.sap
)
add_test(NAME sapc_test_generics COMMAND sapc_test_generics)
