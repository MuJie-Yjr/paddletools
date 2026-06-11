cmake_minimum_required(VERSION 3.24)

project(ppocr_workbench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "Suppress CMake deprecation warnings from vendored third-party projects" FORCE)

if(MSVC)
    add_compile_options(/utf-8)
endif()

set(PPOCR_PADDLEX_PYTHON "" CACHE FILEPATH "PaddleX Python executable used by local training tools")
if(PPOCR_PADDLEX_PYTHON)
    set(ENV{PPOCR_PADDLEX_PYTHON} "${PPOCR_PADDLEX_PYTHON}")
endif()

if(NOT QT_ROOT AND DEFINED ENV{QT_ROOT})
    set(QT_ROOT "$ENV{QT_ROOT}" CACHE PATH "Qt root" FORCE)
else()
    set(QT_ROOT "${QT_ROOT}" CACHE PATH "Qt root")
endif()
if(QT_ROOT AND NOT Qt6_DIR)
    set(Qt6_DIR "${QT_ROOT}/lib/cmake/Qt6" CACHE PATH "Qt6 CMake package" FORCE)
endif()
if(NOT OpenCV_DIR AND DEFINED ENV{OpenCV_DIR})
    set(OpenCV_DIR "$ENV{OpenCV_DIR}" CACHE PATH "OpenCV CMake package" FORCE)
endif()
if(NOT PADDLE_LIB AND DEFINED ENV{PPOCR_PADDLE_INFERENCE_ROOT})
    set(PADDLE_LIB "$ENV{PPOCR_PADDLE_INFERENCE_ROOT}" CACHE PATH "Paddle Inference C++ SDK root" FORCE)
elseif(NOT PADDLE_LIB)
    set(PADDLE_LIB "${CMAKE_SOURCE_DIR}/third_party/paddle_inference" CACHE PATH "Paddle Inference C++ SDK root" FORCE)
else()
    set(PADDLE_LIB "${PADDLE_LIB}" CACHE PATH "Paddle Inference C++ SDK root")
endif()
file(TO_CMAKE_PATH "${PADDLE_LIB}" PADDLE_LIB_NORMALIZED)
set(PPOCR_PADDLE_WITH_GPU OFF)
if(EXISTS "${PADDLE_LIB}/version.txt")
    file(READ "${PADDLE_LIB}/version.txt" PPOCR_PADDLE_VERSION_TEXT)
    if(PPOCR_PADDLE_VERSION_TEXT MATCHES "WITH_GPU:[ \t]*ON")
        set(PPOCR_PADDLE_WITH_GPU ON)
    endif()
endif()
message(STATUS "Paddle Inference SDK: ${PADDLE_LIB_NORMALIZED}")
message(STATUS "Paddle Inference GPU support: ${PPOCR_PADDLE_WITH_GPU}")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Quick QuickWidgets Qml Pdf)
if(POLICY CMP0146)
    cmake_policy(PUSH)
    cmake_policy(SET CMP0146 OLD)
endif()
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)
if(POLICY CMP0146)
    cmake_policy(POP)
endif()

if(NOT OPENCV_RUNTIME_DIR AND DEFINED ENV{OPENCV_RUNTIME_DIR})
    set(OPENCV_RUNTIME_DIR "$ENV{OPENCV_RUNTIME_DIR}" CACHE PATH "OpenCV runtime DLL directory" FORCE)
elseif(NOT OPENCV_RUNTIME_DIR AND OpenCV_DIR)
    get_filename_component(PPOCR_OPENCV_ROOT "${OpenCV_DIR}/.." ABSOLUTE)
    set(OPENCV_RUNTIME_DIR "${PPOCR_OPENCV_ROOT}/bin" CACHE PATH "OpenCV runtime DLL directory" FORCE)
else()
    set(OPENCV_RUNTIME_DIR "${OPENCV_RUNTIME_DIR}" CACHE PATH "OpenCV runtime DLL directory")
endif()
set(PPOCR_CPP_INFER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/PaddleOCR/deploy/cpp_infer" CACHE PATH "PaddleOCR C++ inference source directory")
set(PPOCR_CPP_INFER_THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party/cpp_infer" CACHE PATH "PaddleOCR C++ inference third-party directory")

function(download_and_decompress url filename decompress_dir)
    file(GLOB existing_entries "${decompress_dir}/*")
    if(NOT existing_entries)
        if(NOT EXISTS "${filename}")
            message(STATUS "Downloading ${url}")
            file(DOWNLOAD "${url}" "${filename}.tmp" SHOW_PROGRESS STATUS download_status)
            list(GET download_status 0 download_code)
            if(NOT download_code EQUAL 0)
                list(GET download_status 1 download_message)
                message(FATAL_ERROR "Download failed for ${url}: ${download_message}")
            endif()
            file(RENAME "${filename}.tmp" "${filename}")
        endif()
        file(MAKE_DIRECTORY "${decompress_dir}")
        message(STATUS "Decompressing ${filename}")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar -xf "${filename}"
            WORKING_DIRECTORY "${decompress_dir}"
            RESULT_VARIABLE decompress_result
        )
        if(NOT decompress_result EQUAL 0)
            message(FATAL_ERROR "Failed to decompress ${filename}")
        endif()
    endif()
endfunction()

foreach(pkg abseil-cpp clipper_ver6.4.2 nlohmann)
    download_and_decompress(
        "https://paddle-model-ecology.bj.bcebos.com/paddlex/cpp/libs/${pkg}.tgz"
        "${PPOCR_CPP_INFER_THIRD_PARTY_DIR}/${pkg}.tgz"
        "${PPOCR_CPP_INFER_THIRD_PARTY_DIR}/${pkg}"
    )
endforeach()

set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
set(PPOCR_PREV_CMAKE_WARN_DEPRECATED "${CMAKE_WARN_DEPRECATED}")
set(CMAKE_WARN_DEPRECATED OFF)
add_subdirectory("${PPOCR_CPP_INFER_THIRD_PARTY_DIR}/abseil-cpp" "${CMAKE_BINARY_DIR}/third_party/abseil-cpp" EXCLUDE_FROM_ALL)

set(PPOCR_PREV_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
add_subdirectory("${PPOCR_CPP_INFER_THIRD_PARTY_DIR}/clipper_ver6.4.2/cpp" "${CMAKE_BINARY_DIR}/third_party/clipper" EXCLUDE_FROM_ALL)
set(BUILD_SHARED_LIBS ${PPOCR_PREV_BUILD_SHARED_LIBS} CACHE BOOL "" FORCE)

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
set(YAML_CPP_FORMAT_SOURCE OFF CACHE BOOL "" FORCE)
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(YAML_MSVC_SHARED_RT ON CACHE BOOL "" FORCE)
add_subdirectory("${PPOCR_CPP_INFER_THIRD_PARTY_DIR}/yaml-cpp" "${CMAKE_BINARY_DIR}/third_party/yaml-cpp" EXCLUDE_FROM_ALL)
set(CMAKE_WARN_DEPRECATED "${PPOCR_PREV_CMAKE_WARN_DEPRECATED}")

add_library(ppocr_domain STATIC
    src/domain/ProjectTypes.cpp
)
target_include_directories(ppocr_domain PUBLIC src)
target_link_libraries(ppocr_domain PUBLIC Qt6::Core)

add_library(ppocr_core STATIC
    src/core/AnnotationOps.cpp
    src/core/AssetImporter.cpp
    src/core/ClassificationPrelabeler.cpp
    src/core/CropGenerator.cpp
    src/core/DatasetExporter.cpp
    src/core/DatasetExportPlanner.cpp
    src/core/Exporter.cpp
    src/core/LayoutPrelabeler.cpp
    src/core/OcrPrelabeler.cpp
    src/core/ProjectTypes.cpp
    src/core/ProjectRepository.cpp
    src/core/RuntimePaths.cpp
    src/core/TrainingCommandBuilder.cpp
    src/core/TrainingPreflight.cpp
    src/core/TrainingRunner.cpp
    src/core/TrainingRunStore.cpp
    src/core/TrainingTasks.cpp
    src/core/Validator.cpp
    src/paddle/PaddleProcess.cpp
)

target_include_directories(ppocr_core PUBLIC src)
target_compile_definitions(ppocr_core PUBLIC PPOCR_PROJECT_ROOT="${CMAKE_SOURCE_DIR}")
target_link_libraries(ppocr_core PUBLIC
    ppocr_domain
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Pdf
    ${OpenCV_LIBS}
)

add_library(ppocr_paddle STATIC
    src/paddle/PaddleInferenceRuntime.cpp
)
target_include_directories(ppocr_paddle PUBLIC
    src
    "${PADDLE_LIB}/paddle/include"
)
target_compile_definitions(ppocr_paddle PUBLIC PADDLE_LIB_ROOT="${PADDLE_LIB_NORMALIZED}")
target_link_libraries(ppocr_paddle PUBLIC
    Qt6::Core
    "${PADDLE_LIB}/paddle/lib/paddle_inference.lib"
    "${PADDLE_LIB}/paddle/lib/common.lib"
)

add_library(ppocr_infrastructure STATIC
    src/infrastructure/EnvironmentDetector.cpp
    src/infrastructure/FileSystem.cpp
    src/infrastructure/JsonStore.cpp
    src/infrastructure/Logger.cpp
)
target_include_directories(ppocr_infrastructure PUBLIC src)
target_link_libraries(ppocr_infrastructure PUBLIC Qt6::Core)

file(GLOB_RECURSE PPOCR_CPP_INFER_SOURCES CONFIGURE_DEPENDS
    "${PPOCR_CPP_INFER_SOURCE_DIR}/src/*.cc"
)
list(FILTER PPOCR_CPP_INFER_SOURCES EXCLUDE REGEX "/src/utils/args\\.cc$")

add_library(ppocr_cpp_infer STATIC
    ${PPOCR_CPP_INFER_SOURCES}
    src/paddle/PaddleClsEngine.cpp
    src/paddle/PaddleDocLayoutEngine.cpp
    src/paddle/PaddleOcrEngine.cpp
)
target_include_directories(ppocr_cpp_infer PUBLIC
    src/compat
    src
    "${PPOCR_CPP_INFER_SOURCE_DIR}"
    "${PPOCR_CPP_INFER_SOURCE_DIR}/src"
    "${PPOCR_CPP_INFER_THIRD_PARTY_DIR}"
    "${PPOCR_CPP_INFER_THIRD_PARTY_DIR}/nlohmann"
    "${POLYCLIPPING_INCLUDE_DIR}"
    "${PADDLE_LIB}/paddle/include"
    "${PADDLE_LIB}/third_party/install/gflags/include"
    "${PADDLE_LIB}/third_party/install/glog/include"
    "${PADDLE_LIB}/third_party/install/protobuf/include"
    "${PADDLE_LIB}/third_party/install/xxhash/include"
    "${PADDLE_LIB}/third_party/install/mklml/include"
    "${PADDLE_LIB}/third_party/install/onednn/include"
    "${PADDLE_LIB}/third_party/boost"
    "${PADDLE_LIB}/third_party/eigen3"
)
target_compile_definitions(ppocr_cpp_infer PUBLIC
    PADDLE_LIB_ROOT="${PADDLE_LIB_NORMALIZED}"
    PPOCR_PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
    GOOGLE_GLOG_DLL_DECL=
    YAML_CPP_STATIC_DEFINE
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)
if(PPOCR_PADDLE_WITH_GPU)
    target_compile_definitions(ppocr_cpp_infer PUBLIC WITH_GPU)
endif()
target_link_libraries(ppocr_cpp_infer PUBLIC
    ppocr_paddle
    Qt6::Core
    ${OpenCV_LIBS}
    absl::statusor
    absl::strings
    polyclipping
    yaml-cpp::yaml-cpp
    Shlwapi.lib
)

add_library(ppocr_application STATIC
    src/application/AnnotationService.cpp
    src/application/DatasetService.cpp
    src/application/EnvironmentService.cpp
    src/application/ExportService.cpp
    src/application/InferenceService.cpp
    src/application/ModelRegistryService.cpp
    src/application/PredictionService.cpp
    src/application/ProjectService.cpp
    src/application/TrainingService.cpp
)
target_include_directories(ppocr_application PUBLIC src)
target_link_libraries(ppocr_application PUBLIC
    ppocr_core
    ppocr_infrastructure
    ppocr_cpp_infer
    Qt6::Core
)

add_library(ppocr_environment STATIC
    src/core/EnvironmentReport.cpp
)
target_include_directories(ppocr_environment PUBLIC src)
target_link_libraries(ppocr_environment PUBLIC
    ppocr_core
    ppocr_cpp_infer
    Qt6::Core
    ${OpenCV_LIBS}
)
target_link_libraries(ppocr_application PUBLIC ppocr_environment)

function(copy_paddle_runtime target_name)
    if(EXISTS "${PADDLE_LIB}/paddle/lib")
        file(GLOB PADDLE_RUNTIME_DLLS "${PADDLE_LIB}/paddle/lib/*.dll")
        foreach(runtime_dll ${PADDLE_RUNTIME_DLLS})
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${runtime_dll}" "$<TARGET_FILE_DIR:${target_name}>")
        endforeach()
    endif()
    foreach(dep_dir
        "${PADDLE_LIB}/third_party/install/mklml/lib"
        "${PADDLE_LIB}/third_party/install/onednn/lib"
        "${PADDLE_LIB}/third_party/install/glog/lib"
        "${PADDLE_LIB}/third_party/install/gflags/lib"
        "${PADDLE_LIB}/third_party/install/protobuf/lib"
        "${PADDLE_LIB}/third_party/install/xxhash/lib"
        "${PADDLE_LIB}/third_party/install/utf8proc/lib"
    )
        if(EXISTS "${dep_dir}")
            file(GLOB dep_dlls "${dep_dir}/*.dll")
            foreach(dep_dll ${dep_dlls})
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${dep_dll}" "$<TARGET_FILE_DIR:${target_name}>")
            endforeach()
        endif()
    endforeach()
endfunction()

function(copy_common_runtime target_name)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${target_name}> "$<TARGET_FILE_DIR:${target_name}>"
        COMMAND_EXPAND_LISTS
    )

    foreach(opencv_component core imgproc imgcodecs)
        file(GLOB opencv_dlls "${OPENCV_RUNTIME_DIR}/opencv_${opencv_component}*.dll")
        foreach(opencv_dll ${opencv_dlls})
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${opencv_dll}" "$<TARGET_FILE_DIR:${target_name}>")
        endforeach()
    endforeach()

    foreach(qt_dll Qt6Core Qt6Gui Qt6Widgets Qt6Pdf Qt6Quick Qt6QuickWidgets Qt6Qml Qt6Network Qt6OpenGL)
        if(EXISTS "${QT_ROOT}/bin/${qt_dll}.dll")
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${QT_ROOT}/bin/${qt_dll}.dll" "$<TARGET_FILE_DIR:${target_name}>")
        endif()
    endforeach()

    if(EXISTS "${QT_ROOT}/plugins/platforms/qwindows.dll")
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QT_ROOT}/plugins/platforms/qwindows.dll"
                "$<TARGET_FILE_DIR:${target_name}>/platforms/qwindows.dll")
    endif()

    if(EXISTS "${QT_ROOT}/plugins/imageformats")
        file(GLOB qt_image_plugins "${QT_ROOT}/plugins/imageformats/*.dll")
        foreach(qt_image_plugin ${qt_image_plugins})
            get_filename_component(plugin_name "${qt_image_plugin}" NAME)
            if(NOT plugin_name MATCHES "d\\.dll$")
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/imageformats"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${qt_image_plugin}"
                        "$<TARGET_FILE_DIR:${target_name}>/imageformats/${plugin_name}")
            endif()
        endforeach()
    endif()
endfunction()

add_executable(ppocr_workbench
    resources/app.qrc
    src/app/controllers/AnnotationController.cpp
    src/app/controllers/DatasetController.cpp
    src/app/controllers/ModelLibraryController.cpp
    src/app/controllers/ProjectController.cpp
    src/app/controllers/PredictionController.cpp
    src/app/controllers/TrainingController.cpp
    src/app/MainWindow.cpp
    src/app/widgets/MainWindowAnnotationUi.cpp
    src/app/widgets/MainWindowPages.cpp
    src/app/widgets/MainWindowPredictionUi.cpp
    src/app/widgets/MainWindowProjectDataset.cpp
    src/app/widgets/MainWindowTrainingUi.cpp
    src/app/main.cpp
)
target_link_libraries(ppocr_workbench PRIVATE
    ppocr_application
    ppocr_core
    ppocr_environment
    ppocr_paddle
    ppocr_cpp_infer
    Qt6::QuickWidgets
    Qt6::Qml
)
copy_common_runtime(ppocr_workbench)
copy_paddle_runtime(ppocr_workbench)

add_executable(ppocr_infer_smoke
    tools/ppocr_infer_smoke.cpp
)
target_link_libraries(ppocr_infer_smoke PRIVATE ppocr_paddle)
copy_common_runtime(ppocr_infer_smoke)
copy_paddle_runtime(ppocr_infer_smoke)

add_executable(ppocr_env_check
    tools/ppocr_env_check.cpp
)
target_link_libraries(ppocr_env_check PRIVATE ppocr_environment)
copy_common_runtime(ppocr_env_check)
copy_paddle_runtime(ppocr_env_check)

add_executable(ppocr_ocr_predict
    tools/ppocr_ocr_predict.cpp
)
target_link_libraries(ppocr_ocr_predict PRIVATE ppocr_cpp_infer)
copy_common_runtime(ppocr_ocr_predict)
copy_paddle_runtime(ppocr_ocr_predict)

add_executable(ppocr_cls_predict
    tools/ppocr_cls_predict.cpp
)
target_link_libraries(ppocr_cls_predict PRIVATE ppocr_cpp_infer)
copy_common_runtime(ppocr_cls_predict)
copy_paddle_runtime(ppocr_cls_predict)

add_executable(ppocr_layout_predict
    tools/ppocr_layout_predict.cpp
)
target_link_libraries(ppocr_layout_predict PRIVATE ppocr_cpp_infer)
copy_common_runtime(ppocr_layout_predict)
copy_paddle_runtime(ppocr_layout_predict)

add_executable(ppocr_training_preflight
    tools/ppocr_training_preflight.cpp
)
target_link_libraries(ppocr_training_preflight PRIVATE ppocr_core)
copy_common_runtime(ppocr_training_preflight)

add_executable(ppocr_training_run
    tools/ppocr_training_run.cpp
    tests/fakes/FakeTrainingRunner.cpp
)
target_link_libraries(ppocr_training_run PRIVATE ppocr_core)
target_include_directories(ppocr_training_run PRIVATE ${CMAKE_SOURCE_DIR})
copy_common_runtime(ppocr_training_run)

add_executable(ppocr_training_controller_smoke
    tests/cpp/test_training_controller.cpp
    src/app/controllers/TrainingController.cpp
)
target_link_libraries(ppocr_training_controller_smoke PRIVATE ppocr_core)
target_link_libraries(ppocr_training_controller_smoke PRIVATE ppocr_application)
copy_common_runtime(ppocr_training_controller_smoke)

add_executable(ppocr_prediction_controller_smoke
    tests/cpp/test_prediction_controller.cpp
    src/app/controllers/PredictionController.cpp
)
target_include_directories(ppocr_prediction_controller_smoke PRIVATE src)
target_link_libraries(ppocr_prediction_controller_smoke PRIVATE ppocr_application Qt6::Core)
copy_common_runtime(ppocr_prediction_controller_smoke)

add_executable(ppocr_annotation_controller_smoke
    tests/cpp/test_annotation_controller.cpp
    src/app/controllers/AnnotationController.cpp
)
target_include_directories(ppocr_annotation_controller_smoke PRIVATE src)
target_link_libraries(ppocr_annotation_controller_smoke PRIVATE ppocr_application ppocr_core)
copy_common_runtime(ppocr_annotation_controller_smoke)

add_executable(ppocr_dataset_controller_smoke
    tests/cpp/test_dataset_controller.cpp
    src/app/controllers/DatasetController.cpp
)
target_include_directories(ppocr_dataset_controller_smoke PRIVATE src)
target_link_libraries(ppocr_dataset_controller_smoke PRIVATE ppocr_application ppocr_core)
copy_common_runtime(ppocr_dataset_controller_smoke)

add_executable(ppocr_application_services_smoke
    tests/cpp/test_application_services.cpp
)
target_link_libraries(ppocr_application_services_smoke PRIVATE ppocr_application)
copy_common_runtime(ppocr_application_services_smoke)

add_executable(ppocr_architecture_boundaries
    tests/cpp/test_architecture_boundaries.cpp
)
target_compile_definitions(ppocr_architecture_boundaries PRIVATE PPOCR_PROJECT_ROOT="${CMAKE_SOURCE_DIR}")

add_executable(ppocr_workflow_smoke
    tools/ppocr_workflow_smoke.cpp
    tests/fakes/FakeTrainingRunner.cpp
)
target_link_libraries(ppocr_workflow_smoke PRIVATE ppocr_core ppocr_cpp_infer)
target_include_directories(ppocr_workflow_smoke PRIVATE ${CMAKE_SOURCE_DIR})
copy_common_runtime(ppocr_workflow_smoke)
copy_paddle_runtime(ppocr_workflow_smoke)

add_executable(ppocr_core_tests
    tests/cpp/test_core_workflow.cpp
    tests/fakes/FakeTrainingRunner.cpp
)
target_link_libraries(ppocr_core_tests PRIVATE ppocr_core ppocr_cpp_infer)
target_include_directories(ppocr_core_tests PRIVATE ${CMAKE_SOURCE_DIR})
copy_common_runtime(ppocr_core_tests)
copy_paddle_runtime(ppocr_core_tests)

enable_testing()
add_test(NAME ppocr_core_tests COMMAND ppocr_core_tests)
add_test(NAME ppocr_training_controller_smoke COMMAND ppocr_training_controller_smoke)
add_test(NAME ppocr_prediction_controller_smoke COMMAND ppocr_prediction_controller_smoke)
add_test(NAME ppocr_annotation_controller_smoke COMMAND ppocr_annotation_controller_smoke)
add_test(NAME ppocr_dataset_controller_smoke COMMAND ppocr_dataset_controller_smoke)
add_test(NAME ppocr_application_services_smoke COMMAND ppocr_application_services_smoke)
add_test(NAME ppocr_architecture_boundaries COMMAND ppocr_architecture_boundaries)
set_tests_properties(ppocr_core_tests PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${QT_ROOT}/bin;PATH=path_list_prepend:${OPENCV_RUNTIME_DIR};PATH=path_list_prepend:$<TARGET_FILE_DIR:ppocr_core_tests>"
)
