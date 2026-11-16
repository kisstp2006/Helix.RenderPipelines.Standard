cmake_minimum_required(VERSION 3.28)
project(Helix.RenderPipelines.Standard LANGUAGES CXX)

if(NOT COMMAND helix_add_library)
    include("${CMAKE_CURRENT_SOURCE_DIR}/External/Helix.CMake/Helix.cmake")
endif()

helix_require(RenderPipelines.Core)
include("${HELIX_BINDGEN_ROOT}/CMake/HelixBindgen.cmake")

find_program(HELIX_DOTNET NAMES dotnet HINTS "$ENV{HELIX_DOTNET_ROOT}" "$ENV{DOTNET_ROOT}" REQUIRED)
set(standard_artifacts "${CMAKE_BINARY_DIR}/HelixRenderPipelinesStandard")
helix_dependency_dir(shaders_dir Shaders)
helix_dependency_dir(engine_dir Engine)
helix_dependency_dir(math_dir Math)
helix_dependency_dir(core_dir RenderPipelines.Core)
set(engine_source "${engine_dir}/Source")
set(core_source "${core_dir}/Source")
set(standard_source "${CMAKE_CURRENT_SOURCE_DIR}/Source")

# The pipeline in C++: a native module (HelixRenderPipelinesStandard) over the engine and the core, a shared library
# beside theirs (a static one linked into the app on the Web). C# reaches it through its bindings.
file(GLOB_RECURSE standard_cpp CONFIGURE_DEPENDS "${standard_source}/Helix/RenderPipelines/Standard/*.cpp")
file(GLOB_RECURSE standard_headers CONFIGURE_DEPENDS "${standard_source}/Helix/RenderPipelines/Standard/*.h")
if(HELIX_PLATFORM STREQUAL "web")
    add_library(helix_render_pipelines_standard STATIC ${standard_cpp} ${standard_headers})
else()
    add_library(helix_render_pipelines_standard SHARED ${standard_cpp} ${standard_headers})
    set_target_properties(helix_render_pipelines_standard PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
                                                                     LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
target_link_libraries(helix_render_pipelines_standard PUBLIC Helix::RenderPipelines.Core)
add_library(Helix::RenderPipelines.Standard ALIAS helix_render_pipelines_standard)
set_target_properties(helix_render_pipelines_standard PROPERTIES OUTPUT_NAME HelixRenderPipelinesStandard)
target_include_directories(helix_render_pipelines_standard PUBLIC "${standard_source}")
helix_apply_options(helix_render_pipelines_standard)
# The headers whose tagged types C# sees.
set(standard_bound_headers "${standard_source}/Helix/RenderPipelines/Standard/Settings/PipelineSettings.h"
    "${standard_source}/Helix/RenderPipelines/Standard/Renderer/PostProcessData.h"
    "${standard_source}/Helix/RenderPipelines/Standard/Renderer/ScriptableRendererFeature.h"
    "${standard_source}/Helix/RenderPipelines/Standard/Renderer/RendererData.h"
    "${standard_source}/Helix/RenderPipelines/Standard/Pipeline/UniversalRenderPipelineAsset.h"
    "${standard_source}/Helix/RenderPipelines/Standard/Pipeline/UniversalRenderPipeline.h"
    "${standard_source}/Helix/RenderPipelines/Standard/Components/AdditionalData.h"
    "${standard_source}/Helix/RenderPipelines/Standard/Frame/FrameData.h"
    "${standard_source}/Helix/RenderPipelines/Standard/Passes/ScriptableRenderPass.h"
    "${standard_source}/Helix/RenderPipelines/Standard/Renderer/ScriptableRenderer.h"
    "${standard_source}/Helix/RenderPipelines/Standard/Overrides/Overrides.h")
# The core's bound headers (its types the pipeline's bindings name).
set(core_references "${core_source}/Helix/RenderPipelines/Core/Volume/VolumeComponent.h" "${core_source}/Helix/RenderPipelines/Core/Volume/VolumeParameter.h"
    "${core_source}/Helix/RenderPipelines/Core/Context/ContextContainer.h")
set(engine_references "${engine_source}/Helix/Engine/Object/EntityId.h" "${engine_source}/Helix/Engine/Object/Object.h"
    "${engine_source}/Helix/Engine/GameObject/GameObject.h" "${engine_source}/Helix/Engine/Script/GameBehaviour.h"
    "${engine_source}/Helix/Engine/Script/ScriptableObject.h" "${engine_source}/Helix/Engine/Transform/Transform.h"
    "${engine_source}/Helix/Engine/Texture/Texture.h" "${engine_source}/Helix/Engine/Material/Material.h" "${engine_source}/Helix/Engine/Shader/Shader.h"
    "${engine_source}/Helix/Engine/Rendering/Camera.h" "${engine_source}/Helix/Engine/Rendering/Light.h"
    "${engine_source}/Helix/Engine/Rendering/Pipeline/RenderStates.h" "${engine_source}/Helix/Engine/Texture/RenderTexture.h"
    "${engine_source}/Helix/Engine/Rendering/SystemInfo.h" "${engine_source}/Helix/Engine/Mesh/Mesh.h"
    "${math_dir}/Source/Helix/Math/Vector.h" "${math_dir}/Source/Helix/Math/Color.h" "${math_dir}/Source/Helix/Math/Matrix4x4.h"
    "${math_dir}/Source/Helix/Math/Geometry.h")
helix_bindgen(helix_render_pipelines_standard MODULE HelixRenderPipelinesStandard NAMESPACE HelixEngine.Rendering.Universal KIND NativeGame
              HEADERS ${standard_bound_headers} REFERENCES ${engine_references} ${core_references} REFERENCE_NAMESPACE HelixEngine
              INCLUDE_ROOTS "${standard_source}" "${core_source}" "${engine_source}" "${math_dir}/Source" CS_OUTPUT standard_cs)

# The C# side: the generated bindings and the hand written C# in the reference's shape, over HelixEngine.CoreModule
# and the core's runtime.
set(runtime_dir "${standard_source}/Helix.RenderPipelines.Standard.Runtime")
set(runtime_dll "${standard_artifacts}/bin/Helix.RenderPipelines.Standard.Runtime/release/Helix.RenderPipelines.Standard.Runtime.dll")
file(GLOB_RECURSE runtime_sources CONFIGURE_DEPENDS "${runtime_dir}/*.cs" "${runtime_dir}/*.csproj")
list(FILTER runtime_sources EXCLUDE REGEX "/(bin|obj)/")
add_custom_command(
    OUTPUT "${runtime_dll}"
    COMMAND "${HELIX_DOTNET}" build "${runtime_dir}/Helix.RenderPipelines.Standard.Runtime.csproj" -c Release --artifacts-path "${standard_artifacts}"
            --nologo -v quiet "-p:HelixCoreModule=${HELIX_CORE_MODULE}" "-p:HelixScriptingRuntime=${HELIX_SCRIPTING_RUNTIME}"
            "-p:HelixRenderPipelinesCore=${HELIX_RENDER_PIPELINES_CORE}" "-p:HelixModuleBindings=${standard_cs}"
    DEPENDS ${runtime_sources} "${HELIX_CORE_MODULE}" "${HELIX_RENDER_PIPELINES_CORE}" "${standard_cs}"
    COMMENT "Building Helix.RenderPipelines.Standard.Runtime")
add_custom_target(helix_render_pipelines_standard_runtime ALL DEPENDS "${runtime_dll}")
add_dependencies(helix_render_pipelines_standard_runtime helix_core_module helix_render_pipelines_core_runtime helix_render_pipelines_standard)

# The pipeline's C# shaders (with the core's shader library beside them).
set(standard_shaders_bin "${standard_artifacts}/bin/Helix.RenderPipelines.Standard.Shaders/release")
set(standard_shaders_dll "${standard_shaders_bin}/Helix.RenderPipelines.Standard.Shaders.dll")
file(GLOB_RECURSE standard_shader_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.cs" "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.csproj"
     "${core_dir}/ShaderLibrary/*.cs" "${shaders_dir}/Source/*.cs" "${engine_dir}/Shaders/*.cs")
list(FILTER standard_shader_sources EXCLUDE REGEX "/(bin|obj)/")
add_custom_command(
    OUTPUT "${standard_shaders_dll}"
    COMMAND "${HELIX_DOTNET}" build "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Helix.RenderPipelines.Standard.Shaders.csproj" -c Release
            --artifacts-path "${standard_artifacts}" --nologo -v quiet "-p:HelixShadersDir=${shaders_dir}" "-p:HelixEngineDir=${engine_dir}"
            "-p:HelixRenderPipelinesCoreDir=${core_dir}"
    DEPENDS ${standard_shader_sources}
    COMMENT "Building the Standard pipeline's C# shaders")
add_custom_target(helix_render_pipelines_standard_shaders ALL DEPENDS "${standard_shaders_dll}")
add_dependencies(helix_render_pipelines_standard helix_render_pipelines_standard_shaders)
set(HELIX_RENDER_PIPELINES_STANDARD "${runtime_dll}" CACHE INTERNAL "")
set(HELIX_RENDER_PIPELINES_STANDARD_SHADERS "${standard_shaders_dll}" CACHE INTERNAL "")

# Tests: C# test scripts run by a C++ program in the engine, with the engine's test shaders, the core's and the
# pipeline's shaders as assets: the shared engine on the desktop, a .NET web app in node on the Web.
if(HELIX_PLATFORM STREQUAL "windows" OR HELIX_PLATFORM STREQUAL "linux")
    helix_add_test_suite(helix_render_pipelines_standard_tests EXCLUDE "/(Managed|Shaders)/" DEPS Helix::EngineShared Helix::RenderPipelines.Standard)
elseif(HELIX_PLATFORM STREQUAL "web")
    helix_add_test_suite(helix_render_pipelines_standard_tests DOTNET EXCLUDE "/(Managed|Shaders)/" DEPS Helix::Engine Helix::RenderPipelines.Standard)
endif()
if(TARGET helix_render_pipelines_standard_tests)
    set(tests_dir "${CMAKE_CURRENT_SOURCE_DIR}/Tests")
    # The test program's own game module: C++ features and passes; their C# classes are compiled into the C# tests.
    helix_bindgen(helix_render_pipelines_standard_tests MODULE RenderPipelinesStandardTests NAMESPACE StandardTests KIND NativeGame
                  HEADERS "${tests_dir}/Cpp/CppFeatures.h" REFERENCES ${engine_references} ${core_references} ${standard_bound_headers}
                  REFERENCE_NAMESPACE HelixEngine INCLUDE_ROOTS "${tests_dir}" "${standard_source}" "${core_source}" "${engine_source}" "${math_dir}/Source"
                  CS_OUTPUT test_bindings_cs)
    set(tests_dll "${standard_artifacts}/bin/Helix.RenderPipelines.Standard.Tests/release/Helix.RenderPipelines.Standard.Tests.dll")
    get_filename_component(tests_bin "${tests_dll}" DIRECTORY)
    file(GLOB_RECURSE test_sources CONFIGURE_DEPENDS "${tests_dir}/Managed/*.cs" "${tests_dir}/Managed/*.csproj")
    list(FILTER test_sources EXCLUDE REGEX "/(bin|obj)/")
    add_custom_command(
        OUTPUT "${tests_dll}"
        COMMAND "${HELIX_DOTNET}" build "${tests_dir}/Managed/Helix.RenderPipelines.Standard.Tests.csproj" -c Release --artifacts-path "${standard_artifacts}"
                --nologo -v quiet "-p:HelixCoreModule=${HELIX_CORE_MODULE}" "-p:HelixRenderPipelinesCore=${HELIX_RENDER_PIPELINES_CORE}"
                "-p:HelixRenderPipelinesStandard=${runtime_dll}" "-p:HelixScriptingRuntime=${HELIX_SCRIPTING_RUNTIME}"
                "-p:HelixModuleBindings=${test_bindings_cs}"
        DEPENDS ${test_sources} "${runtime_dll}" "${test_bindings_cs}"
        COMMENT "Building the C# tests of Helix.RenderPipelines.Standard")
    add_custom_target(helix_render_pipelines_standard_test_scripts DEPENDS "${tests_dll}")
    add_dependencies(helix_render_pipelines_standard_test_scripts helix_render_pipelines_standard_runtime)
    add_dependencies(helix_render_pipelines_standard_tests helix_render_pipelines_standard_test_scripts helix_render_pipelines_standard_shaders
                     helix_render_pipelines_core_shaders)

    # The engine's C# test shaders built against this repo's Helix.Shaders, the core's shaders and shader library, and
    # the pipeline's shaders: embedded, written into an asset folder by the tests.
    set(test_shaders_dir "${standard_artifacts}/bin/HelixEngine.TestShaders/release")
    file(GLOB_RECURSE test_shader_sources CONFIGURE_DEPENDS "${engine_dir}/Tests/Shaders/*.cs" "${engine_dir}/Tests/Shaders/*.csproj"
         "${engine_dir}/Shaders/*.cs" "${engine_dir}/Shaders/*.csproj")
    list(FILTER test_shader_sources EXCLUDE REGEX "/(bin|obj)/")
    add_custom_command(
        OUTPUT "${test_shaders_dir}/HelixEngine.TestShaders.dll" "${test_shaders_dir}/HelixEngine.TestShaders.pdb"
        COMMAND "${HELIX_DOTNET}" build "${engine_dir}/Tests/Shaders/HelixEngine.TestShaders.csproj" -c Release --artifacts-path "${standard_artifacts}"
                --nologo -v quiet "-p:HelixShadersDir=${shaders_dir}"
        DEPENDS ${test_shader_sources}
        COMMENT "Building the engine's C# test shaders")
    # The pipeline's test shaders (its light modes, its lights and shadows).
    set(standard_test_shaders_dll "${standard_artifacts}/bin/Helix.RenderPipelines.Standard.TestShaders/release/Helix.RenderPipelines.Standard.TestShaders.dll")
    file(GLOB_RECURSE standard_test_shader_sources CONFIGURE_DEPENDS "${tests_dir}/Shaders/*.cs" "${tests_dir}/Shaders/*.csproj")
    list(FILTER standard_test_shader_sources EXCLUDE REGEX "/(bin|obj)/")
    add_custom_command(
        OUTPUT "${standard_test_shaders_dll}"
        COMMAND "${HELIX_DOTNET}" build "${tests_dir}/Shaders/Helix.RenderPipelines.Standard.TestShaders.csproj" -c Release --artifacts-path "${standard_artifacts}"
                --nologo -v quiet "-p:HelixShadersDir=${shaders_dir}" "-p:HelixEngineDir=${engine_dir}"
        DEPENDS ${standard_test_shader_sources}
        COMMENT "Building the Standard pipeline's C# test shaders")
    set(tests_generated "${CMAKE_CURRENT_BINARY_DIR}/Generated")
    set(embedded "${tests_generated}/TestShadersAssembly.h" "${tests_generated}/CoreShadersAssembly.h" "${tests_generated}/CoreShaderLibraryAssembly.h"
        "${tests_generated}/StandardShadersAssembly.h" "${tests_generated}/StandardTestShadersAssembly.h")
    add_custom_command(
        OUTPUT ${embedded}
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${tests_generated}"
        COMMAND "${CMAKE_COMMAND}" "-DINPUT=${test_shaders_dir}/HelixEngine.TestShaders.dll" "-DOUTPUT=${tests_generated}/TestShadersAssembly.h"
                -DNAME=kTestShadersAssembly -P "${HELIX_EMBED_FILE}"
        COMMAND "${CMAKE_COMMAND}" "-DINPUT=${HELIX_RENDER_PIPELINES_CORE_SHADERS}" "-DOUTPUT=${tests_generated}/CoreShadersAssembly.h"
                -DNAME=kCoreShadersAssembly -P "${HELIX_EMBED_FILE}"
        COMMAND "${CMAKE_COMMAND}" "-DINPUT=${HELIX_RENDER_PIPELINES_CORE_SHADER_LIBRARY}" "-DOUTPUT=${tests_generated}/CoreShaderLibraryAssembly.h"
                -DNAME=kCoreShaderLibraryAssembly -P "${HELIX_EMBED_FILE}"
        COMMAND "${CMAKE_COMMAND}" "-DINPUT=${standard_shaders_dll}" "-DOUTPUT=${tests_generated}/StandardShadersAssembly.h" -DNAME=kStandardShadersAssembly
                -P "${HELIX_EMBED_FILE}"
        COMMAND "${CMAKE_COMMAND}" "-DINPUT=${standard_test_shaders_dll}" "-DOUTPUT=${tests_generated}/StandardTestShadersAssembly.h"
                -DNAME=kStandardTestShadersAssembly -P "${HELIX_EMBED_FILE}"
        DEPENDS "${test_shaders_dir}/HelixEngine.TestShaders.dll" "${HELIX_RENDER_PIPELINES_CORE_SHADERS}" "${HELIX_RENDER_PIPELINES_CORE_SHADER_LIBRARY}"
                "${standard_shaders_dll}" "${standard_test_shaders_dll}" "${HELIX_EMBED_FILE}"
        COMMENT "Embedding the test shaders, the core's and the pipeline's shaders")
    target_sources(helix_render_pipelines_standard_tests PRIVATE ${embedded})
    target_include_directories(helix_render_pipelines_standard_tests PRIVATE "${tests_dir}" "${tests_generated}" "${engine_dir}/Tests")
    target_compile_definitions(helix_render_pipelines_standard_tests PRIVATE "HX_TEST_RUNTIME_ASSEMBLY=\"${HELIX_SCRIPTING_RUNTIME}\""
                                                                            "HX_TEST_CORE_MODULE=\"${HELIX_CORE_MODULE}\"" "HX_TEST_SCRIPTS=\"${tests_dll}\""
                                                                            "HX_TEST_CORE_RUNTIME=\"${tests_bin}/Helix.RenderPipelines.Core.Runtime.dll\""
                                                                            "HX_TEST_STANDARD_RUNTIME=\"${tests_bin}/Helix.RenderPipelines.Standard.Runtime.dll\""
                                                                            "HELIX_ENGINE_GOLDEN_DIR=\"${tests_dir}/Golden\"")
    # The web app's assemblies.
    set_property(TARGET helix_render_pipelines_standard_tests APPEND PROPERTY HELIX_DOTNET_ASSEMBLIES "${HELIX_CORE_MODULE}" ${HELIX_CORE_MODULE_REFERENCES}
                                                                                                      "${HELIX_RENDER_PIPELINES_CORE}" "${runtime_dll}" "${tests_dll}")
    if(TARGET helix_render_pipelines_standard_tests_web)
        add_dependencies(helix_render_pipelines_standard_tests_web helix_render_pipelines_standard_test_scripts helix_core_module)
    endif()
    # Once more on the coreclr starter (as Android hosts the runtime), and with the render thread and persistent blocks.
    if(HELIX_PLATFORM STREQUAL "windows" OR HELIX_PLATFORM STREQUAL "linux")
        add_test(NAME helix_render_pipelines_standard_tests_coreclr COMMAND helix_render_pipelines_standard_tests)
        set_tests_properties(helix_render_pipelines_standard_tests_coreclr PROPERTIES ENVIRONMENT "HELIX_DOTNET_RUNTIME=coreclr")
        add_test(NAME helix_render_pipelines_standard_tests_render_thread COMMAND helix_render_pipelines_standard_tests)
        set_tests_properties(helix_render_pipelines_standard_tests_render_thread PROPERTIES ENVIRONMENT "HELIX_RENDER_THREAD=1;HELIX_RENDER_BATCHING=1")
    endif()
endif()

# The public surface of the runtime against its inventory (Tests/Api/Helix.RenderPipelines.Standard.csv), with Helix.ScriptCore's tool.
if(HELIX_BUILD_TESTS AND PROJECT_IS_TOP_LEVEL AND HELIX_API_INVENTORY)
    add_test(NAME helix_render_pipelines_standard_api COMMAND "${HELIX_DOTNET}" "${HELIX_API_INVENTORY}" --assembly "${runtime_dll}" --reference "${HELIX_CORE_MODULE}"
             --reference "${HELIX_SCRIPTING_RUNTIME}" --reference "${HELIX_RENDER_PIPELINES_CORE}"
             --inventory "${CMAKE_CURRENT_SOURCE_DIR}/Tests/Api/Helix.RenderPipelines.Standard.csv")
    add_dependencies(helix_render_pipelines_standard_runtime helix_api_inventory)
endif()

if(TARGET helix_render_pipelines_standard_tests AND HELIX_ENABLE_ASAN AND HELIX_PLATFORM STREQUAL "linux")
    helix_dependency_dir(scripting_dir Scripting)
    set(suppressions "LSAN_OPTIONS=suppressions=${scripting_dir}/Tests/Support/LeakSuppressions.txt")
    set_tests_properties(helix_render_pipelines_standard_tests PROPERTIES ENVIRONMENT "${suppressions}")
    set_tests_properties(helix_render_pipelines_standard_tests_coreclr PROPERTIES ENVIRONMENT "HELIX_DOTNET_RUNTIME=coreclr;${suppressions}")
    set_tests_properties(helix_render_pipelines_standard_tests_render_thread PROPERTIES ENVIRONMENT "HELIX_RENDER_THREAD=1;HELIX_RENDER_BATCHING=1;${suppressions}")
endif()
