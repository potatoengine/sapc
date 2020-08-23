add_executable(sapc
    context.cc
    context.h
    main.cc
    overloaded.hh
    state.cc
    state.hh
)
target_compile_features(sapc PRIVATE cxx_std_17)
target_link_libraries(sapc PRIVATE nlohmann_json)

set(sapc_prefix sapc_parse)
set(sapc_sources ${CMAKE_CURRENT_BINARY_DIR}/${sapc_prefix}.h ${CMAKE_CURRENT_BINARY_DIR}/${sapc_prefix}.c)
add_custom_command(
    OUTPUT ${sapc_sources}
    MAIN_DEPENDENCY sapc.peg
    COMMAND packcc -o ${sapc_prefix} ${CMAKE_CURRENT_SOURCE_DIR}/sapc.peg
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
target_sources(sapc PRIVATE ${sapc_sources})
target_include_directories(sapc PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS sapc RUNTIME)