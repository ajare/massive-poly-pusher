cmake_minimum_required(VERSION 3.20)
project(MassivePolyPusher VERSION 0.1 LANGUAGES C CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "The current MassivePolyPusher sources and vendored libraries support Windows only")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_DEBUG_POSTFIX d)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

find_package(OpenGL REQUIRED)

# SDL is built from the source submodule. SDL 2.0.14 predates CMake 3.5 policies.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum policy version for legacy dependencies")
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ext/sdl/CMakeLists.txt")
    message(FATAL_ERROR "SDL submodule is missing; run: git submodule update --init --recursive")
endif()
set(SDL_SHARED ON CACHE BOOL "Build SDL as a shared library" FORCE)
set(SDL_STATIC OFF CACHE BOOL "Do not build the SDL static library" FORCE)
set(SDL_TEST OFF CACHE BOOL "Do not build SDL tests" FORCE)
add_subdirectory(ext/sdl EXCLUDE_FROM_ALL)

add_library(mpp_vendor_headers INTERFACE)
target_include_directories(mpp_vendor_headers INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/include")

function(mpp_import_vendor_library target release_lib debug_lib release_dll debug_dll)
    add_library(${target} SHARED IMPORTED GLOBAL)
    set_target_properties(${target} PROPERTIES
        IMPORTED_CONFIGURATIONS "Debug;Release"
        IMPORTED_IMPLIB_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/lib/vs2026/x64/Release/${release_lib}"
        IMPORTED_IMPLIB_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/vendor/lib/vs2026/x64/Debug/${debug_lib}"
        IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/bin/vs2026/x64/Release/${release_dll}"
        IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/vendor/bin/vs2026/x64/Debug/${debug_dll}"
        MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
        MAP_IMPORTED_CONFIG_MINSIZEREL Release)
endfunction()

mpp_import_vendor_library(FreeImage FreeImage.lib FreeImage.lib FreeImage.dll FreeImage.dll)
mpp_import_vendor_library(GLEW glew32.lib glew32.lib glew32.dll glew32.dll)
mpp_import_vendor_library(Assimp assimp-vc143-mt.lib assimp-vc143-mtd.lib assimp-vc143-mt.dll assimp-vc143-mtd.dll)

target_link_libraries(FreeImage INTERFACE mpp_vendor_headers)
target_link_libraries(GLEW INTERFACE mpp_vendor_headers)
target_link_libraries(Assimp INTERFACE mpp_vendor_headers)

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

function(mpp_copy_runtime target)
    foreach(runtime IN LISTS ARGN)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:${runtime}>" "$<TARGET_FILE_DIR:${target}>"
            VERBATIM)
    endforeach()
endfunction()

# The utils submodule has no CMake project, so mirror its VS2026 target here
# without changing the submodule itself.
add_library(Utils SHARED
    ext/utils/src/FileSystem.cpp
    ext/utils/src/Image.cpp
    ext/utils/src/StringUtils.cpp
    ext/utils/src/StructuredData.cpp
    ext/utils/src/tinyxml2.cpp
    ext/utils/src/XmlReader.cpp
    ext/utils/src/XmlWriter.cpp)
target_include_directories(Utils PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/utils/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/ext/utils/include/utils"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/ext/utils/vendor/include")
target_compile_definitions(Utils PRIVATE UTILS_DLL_EXPORT)
target_link_libraries(Utils PRIVATE FreeImage)
mpp_target_defaults(Utils)
mpp_add_header_filters(Utils "${CMAKE_CURRENT_SOURCE_DIR}/ext/utils/include")

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

add_library(MppMesh SHARED
    mpp-mesh/src/MaterialInformation.cpp
    mpp-mesh/src/MeshDefinition.cpp
    mpp-mesh/src/MeshSpecification.cpp
    mpp-mesh/src/Vertex.cpp
    mpp-mesh/src/VertexBufferAttributeLayout.cpp
    mpp-mesh/src/VertexBufferDefinition.cpp
    mpp-mesh/src/VertexData.cpp
    mpp-mesh/build/version/Version.rc)
target_include_directories(MppMesh PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/mpp-mesh/include")
target_compile_definitions(MppMesh PRIVATE MPP_MESH_DLL_EXPORT)
target_link_libraries(MppMesh PUBLIC mpp_vendor_headers)
mpp_target_defaults(MppMesh)
mpp_add_header_filters(MppMesh "${CMAKE_CURRENT_SOURCE_DIR}/mpp-mesh/include")

add_library(MppProgram SHARED
    mpp-program/include/mpp/program/glslRegistry.cpp
    mpp-program/src/Attribute.cpp
    mpp-program/src/Parser.cpp
    mpp-program/src/ShaderStage.cpp
    mpp-program/build/version/Version.rc)
target_include_directories(MppProgram
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/mpp-program/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/mpp-program/include/mpp/program")
target_compile_definitions(MppProgram PRIVATE MPP_PROGRAM_DLL_EXPORT)
target_link_libraries(MppProgram PUBLIC MppMesh Utils mpp_vendor_headers)
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
    mpp/src/PostEffect.cpp
    mpp/src/PostEffectStream.cpp
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
    mpp/build/version/Version.rc)
target_include_directories(MassivePolyPusher PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/mpp/include")
target_compile_definitions(MassivePolyPusher PRIVATE
    MPP_DLL_EXPORT _ENABLE_EXTENDED_ALIGNED_STORAGE
    $<$<CONFIG:Debug>:MPP_DEBUG_BUILD>)
target_link_libraries(MassivePolyPusher
    PUBLIC MppMesh MppProgram Utils mpp_vendor_headers
    PRIVATE OpenGL::GL GLEW)
mpp_target_defaults(MassivePolyPusher)
mpp_add_header_filters(MassivePolyPusher "${CMAKE_CURRENT_SOURCE_DIR}/mpp/include")

add_library(MppResourceParsers SHARED
    mpp-resource-parsers/src/FileBasicMaterialStream.cpp
    mpp-resource-parsers/src/FileMaterialStream.cpp
    mpp-resource-parsers/src/FilePbrMaterialStream.cpp
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
    mpp-resource-parsers/build/version/Version.rc)
target_include_directories(MppResourceParsers PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/mpp-resource-parsers/include")
target_compile_definitions(MppResourceParsers PRIVATE MPP_RESOURCE_PARSERS_DLL_EXPORT)
target_link_libraries(MppResourceParsers PUBLIC MassivePolyPusher MppProgram MppMesh Utils)
mpp_target_defaults(MppResourceParsers)
mpp_add_header_filters(MppResourceParsers "${CMAKE_CURRENT_SOURCE_DIR}/mpp-resource-parsers/include")

add_library(MppMeshSpecificationParser SHARED
    mpp-mesh-specification-parser/src/ModelspecStream.cpp
    mpp-mesh-specification-parser/src/ProgramInformation.cpp
    mpp-mesh-specification-parser/src/SpecificationParser.cpp
    mpp-mesh-specification-parser/build/version/Version.rc)
target_include_directories(MppMeshSpecificationParser PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/mpp-mesh-specification-parser/include")
target_compile_definitions(MppMeshSpecificationParser PRIVATE MPP_MESH_SPECIFICATION_PARSER_DLL_EXPORT)
target_link_libraries(MppMeshSpecificationParser PUBLIC MassivePolyPusher MppMesh MppResourceParsers Utils)
mpp_target_defaults(MppMeshSpecificationParser)
mpp_add_header_filters(MppMeshSpecificationParser "${CMAKE_CURRENT_SOURCE_DIR}/mpp-mesh-specification-parser/include")

add_library(MppHelper SHARED
    mpp-helper/src/FpsCamera.cpp
    mpp-helper/src/FreeCamera.cpp
    mpp-helper/src/OrthoCamera.cpp
    mpp-helper/build/version/Version.rc)
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
target_include_directories(MppAppSupport PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/mpp-app-support/include")
target_link_libraries(MppAppSupport PUBLIC
    MassivePolyPusher MppMesh MppProgram Utils ImGui SDL2 FreeImage
    PRIVATE comdlg32 shell32 ole32)
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
    model-convert/build/version/Version.rc)
target_include_directories(ModelConvert PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/model-convert/include")
target_link_libraries(ModelConvert PRIVATE
    MppMesh MppMeshSpecificationParser MppResourceParsers MassivePolyPusher Utils Assimp)
mpp_target_defaults(ModelConvert)
mpp_add_header_filters(ModelConvert "${CMAKE_CURRENT_SOURCE_DIR}/model-convert/include")
mpp_copy_runtime(ModelConvert Assimp FreeImage GLEW SDL2 Utils MassivePolyPusher MppMesh MppProgram MppResourceParsers MppMeshSpecificationParser)

add_executable(PipelineEditor WIN32
    pipeline-editor/src/Main.cpp
    pipeline-editor/src/ProcessFlowModel.cpp
    pipeline-editor/src/ProcessFlowLayout.cpp
    pipeline-editor/src/ProcessFlowView.cpp)
target_include_directories(PipelineEditor PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/pipeline-editor/include")
target_link_libraries(PipelineEditor PRIVATE
    MppAppSupport MppResourceParsers MassivePolyPusher MppMesh MppProgram
    Utils ImGui SDL2 SDL2main FreeImage OpenGL::GL GLEW)
mpp_target_defaults(PipelineEditor)
mpp_add_header_filters(PipelineEditor "${CMAKE_CURRENT_SOURCE_DIR}/pipeline-editor/include")
mpp_copy_runtime(PipelineEditor SDL2 FreeImage GLEW Utils MassivePolyPusher MppMesh MppProgram MppResourceParsers)
add_custom_command(TARGET PipelineEditor POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/pipeline-editor/editor.ini"
        "$<TARGET_FILE_DIR:PipelineEditor>/editor.ini"
    VERBATIM)

add_subdirectory(demo-suite)
