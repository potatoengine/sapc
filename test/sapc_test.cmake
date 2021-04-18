find_package (Python COMPONENTS Interpreter)

function(sapc_test NAME)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "" "TARGET" "SOURCES;SCHEMAS;INCLUDE" )

    add_executable(${ARG_TARGET}_bin)
    add_test(${ARG_TARGET} COMMAND ${ARG_TARGET}_bin)

    if(ARG_SOURCES)
        target_sources(${ARG_TARGET}_bin PRIVATE ${ARG_SOURCES})
    endif()

    target_compile_features(${ARG_TARGET}_bin PRIVATE cxx_std_17)
    target_include_directories(${ARG_TARGET}_bin PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

    foreach(SCHEMA ${ARG_SCHEMAS})
        get_filename_component(BASENAME ${SCHEMA} NAME_WE)

        set(GEN_SCRIPT ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/gen_header.py)
        set(JSON_FILE ${CMAKE_CURRENT_BINARY_DIR}/${SCHEMA}.json)
        set(DEPS_FILE ${CMAKE_CURRENT_BINARY_DIR}/${SCHEMA}.d)
        set(HEAD_FILE ${CMAKE_CURRENT_BINARY_DIR}/${BASENAME}.h)

        set(INCLUDE_OPTS "")
        foreach(INCLUDE ${ARG_INCLUDE})
            file(REAL_PATH "${INCLUDE}" INCLUDE_LOCAL BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
            list(APPEND INCLUDE_OPTS "-I${INCLUDE_LOCAL}")
        endforeach()

        add_custom_command(OUTPUT ${SCHEMA}.json
            COMMAND sapc -o ${JSON_FILE} -d ${DEPS_FILE} ${INCLUDE_OPTS} -- ${CMAKE_CURRENT_SOURCE_DIR}/${SCHEMA}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            MAIN_DEPENDENCY ${SCHEMA}
            DEPENDS sapc
            DEPFILE ${DEPS_FILE}
        )

        add_custom_command(OUTPUT ${HEAD_FILE}
            COMMAND Python::Interpreter ${GEN_SCRIPT} -i ${JSON_FILE} -o ${HEAD_FILE}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            MAIN_DEPENDENCY ${JSON_FILE}
            DEPENDS ${GEN_SCRIPT}
        )
        target_sources(${ARG_TARGET}_bin PRIVATE ${BASENAME}.h)
    endforeach()
endfunction()
