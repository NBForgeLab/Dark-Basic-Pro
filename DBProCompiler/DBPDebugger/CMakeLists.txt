add_executable(DBPDebugger
    DBPDebugger.cpp
    DBPDebugger.h
    DBPDebuggerProtocol.h
)

dbp_apply_modern_cpp_options(DBPDebugger)

target_include_directories(DBPDebugger PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(DBPDebugger PRIVATE
    user32
    gdi32
)

set_target_properties(DBPDebugger PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/$<CONFIG>"
)

# Deploy into the FPSC checkout after every build (see cmake/DBPDeploy.cmake).
dbp_deploy_target(DBPDebugger)
