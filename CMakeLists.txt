cmake_minimum_required(VERSION 3.10.0 FATAL_ERROR)

project (physx-server CXX)

if (WIN32)
    include_directories(${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include/)
    link_directories(${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib/)
else()
    include_directories($ENV{VCPKG_ROOT}/installed/x64-linux/include/)
    link_directories($ENV{VCPKG_ROOT}/installed/x64-linux/lib/)
endif()

set(CMAKE_CXX_STANDARD 17)

if (WIN32)
else()
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
    set(CMAKE_CXX_FLAGS "-pthread -march=znver2 -O3")
endif()

set(SRC_FILES
    "src/world/world.cpp"
    "src/viz/renderer.cpp"
    "src/main.cpp"
)

set(SRC_HEADLESS_FILES
    "src/world/world.cpp"
    "src/headless/main.cpp"
)

if (WIN32)
    set(PHYSX_LIBS
        "PhysXExtensions_static_64"
        "PhysX_64"
        "PhysXPvdSDK_static_64"
        "PhysXCharacterKinematic_static_64"
        "PhysXCooking_64"
        "PhysXCommon_64"
        "PhysXFoundation_64"
    )
else()
    set(PHYSX_LIBS
        "PhysXExtensions_static_64"
        "PhysX_static_64"
        "PhysXPvdSDK_static_64"
        "PhysXCharacterKinematic_static_64"
        "PhysXCooking_static_64"
        "PhysXCommon_static_64"
        "PhysXFoundation_static_64"
    )
endif()

if (WIN32)
    add_executable(server ${SRC_HEADLESS_FILES})
    target_link_libraries(server ${PHYSX_LIBS})

    # set(OpenGL_GL_PREFERENCE LEGACY)
    # find_package(OpenGL REQUIRED)
    # find_package(GLUT REQUIRED)
    # add_executable(server ${SRC_FILES})
    # target_link_libraries(server ${PHYSX_LIBS} ${OPENGL_LIBRARIES} freeglut)
else()
    add_executable(server ${SRC_HEADLESS_FILES})
    target_link_libraries(server pthread ${PHYSX_LIBS} dl)
endif()