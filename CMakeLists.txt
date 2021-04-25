cmake_minimum_required(VERSION 3.16)
project(sapc VERSION 0.16 LANGUAGES CXX C)

cmake_policy(PUSH)

# CMake 3.20 will warn on some of our add_custom_command calls without this;
# we work just fine with the NEW policy behavior, but this is safer until we
# can test it thoroughly
#
if(POLICY CMP0116)
    cmake_policy(SET CMP0116 OLD)
endif()

set(SAPC_JSON_SCHEMA_PATH ${CMAKE_CURRENT_SOURCE_DIR}/schema/sap-1.schema.json)

set(SAPC_IS_ROOT_PROJECT OFF)
if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
	set(SAPC_IS_ROOT_PROJECT ON)
endif()

option(SAPC_BUILD_TESTS "Build sapc test" ${SAPC_IS_ROOT_PROJECT})
option(SAPC_VALIDATE_SCHEMA_TESTS "Validate schemas when building tests (requires ajv-bin from npm)" OFF)
option(SAPC_USE_EMBEDDED_JSON "Try to acquire and use an embedded copy of nlohmann_json" ${SAPC_IS_ROOT_PROJECT})

if(SAPC_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(external)
add_subdirectory(source)
add_subdirectory(test)

cmake_policy(POP)
