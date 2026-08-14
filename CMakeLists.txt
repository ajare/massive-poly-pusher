cmake_minimum_required(VERSION 3.22)
project(MassivePolyPusher VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_DEBUG_POSTFIX d)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

find_package(OpenGL REQUIRED)

# Assimp and GLEW still declare pre-3.5 policy compatibility.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum policy version for legacy dependencies")

# SDL 3 provides the window and input platform layer on Windows and Linux.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ext/sdl/CMakeLists.txt")
    message(FATAL_ERROR "SDL submodule is missing; run: git submodule update --init --recursive")
endif()
set(SDL_SHARED ON CACHE BOOL "Build SDL as a shared library" FORCE)
set(SDL_STATIC OFF CACHE BOOL "Do not build the SDL static library" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Do not build the SDL3_test library" FORCE)
set(SDL_TESTS OFF CACHE BOOL "Do not build SDL tests" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "Do not build SDL examples" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "Do not install SDL" FORCE)
add_subdirectory(ext/sdl EXCLUDE_FROM_ALL)

# GLEW keeps its CMake project under build/cmake. Its Git repository omits the
# generated headers and glew.c, so bootstrap those files from the matching,
# hash-pinned official source release while retaining the upstream submodule.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ext/glew/build/cmake/CMakeLists.txt")
    message(FATAL_ERROR "GLEW submodule is missing; run: git submodule update --init --recursive")
endif()
set(MPP_GLEW_VERSION 2.3.1)
set(MPP_GLEW_ARCHIVE "${CMAKE_BINARY_DIR}/_downloads/glew-${MPP_GLEW_VERSION}.tgz")
set(MPP_GLEW_SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/glew-${MPP_GLEW_VERSION}")
if(NOT EXISTS "${MPP_GLEW_ARCHIVE}")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/_downloads")
    file(DOWNLOAD
        "https://github.com/nigels-com/glew/releases/download/glew-${MPP_GLEW_VERSION}/glew-${MPP_GLEW_VERSION}.tgz"
        "${MPP_GLEW_ARCHIVE}"
        EXPECTED_HASH SHA256=b64790f94b926acd7e8f84c5d6000a86cb43967bd1e688b03089079799c9e889
        TLS_VERIFY ON
        SHOW_PROGRESS)
endif()
if(NOT EXISTS "${MPP_GLEW_SOURCE_DIR}/src/glew.c")
    file(REMOVE_RECURSE "${MPP_GLEW_SOURCE_DIR}")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/_deps")
    file(ARCHIVE_EXTRACT INPUT "${MPP_GLEW_ARCHIVE}" DESTINATION "${CMAKE_BINARY_DIR}/_deps")
endif()
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared dependency libraries" FORCE)
set(BUILD_UTILS OFF CACHE BOOL "Do not build GLEW utilities" FORCE)
set(GLEW_CUSTOM_OUTPUT_DIRS ON CACHE BOOL "Use the parent output directories" FORCE)
add_subdirectory("${MPP_GLEW_SOURCE_DIR}/build/cmake" "${CMAKE_BINARY_DIR}/ext/glew" EXCLUDE_FROM_ALL)
target_include_directories(glew PUBLIC "$<BUILD_INTERFACE:${MPP_GLEW_SOURCE_DIR}/include>")
target_compile_definitions(glew PUBLIC GLEW_NO_GLU)
add_library(GLEW::GLEW ALIAS glew)

# Assimp is built from the source submodule. Keep its optional tools, tests,
# samples, and install rules out of the application build.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ext/assimp/CMakeLists.txt")
    message(FATAL_ERROR "Assimp submodule is missing; run: git submodule update --init --recursive")
endif()
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "Do not build Assimp tools" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "Do not build Assimp samples" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "Do not build Assimp tests" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "Do not install Assimp" FORCE)
set(ASSIMP_INSTALL_PDB OFF CACHE BOOL "Do not install Assimp PDBs" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "Do not promote dependency warnings to errors" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "Build Assimp's bundled zlib" FORCE)
# Draco decompression for KHR_draco_mesh_compression glTF assets. Assimp leaves
# this off by default, which makes Draco-compressed glTF files load without
# geometry. The bundled Draco predates the warnings the project build enables,
# so it is compiled with its own relaxed flags (see ext/assimp/CMakeLists.txt).
set(ASSIMP_BUILD_DRACO ON CACHE BOOL "Build Draco for compressed glTF meshes" FORCE)
add_subdirectory(ext/assimp EXCLUDE_FROM_ALL)

# Draco follows the shared-library convention used by the rest of the
# dependencies, so its runtime has to travel with the executables that link
# Assimp. The target name differs by platform.
if(TARGET draco)
    set(MPP_DRACO_RUNTIME draco)
elseif(TARGET draco_shared)
    set(MPP_DRACO_RUNTIME draco_shared)
else()
    set(MPP_DRACO_RUNTIME "")
endif()

add_library(mpp_vendor_headers INTERFACE)
target_include_directories(mpp_vendor_headers INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/include")

function(mpp_target_defaults target)
    target_compile_features(${target} PUBLIC cxx_std_20)
    target_compile_definitions(${target} PRIVATE
        NOMINMAX
        UNICODE
        _UNICODE
        _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
        $<$<CONFIG:Debug>:_DEBUG>)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W3 /permissive-)
    endif()
endfunction()

# Add public/project headers to generated Visual Studio projects and arrange
# them under Header Files filters that mirror the on-disk include tree.
function(mpp_add_header_filters target)
    foreach(header_root IN LISTS ARGN)
        if(IS_DIRECTORY "${header_root}")
            file(GLOB_RECURSE target_headers CONFIGURE_DEPENDS
                "${header_root}/*.h"
                "${header_root}/*.hpp"
                "${header_root}/*.inl"
                "${header_root}/*.ipp")
            if(target_headers)
                target_sources(${target} PRIVATE ${target_headers})
                source_group(TREE "${header_root}" PREFIX "Header Files" FILES ${target_headers})
            endif()
        endif()
    endforeach()
endfunction()

add_custom_target(MppPublicHeaderAudit ALL
    COMMAND "${CMAKE_COMMAND}"
        -DMPP_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/AuditPublicHeaders.cmake"
    VERBATIM)

function(mpp_copy_runtime target)
    foreach(runtime IN LISTS ARGN)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:${runtime}>" "$<TARGET_FILE_DIR:${target}>"
            VERBATIM)
    endforeach()
endfunction()

# Utils owns its build in the source submodule. The additional build-interface
# include directory preserves this project's existing <utils/...> includes.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ext/utils/CMakeLists.txt")
    message(FATAL_ERROR "Utils submodule is missing; run: git submodule update --init --recursive")
endif()
add_subdirectory(ext/utils EXCLUDE_FROM_ALL)
target_include_directories(Utils
    BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/include"
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ext/utils/include>")

add_library(ImGui STATIC
    ext/imgui/src/imgui.cpp
    ext/imgui/src/imgui_demo.cpp
    ext/imgui/src/imgui_draw.cpp
    ext/imgui/src/imgui_tables.cpp
    ext/imgui/src/imgui_widgets.cpp)
target_include_directories(ImGui PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/imgui/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/imgui/include/imgui")
mpp_target_defaults(ImGui)
mpp_add_header_filters(ImGui "${CMAKE_CURRENT_SOURCE_DIR}/ext/imgui/include")

add_library(MppData SHARED
    mpp-data/src/StructuredData.cpp
)
target_include_directories(MppData PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/mpp-data/include")
target_compile_definitions(MppData PRIVATE MPP_DATA_DLL_EXPORT)
mpp_target_defaults(MppData)
mpp_add_header_filters(MppData "${CMAKE_CURRENT_SOURCE_DIR}/mpp-data/include")

add_library(MppMesh SHARED
    mpp-mesh/src/MaterialInformation.cpp
    mpp-mesh/src/MeshDefinition.cpp
    mpp-mesh/src/MeshSpecification.cpp
    mpp-mesh/src/Vertex.cpp
    mpp-mesh/src/VertexBufferAttributeLayout.cpp
    mpp-mesh/src/VertexBufferDefinition.cpp
    mpp-mesh/src/VertexData.cpp
)
target_include_directories(MppMesh PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/mpp-mesh/include")
target_compile_definitions(MppMesh PRIVATE MPP_MESH_DLL_EXPORT)
target_link_libraries(MppMesh PRIVATE mpp_vendor_headers)
mpp_target_defaults(MppMesh)
mpp_add_header_filters(MppMesh "${CMAKE_CURRENT_SOURCE_DIR}/mpp-mesh/include")

add_library(MppProgram SHARED
    mpp-program/include/mpp/program/glslRegistry.cpp
    mpp-program/src/Attribute.cpp
    mpp-program/src/Parser.cpp
    mpp-program/src/ShaderStage.cpp
)
target_include_directories(MppProgram
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/mpp-program/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/mpp-program/include/mpp/program")
target_compile_definitions(MppProgram PRIVATE MPP_PROGRAM_DLL_EXPORT)
target_link_libraries(MppProgram
    PUBLIC MppMesh
    PRIVATE Utils mpp_vendor_headers)
mpp_target_defaults(MppProgram)
mpp_add_header_filters(MppProgram "${CMAKE_CURRENT_SOURCE_DIR}/mpp-program/include")

add_library(MassivePolyPusher SHARED
    mpp/src/Batch.cpp
    mpp/src/BatchDataProvider.cpp
    mpp/src/BoxModelStream.cpp
    mpp/src/BufferDataProvider.cpp
    mpp/src/BufferRenderer.cpp
    mpp/src/Camera.cpp
    mpp/src/CircleBatch.cpp
    mpp/src/ClipRectangle.cpp
    mpp/src/Colour.cpp
    mpp/src/CylinderModelStream.cpp
    mpp/src/Diagnostic.cpp
    mpp/src/DiagnosticTests.cpp
    mpp/src/FileDataStream.cpp
    mpp/src/Font.cpp
    mpp/src/GLErrorCheck.cpp
    mpp/src/GpuDebugScope.cpp
    mpp/src/InternalFont.cpp
    mpp/src/IblEnvironmentCache.cpp
    mpp/src/LineBatch.cpp
    mpp/src/Logger.cpp
    mpp/src/BasicMaterial.cpp
    mpp/src/PbrMaterial.cpp
    mpp/src/PbrPipelineDocument.cpp
    mpp/src/PbrPipelineStream.cpp
    mpp/src/PbrPipelineTemplate.cpp
    mpp/src/PbrMaterialFeatures.cpp
    mpp/src/PbrMaterialTests.cpp
    mpp/src/PbrMaterialStream.cpp
    mpp/src/ProgrammaticPbrMaterialStream.cpp
    mpp/src/BasicMaterialStream.cpp
    mpp/src/Mesh.cpp
    mpp/src/MeshInstance.cpp
    mpp/src/Model.cpp
    mpp/src/ModelInstance.cpp
    mpp/src/ModelSerializer.cpp
    mpp/src/ModelStream.cpp
    mpp/src/MppException.cpp
    mpp/src/MppModelStream.cpp
    mpp/src/PersistentMappedBuffer.cpp
    mpp/src/PostEffectMaterial.cpp
    mpp/src/PostEffectMaterialStream.cpp
    mpp/src/ProgrammaticPostEffectMaterialStream.cpp
    mpp/src/PrimitiveModelStream.cpp
    mpp/src/Profiler.cpp
    mpp/src/Program.cpp
    mpp/src/ProgrammaticBasicMaterialStream.cpp
    mpp/src/ProgrammaticModelStream.cpp
    mpp/src/ProgrammaticProgramStream.cpp
    mpp/src/ProgrammaticRenderTextureStream.cpp
    mpp/src/ProgrammaticSamplerStream.cpp
    mpp/src/ProgrammaticStringStream.cpp
    mpp/src/ProgrammaticTextureStream.cpp
    mpp/src/ProgramStream.cpp
    mpp/src/QuadBatch.cpp
    mpp/src/RenderGraph.cpp
    mpp/src/RenderGraphExecutor.cpp
    mpp/src/RenderGraphStream.cpp
    mpp/src/RenderGraphTemplate.cpp
    mpp/src/RenderGraphPassFactoryRegistry.cpp
    mpp/src/RenderGraphImportRegistry.cpp
    mpp/src/RenderGraphGpuTests.cpp
    mpp/src/RenderGraphTests.cpp
    mpp/src/RenderGraphBuiltInPasses.cpp
    mpp/src/RenderGraphTargets.cpp
    mpp/src/RenderOutputProcessor.cpp
    mpp/src/RenderPass.cpp
    mpp/src/RenderPipeline.cpp
    mpp/src/RenderPipelineFlow.cpp
    mpp/src/RenderSystem.cpp
    mpp/src/RenderTarget.cpp
    mpp/src/RenderTexture.cpp
    mpp/src/RenderTextureStream.cpp
    mpp/src/Resource.cpp
    mpp/src/ResourceManager.cpp
    mpp/src/ResourceStream.cpp
    mpp/src/ResourceStreamSerializer.cpp
    mpp/src/ResourceWrangler.cpp
    mpp/src/Sampler.cpp
    mpp/src/SamplerParams.cpp
    mpp/src/SamplerStream.cpp
    mpp/src/Scene.cpp
    mpp/src/SceneDocument.cpp
    mpp/src/SceneStream.cpp
    mpp/src/SceneRuntime.cpp
    mpp/src/SceneTemplate.cpp
    mpp/src/SceneModel2d.cpp
    mpp/src/SceneModel3d.cpp
    mpp/src/Screen.cpp
    mpp/src/SphereModelStream.cpp
    mpp/src/StaticLogger.cpp
    mpp/src/String.cpp
    mpp/src/StringStream.cpp
    mpp/src/Texture.cpp
    mpp/src/TextureData.cpp
    mpp/src/TextureParams.cpp
    mpp/src/TextureRenderer.cpp
    mpp/src/TextureStream.cpp
    mpp/src/GridModelStream.cpp
    mpp/src/TextureStreamBase.cpp
    mpp/src/TriangleBatch.cpp
    mpp/src/UniformBuffer.cpp
    mpp/src/UniformCollection.cpp
    mpp/src/VertexBuffer.cpp
)
target_include_directories(MassivePolyPusher PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/mpp/include")
target_compile_definitions(MassivePolyPusher PRIVATE
    MPP_DLL_EXPORT _ENABLE_EXTENDED_ALIGNED_STORAGE
    $<$<CONFIG:Debug>:MPP_DEBUG_BUILD>)
target_link_libraries(MassivePolyPusher
    PUBLIC MppData MppMesh MppProgram mpp_vendor_headers
    PRIVATE Utils OpenGL::GL GLEW::GLEW)
mpp_target_defaults(MassivePolyPusher)
mpp_add_header_filters(MassivePolyPusher "${CMAKE_CURRENT_SOURCE_DIR}/mpp/include")

add_library(MppResourceParsers SHARED
    mpp-resource-parsers/src/FileBasicMaterialStream.cpp
    mpp-resource-parsers/src/FileMaterialStream.cpp
    mpp-resource-parsers/src/FilePbrMaterialStream.cpp
    mpp-resource-parsers/src/FilePostEffectMaterialStream.cpp
    mpp-resource-parsers/src/FileGltfPbrMaterialStream.cpp
    mpp-resource-parsers/src/GltfPbrMaterialLoader.cpp
    mpp-resource-parsers/src/FilePbrPipelineStream.cpp
    mpp-resource-parsers/src/FileRenderGraphStream.cpp
    mpp-resource-parsers/src/FileProgramStream.cpp
    mpp-resource-parsers/src/FileSamplerStream.cpp
    mpp-resource-parsers/src/FileStream.cpp
    mpp-resource-parsers/src/FileStringStream.cpp
    mpp-resource-parsers/src/FileSceneStream.cpp
    mpp-resource-parsers/src/FileTextureStream.cpp
    mpp-resource-parsers/src/MeshSpecificationParser.cpp
    mpp-resource-parsers/src/MaterialResourceTests.cpp
    mpp-resource-parsers/src/RenderGraphParser.cpp
    mpp-resource-parsers/src/PbrPipelineDocumentLoader.cpp
    mpp-resource-parsers/src/PbrPipelineParser.cpp
    mpp-resource-parsers/src/PbrPipelineSerializer.cpp
    mpp-resource-parsers/src/PbrPipelineRuntime.cpp
    mpp-resource-parsers/src/PbrPipelineResourceValidator.cpp
    mpp-resource-parsers/src/SceneParser.cpp
    mpp-resource-parsers/src/SceneSerializer.cpp
    mpp-resource-parsers/src/RenderGraphSerializer.cpp
    mpp-resource-parsers/src/RenderGraphResourceTests.cpp
    mpp-resource-parsers/src/Serializer.cpp
    mpp-resource-parsers/src/XmlSerializer.cpp
)
target_include_directories(MppResourceParsers PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/mpp-resource-parsers/include")
target_compile_definitions(MppResourceParsers PRIVATE MPP_RESOURCE_PARSERS_DLL_EXPORT)
target_link_libraries(MppResourceParsers
    PUBLIC MppData MassivePolyPusher MppProgram MppMesh
    PRIVATE Utils GLEW::GLEW)
mpp_target_defaults(MppResourceParsers)
mpp_add_header_filters(MppResourceParsers "${CMAKE_CURRENT_SOURCE_DIR}/mpp-resource-parsers/include")

add_library(MppMeshSpecificationParser SHARED
    mpp-mesh-specification-parser/src/ModelspecStream.cpp
    mpp-mesh-specification-parser/src/ProgramInformation.cpp
    mpp-mesh-specification-parser/src/SpecificationParser.cpp
)
target_include_directories(MppMeshSpecificationParser PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/mpp-mesh-specification-parser/include")
target_compile_definitions(MppMeshSpecificationParser PRIVATE MPP_MESH_SPECIFICATION_PARSER_DLL_EXPORT)
target_link_libraries(MppMeshSpecificationParser
    PUBLIC MassivePolyPusher MppMesh MppResourceParsers
    PRIVATE Utils)
mpp_target_defaults(MppMeshSpecificationParser)
mpp_add_header_filters(MppMeshSpecificationParser "${CMAKE_CURRENT_SOURCE_DIR}/mpp-mesh-specification-parser/include")

add_library(MppHelper SHARED
    mpp-helper/src/FpsCamera.cpp
    mpp-helper/src/FreeCamera.cpp
    mpp-helper/src/OrthoCamera.cpp
)
target_include_directories(MppHelper PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/mpp-helper/include")
target_compile_definitions(MppHelper PRIVATE MPP_HELPER_DLL_EXPORT)
target_link_libraries(MppHelper PUBLIC MassivePolyPusher MppMesh)
mpp_target_defaults(MppHelper)
mpp_add_header_filters(MppHelper "${CMAKE_CURRENT_SOURCE_DIR}/mpp-helper/include")

add_library(MppAppSupport STATIC
        mpp-app-support/src/BackgroundWork.cpp
        mpp-app-support/src/CommandStack.cpp
        mpp-app-support/src/DocumentFile.cpp
        mpp-app-support/src/DocumentFoundationTests.cpp
        mpp-app-support/src/DocumentId.cpp
        mpp-app-support/src/FileDialog.cpp
        mpp-app-support/src/ImGuiPlatform.cpp
        mpp-app-support/src/PackageManifest.cpp
        mpp-app-support/src/RenderSystemConfig.cpp
        mpp-app-support/src/RenderSystemConfigTests.cpp
        mpp-app-support/src/ImageLoader.cpp
        mpp-app-support/src/InputManager.cpp
        mpp-app-support/src/InputManagerSDL.cpp
        mpp-app-support/src/Timer.cpp
        mpp-app-support/src/TimerSDL.cpp
        mpp-app-support/src/Window.cpp
        mpp-app-support/src/WindowSDL.cpp
        mpp-app-support/src/ZipArchive.cpp)
    target_include_directories(MppAppSupport
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/mpp-app-support/include"
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/vendor/include")
target_link_libraries(MppAppSupport
    PUBLIC MassivePolyPusher MppMesh MppProgram Utils ImGui SDL3::SDL3
    PRIVATE GLEW::GLEW)
if(WIN32)
    target_link_libraries(MppAppSupport PRIVATE comdlg32 shell32 ole32)
endif()
mpp_target_defaults(MppAppSupport)
mpp_add_header_filters(MppAppSupport "${CMAKE_CURRENT_SOURCE_DIR}/mpp-app-support/include")

add_executable(ProgramBuilder program-builder/src/Main.cpp)
target_include_directories(ProgramBuilder PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/program-builder/include")
target_compile_definitions(ProgramBuilder PRIVATE _CONSOLE)
target_link_libraries(ProgramBuilder PRIVATE MppMesh MppProgram MppMeshSpecificationParser Utils)
mpp_target_defaults(ProgramBuilder)
mpp_add_header_filters(ProgramBuilder "${CMAKE_CURRENT_SOURCE_DIR}/program-builder/include")

add_executable(ModelConvert
    model-convert/src/AssImpModelLoader.cpp
    model-convert/src/Main.cpp
)
target_include_directories(ModelConvert PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/model-convert/include")
target_link_libraries(ModelConvert PRIVATE
    MppMesh MppMeshSpecificationParser MppResourceParsers MassivePolyPusher Utils assimp::assimp)
mpp_target_defaults(ModelConvert)
mpp_add_header_filters(ModelConvert "${CMAKE_CURRENT_SOURCE_DIR}/model-convert/include")
mpp_copy_runtime(ModelConvert assimp ${MPP_DRACO_RUNTIME} glew Utils MppData MassivePolyPusher MppMesh MppProgram MppResourceParsers MppMeshSpecificationParser)

if(WIN32)
    add_executable(PipelineEditor WIN32
        pipeline-editor/src/Main.cpp
        pipeline-editor/src/ProcessFlowModel.cpp
        pipeline-editor/src/ProcessFlowLayout.cpp
        pipeline-editor/src/ProcessFlowView.cpp)
    target_include_directories(PipelineEditor PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/pipeline-editor/include")
    target_link_libraries(PipelineEditor PRIVATE
        MppAppSupport MppResourceParsers MassivePolyPusher MppMesh MppProgram
        Utils ImGui SDL3::SDL3 OpenGL::GL GLEW::GLEW)
    mpp_target_defaults(PipelineEditor)
    mpp_add_header_filters(PipelineEditor "${CMAKE_CURRENT_SOURCE_DIR}/pipeline-editor/include")
    mpp_copy_runtime(PipelineEditor SDL3-shared glew Utils MppData MassivePolyPusher MppMesh MppProgram MppResourceParsers)
    add_custom_command(TARGET PipelineEditor POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/pipeline-editor/editor.ini"
            "$<TARGET_FILE_DIR:PipelineEditor>/editor.ini"
        VERBATIM)
else()
    message(STATUS "Skipping PipelineEditor: its RenderDoc/process integration is Windows-only")
endif()

add_subdirectory(demo-suite)
