set(INCLUDES_FOR_EXAMPLES ../include)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(utf8cpp CONFIG REQUIRED)
set(LIBS_FOR_EXAMPLES 
    unofficial::sqlite3::sqlite3
    nlohmann_json::nlohmann_json
    utf8cpp::utf8cpp
)

function(create_example_binary bin_name cpp_file)
    cmake_path(GET cpp_file STEM target)
    message("configure example '${target}'")
    add_executable("${target}" ${cpp_file})
    set_target_properties("${target}" PROPERTIES OUTPUT_NAME "${bin_name}")

    set_property(TARGET "${target}" PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    target_include_directories("${target}" PRIVATE ${INCLUDES_FOR_EXAMPLES})
    target_link_libraries("${target}" PRIVATE ${LIBS_FOR_EXAMPLES})

endfunction()

create_example_binary(sqlitemap sqlitemap_client.cpp)
create_example_binary(lines2sqlitemap lines_to_sqlitemap.cpp)
create_example_binary(sqlitemap_json sqlitemap_json.cpp)
create_example_binary(sqlitemap_tiles sqlitemap_tiles.cpp)


