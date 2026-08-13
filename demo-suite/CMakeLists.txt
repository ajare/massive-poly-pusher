add_executable(DemoSuite
    src/Camera.cpp
    src/Helper.cpp
    src/Logger.cpp
    src/Main.cpp
    src/ModelScene.cpp
    src/ProgramOptions.cpp
    src/PackageScene.cpp
    src/Scene.cpp)

if(WIN32)
    set_target_properties(DemoSuite PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

mpp_target_defaults(DemoSuite)
mpp_add_header_filters(DemoSuite "${CMAKE_CURRENT_SOURCE_DIR}/include")
set_target_properties(DemoSuite PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:DemoSuite>")
target_include_directories(DemoSuite PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/vendor/include")
target_link_libraries(DemoSuite PRIVATE
    MppAppSupport
    MppHelper
    MppResourceParsers
    MassivePolyPusher
    MppMesh
    MppProgram
    Utils
    ImGui
    SDL3::SDL3
    assimp::assimp
    OpenGL::GL
    GLEW::GLEW
    ${CMAKE_DL_LIBS})

mpp_copy_runtime(DemoSuite
    SDL3-shared
    glew
    assimp
    ${MPP_DRACO_RUNTIME}
    Utils
    MppData
    MassivePolyPusher
    MppMesh
    MppProgram
    MppResourceParsers
    MppHelper)

add_custom_command(TARGET DemoSuite POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/demosuite.ini"
        "$<TARGET_FILE_DIR:DemoSuite>/demosuite.ini"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${PROJECT_SOURCE_DIR}/resources/demo-suite/bin/Common/DemoSuite.cfg"
        "$<TARGET_FILE_DIR:DemoSuite>/DemoSuite.cfg"
    VERBATIM)
