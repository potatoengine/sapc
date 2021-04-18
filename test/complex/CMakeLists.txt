sapc_test(
    TARGET sapc_test_complex
    SOURCES test.cc
    SCHEMAS common.sap test.sap
)
add_test(NAME sapc_test_complex COMMAND sapc_test_complex)
