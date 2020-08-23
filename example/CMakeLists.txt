add_custom_command(OUTPUT test.json
    COMMAND sapc -c -i ${CMAKE_CURRENT_SOURCE_DIR}/test.sap -o example/test.json -d example/test.sap.d
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    MAIN_DEPENDENCY test.sap
    DEPENDS sapc
    DEPFILE example/test.sap.d
)

find_package (Python COMPONENTS Interpreter)
add_custom_command(OUTPUT test.h
    COMMAND Python::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/gen_header.py -i ${CMAKE_CURRENT_BINARY_DIR}/test.json -o example/test.h
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    MAIN_DEPENDENCY test.json
    DEPENDS gen_header.py
)

add_executable(sapc_gen_test test.cc test.h)
target_compile_features(sapc_gen_test PRIVATE cxx_std_17)
target_include_directories(sapc_gen_test PRIVATE ${CMAKE_CURRENT_BINARY_DIR})