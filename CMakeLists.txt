cmake_minimum_required(VERSION 3.10.0 FATAL_ERROR)

project (physx-test CXX)

if (WIN32)
    include_directories(${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include/)
    message("Arch: ${VCPKG_TARGET_TRIPLET}, Build type: ${CMAKE_BUILD_TYPE}")
    if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
        link_directories(${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib/)
    else()
        link_directories(${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib/)
    endif()
else()
    include_directories($ENV{VCPKG_ROOT}/installed/x64-linux/include/)
    link_directories($ENV{VCPKG_ROOT}/installed/x64-linux/lib/)
endif()

set(CMAKE_CXX_STANDARD 17)

if (WIN32)
else()
    set(CMAKE_BUILD_TYPE Release)
    # add_definitions(-D_DEBUG)
    # set(CMAKE_BUILD_TYPE Debug)
    set(CMAKE_CXX_FLAGS "-pthread -march=native -O3")
endif()

set(SRC_SERVER_FILES
    "src/gl/base.cpp"
    "src/world/world.cpp"
    "src/network/quic/server.cpp"
    "src/server/game.cpp"
    "src/server/debug/renderer.cpp"
    "src/network/protocol/server-tick.cpp"
    "src/main/server-gui.cpp"
)

set(SRC_CLIENT_FILES
    "src/network/quic/client.cpp"
    "src/client/gui.cpp"
    "src/main/client-gui.cpp"
)

set(SRC_HEADLESS_SERVER_FILES
    "src/world/world.cpp"
    "src/network/quic/server.cpp"
    "src/server/game.cpp"
    "src/network/protocol/server-tick.cpp"
    "src/main/server-headless.cpp"
)

set(SRC_HEADLESS_CLIENT_FILES
    "src/network/quic/client.cpp"
    "src/main/client-headless.cpp"
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
    set(OpenGL_GL_PREFERENCE LEGACY)
    find_package(OpenGL REQUIRED)
    find_package(GLUT REQUIRED)
    find_package(msquic CONFIG REQUIRED)

    add_compile_definitions(PHYSX_USE_CUDA)

    add_executable("server-gui" ${SRC_SERVER_FILES})
    target_link_libraries("server-gui" ${PHYSX_LIBS} ${OPENGL_LIBRARIES} libuv freeglut msquic)
        
    add_executable("client-gui" ${SRC_CLIENT_FILES})
    target_link_libraries("client-gui" ${OPENGL_LIBRARIES} libuv freeglut msquic)

    add_executable("server-headless" ${SRC_HEADLESS_SERVER_FILES})
    target_link_libraries("server-headless" ${PHYSX_LIBS} libuv msquic)
        
    add_executable("client-headless" ${SRC_HEADLESS_CLIENT_FILES})
    target_link_libraries("client-headless" msquic libuv)
    
else()
    # set(OpenGL_GL_PREFERENCE LEGACY)
    # find_package(OpenGL REQUIRED)
    # set(X_LIBS X11 Xi Xrandr Xxf86vm)
    # add_executable("server-gui" ${SRC_SERVER_FILES})
    # target_link_libraries("server-gui" ${PHYSX_LIBS} ${OPENGL_LIBRARIES} libuv glut msquic dl)

    add_executable("server-headless" ${SRC_HEADLESS_SERVER_FILES})
    target_link_libraries("server-headless" ${PHYSX_LIBS} libuv msquic dl)
    
    add_executable("client-headless" ${SRC_HEADLESS_CLIENT_FILES})
    target_link_libraries("client-headless" libuv msquic)
endif()