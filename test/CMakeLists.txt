if(SAPC_BUILD_TESTS)
    include(sapc_test.cmake)

    add_subdirectory(basic)
    add_subdirectory(enum)
    add_subdirectory(include)
    add_subdirectory(search)
    add_subdirectory(complex)
endif()
