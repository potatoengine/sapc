sapc_test(
    TARGET sapc_test_search
    SOURCES search_main.cc
    SCHEMAS other/base.sap search_test.sap
    INCLUDE other
)
add_test(NAME sapc_test_search COMMAND sapc_test_search)
