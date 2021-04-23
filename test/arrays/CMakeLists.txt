sapc_test(
    TARGET sapc_test_arrays
    SOURCES arrays_main.cc
    SCHEMAS arrays_test.sap
)
add_test(NAME sapc_test_arrays COMMAND sapc_test_arrays)
