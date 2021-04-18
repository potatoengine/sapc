sapc_test(
    TARGET sapc_test_basic
    SOURCES basic_main.cc
    SCHEMAS basic_test.sap
)
add_test(NAME sapc_test_basic COMMAND sapc_test_basic)
