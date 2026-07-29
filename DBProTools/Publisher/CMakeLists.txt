add_library(dbp_publisher STATIC
    PublisherManifest.cpp
)

dbp_apply_modern_cpp_options(dbp_publisher)
dbp_enable_sanitizers(dbp_publisher)

target_include_directories(dbp_publisher PUBLIC
    "${PROJECT_SOURCE_DIR}"
)

target_link_libraries(dbp_publisher PUBLIC
    dbp_package
    nlohmann_json::nlohmann_json
)
