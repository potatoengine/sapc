sapc_test(
    TARGET sapc_test_enum
    SOURCES enum_main.cc
    SCHEMAS enum_test.sap
)
add_test(NAME sapc_test_enum COMMAND sapc_test_enum)
