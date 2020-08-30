add_subdirectory(packcc)

if(SAPC_USE_EMBEDDED_JSON)
    include(FetchContent)

    if(NOT TARGET nlohmann_json)
        find_package(nlohmann_json 3.2.0 QUIET)
        if(NOT nlohmann_json_FOUND)
            FetchContent_Declare(json
                GIT_REPOSITORY https://github.com/nlohmann/json
                GIT_TAG v3.7.3
            )

            FetchContent_GetProperties(json)
            if(NOT json_POPULATED)
                FetchContent_Populate(json)
                set(JSON_BuildTests OFF CACHE BOOL "disable json's tests")
                set(JSON_Install OFF CACHE BOOL "disable json's install")
                add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR} EXCLUDE_FROM_ALL)
                set(nlohmann_json_DIR ${json_BINARY_DIR} PARENT_SCOPE)
            endif()
        endif()
    endif()
endif()
