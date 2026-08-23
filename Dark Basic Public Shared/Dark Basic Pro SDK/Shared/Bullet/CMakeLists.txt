project(DBPPluginODE LANGUAGES C CXX)

set(BULLET_SOURCES
    BulletPhysics.CPP
    DynamicCharacterController.cpp
    Ragdoll/BaseItem.cpp
    Ragdoll/BaseItemManager.cpp
    Ragdoll/BT2DX.cpp
    Ragdoll/DBProJoint.cpp
    Ragdoll/DBProJointManager.cpp
    Ragdoll/DBProJoints.cpp
    Ragdoll/DBProMotionState.cpp
    Ragdoll/DBProRagDoll.cpp
    Ragdoll/DBProRagDollBone.cpp
    Ragdoll/DBProRagdollManager.cpp
    Ragdoll/DBProToBullet.cpp
    Ragdoll/DBPro/include/DBPro.cpp
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Data/CDataC.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core/globstruct.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error/CError.cpp"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDK/Bullet/Script1.RC"
)

file(GLOB BULLET_COLLISION_SRCS
    "${CMAKE_SOURCE_DIR}/SDK/BULLET/bullet-2.81-rev2613/src/BulletCollision/BroadphaseCollision/*.cpp"
    "${CMAKE_SOURCE_DIR}/SDK/BULLET/bullet-2.81-rev2613/src/BulletCollision/CollisionDispatch/*.cpp"
    "${CMAKE_SOURCE_DIR}/SDK/BULLET/bullet-2.81-rev2613/src/BulletCollision/CollisionShapes/*.cpp"
    "${CMAKE_SOURCE_DIR}/SDK/BULLET/bullet-2.81-rev2613/src/BulletCollision/Gimpact/*.cpp"
    "${CMAKE_SOURCE_DIR}/SDK/BULLET/bullet-2.81-rev2613/src/BulletCollision/NarrowPhaseCollision/*.cpp"
)
file(GLOB BULLET_DYNAMICS_SRCS
    "${CMAKE_SOURCE_DIR}/SDK/BULLET/bullet-2.81-rev2613/src/BulletDynamics/Character/*.cpp"
    "${CMAKE_SOURCE_DIR}/SDK/BULLET/bullet-2.81-rev2613/src/BulletDynamics/ConstraintSolver/*.cpp"
    "${CMAKE_SOURCE_DIR}/SDK/BULLET/bullet-2.81-rev2613/src/BulletDynamics/Dynamics/*.cpp"
    "${CMAKE_SOURCE_DIR}/SDK/BULLET/bullet-2.81-rev2613/src/BulletDynamics/Vehicle/*.cpp"
)
file(GLOB BULLET_LINEARMATH_SRCS
    "${CMAKE_SOURCE_DIR}/SDK/BULLET/bullet-2.81-rev2613/src/LinearMath/*.cpp"
)
file(GLOB BULLET_SOFTBODY_SRCS
    "${CMAKE_SOURCE_DIR}/SDK/BULLET/bullet-2.81-rev2613/src/BulletSoftBody/*.cpp"
)

add_library(bullet_physics STATIC
    ${BULLET_COLLISION_SRCS}
    ${BULLET_DYNAMICS_SRCS}
    ${BULLET_LINEARMATH_SRCS}
    ${BULLET_SOFTBODY_SRCS}
)
dbp_apply_legacy_cpp_options(bullet_physics)
target_include_directories(bullet_physics PUBLIC
    "${CMAKE_SOURCE_DIR}/SDK/BULLET/bullet-2.81-rev2613/src"
)

# Vendored third-party source (bullet-2.81-rev2613) is kept pristine, mirroring the
# documented C4201 exception for DirectX headers: /W4 noise from upstream code is
# silenced here only, while all plugin-owned code keeps the full warning set.
if(MSVC)
    target_compile_options(bullet_physics PRIVATE
        /wd4100  # unreferenced formal parameter (upstream API signatures)
        /wd4189  # unreferenced local (upstream debug hooks)
        /wd4244  # narrowing conversions inside upstream math/serialization
        /wd4267  # size_t narrowing in upstream serialization
        /wd4305  # double->float truncation in upstream literals
        /wd4389 /wd4018 /wd4245 # signed/unsigned loop comparisons
        /wd4456 /wd4457 /wd4458 # shadowed declarations in upstream scopes
        /wd4463 # overflow in implicit constant conversion
        /wd4505 # unreferenced inline function removed
        /wd4701 /wd4702 /wd4715 # uninitialized/unreachable paths in upstream dispatch
        /wd4838 # narrowing in aggregate initialization
    )
endif()

target_compile_definitions(bullet_physics PRIVATE
    WIN32
    _WINDOWS
    _MBCS
    _CRT_SECURE_NO_WARNINGS
)

add_library(dbp_plugin_ode SHARED ${BULLET_SOURCES})
dbp_apply_legacy_cpp_options(dbp_plugin_ode)

set_target_properties(dbp_plugin_ode PROPERTIES
    OUTPUT_NAME "DBProODEDebug"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins"
)

target_compile_definitions(dbp_plugin_ode PRIVATE
    WIN32
    _WINDOWS
    _MBCS
    _CRT_SECURE_NO_WARNINGS
)

target_include_directories(dbp_plugin_ode PUBLIC
    .
    Ragdoll
    Ragdoll/DBPro/include
    "${CMAKE_SOURCE_DIR}/SDK/BULLET/bullet-2.81-rev2613/src"
    "${CMAKE_SOURCE_DIR}/SDK/BaseClasses"
    "${CMAKE_SOURCE_DIR}/SDK/DB3"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Error"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Data"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Objects"
    "${CMAKE_SOURCE_DIR}/Dark Basic Public Shared/Dark Basic Pro SDK/Shared/DBOFormat"
    ${DXSDK_INCLUDE_DIR}
)

if(DXSDK_LIBRARY_DIR)
    target_link_directories(dbp_plugin_ode PRIVATE ${DXSDK_LIBRARY_DIR})
endif()

target_link_libraries(dbp_plugin_ode PRIVATE
    bullet_physics
    winmm
    d3d9
    d3dx9
    dxguid
    gdi32
    user32
    advapi32
)
