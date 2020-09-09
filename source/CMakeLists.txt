find_package(nlohmann_json 3.2.0 REQUIRED)

add_executable(sapc
    analyze.cc
    analyze.hh
    compiler.cc
    compiler.hh
    file_util.hh
    lexer.cc
    lexer.hh
    location.cc
    location.hh
    grammar.cc
    grammar.hh
    main.cc
    model.cc
    model.hh
)
target_compile_features(sapc PRIVATE cxx_std_17)
target_link_libraries(sapc PRIVATE nlohmann_json)

set(sapc_prefix sapc_parse)
target_sources(sapc PRIVATE ${sapc_sources})
target_include_directories(sapc PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS sapc RUNTIME)
