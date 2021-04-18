sapc_test(
    TARGET sapc_test_search
    SOURCES search_main.cc
    SCHEMAS other/base.sap search_test.sap
    INCLUDE other
)
