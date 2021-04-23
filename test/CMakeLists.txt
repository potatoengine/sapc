if(SAPC_BUILD_TESTS)
    include(sapc_test.cmake)

    # Core language features
    add_subdirectory(basic)
    add_subdirectory(enum)
    add_subdirectory(attributes)
    add_subdirectory(arrays)

    # Including other modules
    add_subdirectory(include)
    add_subdirectory(search)

    # Advanced features
    add_subdirectory(generics)
    add_subdirectory(custom)
    add_subdirectory(complex)
endif()
