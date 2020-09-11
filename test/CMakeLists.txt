if(SAPC_BUILD_TESTS)
    find_package (Python COMPONENTS Interpreter)

    add_executable(sapc_gen_test test.cc)
    target_compile_features(sapc_gen_test PRIVATE cxx_std_17)
    target_include_directories(sapc_gen_test PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

    function(sapc_test_compile_header NAME)
        add_custom_command(OUTPUT ${NAME}.sap.json
            COMMAND sapc -o test/${NAME}.sap.json -d test/${NAME}.sap.d -- ${CMAKE_CURRENT_SOURCE_DIR}/${NAME}.sap
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            MAIN_DEPENDENCY ${NAME}.sap
            DEPENDS sapc
            DEPFILE test/${NAME}.sap.d
        )
        add_custom_command(OUTPUT ${NAME}.h
            COMMAND Python::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/gen_header.py -i ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.sap.json -o test/${NAME}.h
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            MAIN_DEPENDENCY ${NAME}.sap.json
            DEPENDS gen_header.py
        )
        target_sources(sapc_gen_test PRIVATE ${NAME}.h)
    endfunction()

    sapc_test_compile_header(common)
    sapc_test_compile_header(test)
endif()
