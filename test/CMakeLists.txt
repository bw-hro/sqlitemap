find_package(Catch2 3 REQUIRED)

add_executable(tests
    "catch2/unit_tests/sqlitemap_algorithms_tests.cpp"
    "catch2/unit_tests/sqlitemap_codecs_tests.cpp"
    "catch2/unit_tests/sqlitemap_core_tests.cpp"
    "catch2/unit_tests/sqlitemap_helper_tests.cpp"
)

set_property(TARGET tests PROPERTY
             MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(INCLUDES_FOR_TESTS ../include)
target_compile_definitions(tests PRIVATE CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS)
target_include_directories(tests PRIVATE ${INCLUDES_FOR_TESTS})
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
target_link_libraries(tests PRIVATE unofficial::sqlite3::sqlite3)

if(SM_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(tests PRIVATE --coverage)
        target_link_options(tests PRIVATE --coverage)
    else()
        message(FATAL_ERROR "Coverage only supported with GCC/Clang")
    endif()
endif()

option(SM_SKIP_TESTS "skip tests execution" OFF)
message("SM_SKIP_TESTS: ${SM_SKIP_TESTS}")
if(NOT SM_SKIP_TESTS)
    include(CTest)
    include(Catch)
    catch_discover_tests(tests)
endif()
