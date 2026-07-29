add_library(dbp_publisher STATIC
    PublisherCli.cpp
    PublisherManifest.cpp
)

dbp_apply_modern_cpp_options(dbp_publisher)
dbp_enable_sanitizers(dbp_publisher)

target_include_directories(dbp_publisher PUBLIC
    "${PROJECT_SOURCE_DIR}"
)

target_link_libraries(dbp_publisher PUBLIC
    dbp_package
)

target_link_libraries(dbp_publisher PRIVATE
    nlohmann_json::nlohmann_json
)

add_executable(dbp-publish
    Main.cpp
)

dbp_apply_modern_cpp_options(dbp-publish)
dbp_enable_sanitizers(dbp-publish)

target_link_libraries(dbp-publish PRIVATE
    dbp_publisher
)
