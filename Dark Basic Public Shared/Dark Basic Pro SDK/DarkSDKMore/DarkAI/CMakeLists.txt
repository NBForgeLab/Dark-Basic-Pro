project(DBPPluginDarkAI LANGUAGES C CXX)

set(DARKAI_SOURCES
    AIThread.cpp
    Beacon.cpp
    CollisionTree.cpp
    Container.cpp
    DarkAI.cpp
    "DBPro Functions.cpp"
    DynamicPathFinder.cpp
    Entity.cpp
    EntityBuffer.cpp
    EntityThread.cpp
    Grid.cpp
    Hero.cpp
    LeeThread.cpp
    LUAScript.cpp
    Path.cpp
    PathFinderAdvanced.cpp
    Polygon.cpp
    StateMachine/StateAttack.cpp
    StateMachine/StateAttackFromCover.cpp
    StateMachine/StateChaseAttack.cpp
    StateMachine/StateDefend.cpp
    StateMachine/StateDiving.cpp
    StateMachine/StateFallBack.cpp
    StateMachine/StateGoToDest.cpp
    StateMachine/StateIdle.cpp
    StateMachine/StateInvestigate.cpp
    StateMachine/StateLeaping.cpp
    StateMachine/StateManual.cpp
    StateMachine/StatePatrol.cpp
    StateMachine/StatePeekFromCorner.cpp
    StateMachine/StateSearchArea.cpp
    StateMachine/StateSet.cpp
    StateMachine/StateStrafeAttack.cpp
    StateMachine/StateWaitInCover.cpp
    Team.cpp
    TeamController.cpp
    TreeFace.cpp
    TreeNode.cpp
    World.cpp
    Zone.cpp
    stringtable.rc
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core/globstruct.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error/CError.cpp"
)

add_library(dbp_plugin_darkai SHARED ${DARKAI_SOURCES})
dbp_apply_legacy_cpp_options(dbp_plugin_darkai)

set_target_properties(dbp_plugin_darkai PROPERTIES
    OUTPUT_NAME "DarkAI"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
)

target_compile_definitions(dbp_plugin_darkai PRIVATE
    WIN32
    _WINDOWS
    _USRDLL
    DARKAI_EXPORTS
    _MBCS
    _CRT_SECURE_NO_WARNINGS
)

target_include_directories(dbp_plugin_darkai PUBLIC
    .
    StateMachine
    "${CMAKE_SOURCE_DIR}/SDK/BaseClasses"
    "${CMAKE_SOURCE_DIR}/SDK/DB3"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Include"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Objects"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/DBOFormat"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Memblocks"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Camera"
    ${DXSDK_INCLUDE_DIR}
)

if(DXSDK_LIBRARY_DIR)
    target_link_directories(dbp_plugin_darkai PRIVATE ${DXSDK_LIBRARY_DIR})
endif()

target_link_libraries(dbp_plugin_darkai PRIVATE
    winmm
    d3d9
    d3dx9
    dxguid
    gdi32
    user32
    advapi32
)
