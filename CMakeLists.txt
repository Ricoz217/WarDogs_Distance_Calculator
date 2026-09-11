cmake_minimum_required(VERSION 3.24)
project(WarDogsDistanceCalculator VERSION 1.1.0 LANGUAGES CXX)
set(CMAKE_CL_SHOWINCLUDES_PREFIX "注意: 包含文件:")

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(wardogs_core
    src/calculator.cpp
    src/coordinates.cpp
    src/hotkeys.cpp
    src/image.cpp
    src/ocr.cpp
    src/presentation.cpp
)
target_include_directories(wardogs_core PUBLIC include)
target_compile_definitions(wardogs_core PUBLIC UNICODE _UNICODE NOMINMAX)
target_include_directories(wardogs_core PRIVATE third_party/onnxruntime/include)
target_link_libraries(wardogs_core PRIVATE
    windowscodecs
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime/lib/x64/onnxruntime.lib"
)
if(MSVC)
    target_compile_options(wardogs_core PRIVATE /W4 /permissive- /utf-8)
endif()

if(WIN32)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/version.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
        @ONLY
    )
    qt_add_executable(WarDogsDistanceCalculator WIN32
        src/app.cpp
        src/capture.cpp
        src/settings.cpp
        src/windows_ocr.cpp
        "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
    )
    target_include_directories(WarDogsDistanceCalculator PRIVATE include)
    target_compile_definitions(WarDogsDistanceCalculator PRIVATE
        UNICODE _UNICODE NOMINMAX WARDOGS_VERSION="${PROJECT_VERSION}"
    )
    target_compile_options(WarDogsDistanceCalculator PRIVATE /W4 /permissive- /utf-8)
    target_link_libraries(WarDogsDistanceCalculator PRIVATE
        wardogs_core Qt6::Widgets dwmapi gdi32 user32 windowsapp
    )
    add_custom_command(TARGET WarDogsDistanceCalculator POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime/lib/x64/onnxruntime.dll"
            "$<TARGET_FILE_DIR:WarDogsDistanceCalculator>/onnxruntime.dll"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:WarDogsDistanceCalculator>/models"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/PP-OCRv6_rec_small.onnx"
            "$<TARGET_FILE_DIR:WarDogsDistanceCalculator>/models/PP-OCRv6_rec_small.onnx"
    )
endif()

install(TARGETS WarDogsDistanceCalculator RUNTIME DESTINATION .)
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime/lib/x64/onnxruntime.dll"
    "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
    "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    DESTINATION .
)
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/PP-OCRv6_rec_small.onnx"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/LICENSE.PaddleOCR.txt"
    DESTINATION models
)
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime/LICENSE.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime/ThirdPartyNotices.txt"
    DESTINATION licenses/onnxruntime
)

enable_testing()
add_executable(wardogs_tests tests/core_tests.cpp)
target_link_libraries(wardogs_tests PRIVATE wardogs_core)
if(MSVC)
    target_compile_options(wardogs_tests PRIVATE /W4 /permissive- /utf-8)
endif()
add_test(NAME wardogs_core_tests COMMAND wardogs_tests)

add_executable(wardogs_settings_tests
    tests/settings_tests.cpp
    src/settings.cpp
)
target_include_directories(wardogs_settings_tests PRIVATE include)
target_compile_definitions(wardogs_settings_tests PRIVATE UNICODE _UNICODE NOMINMAX)
target_link_libraries(wardogs_settings_tests PRIVATE wardogs_core shell32 ole32)
if(MSVC)
    target_compile_options(wardogs_settings_tests PRIVATE /W4 /permissive- /utf-8)
endif()
add_test(NAME wardogs_settings_tests COMMAND wardogs_settings_tests)

add_executable(wardogs_ocr_tests tests/ocr_tests.cpp)
target_link_libraries(wardogs_ocr_tests PRIVATE wardogs_core)
target_compile_definitions(wardogs_ocr_tests PRIVATE
    WARDOGS_TEST_MODEL=L"${CMAKE_CURRENT_SOURCE_DIR}/assets/models/PP-OCRv6_rec_small.onnx"
    WARDOGS_TEST_IMAGE=L"${CMAKE_CURRENT_SOURCE_DIR}/tests/data/coordinate.png"
    WARDOGS_TEST_EXTRA_TEXT_IMAGE=L"${CMAKE_CURRENT_SOURCE_DIR}/tests/data/coordinate_with_extra_text.png"
    WARDOGS_TEST_REPEATED_IMAGE=L"${CMAKE_CURRENT_SOURCE_DIR}/tests/data/coordinate_repeated_digit.png"
)
if(MSVC)
    target_compile_options(wardogs_ocr_tests PRIVATE /W4 /permissive- /utf-8)
endif()
add_custom_command(TARGET wardogs_ocr_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime/lib/x64/onnxruntime.dll"
        "$<TARGET_FILE_DIR:wardogs_ocr_tests>/onnxruntime.dll"
)
add_test(NAME wardogs_ocr_tests COMMAND wardogs_ocr_tests)

add_executable(ocr_benchmark tools/ocr_benchmark.cpp)
target_link_libraries(ocr_benchmark PRIVATE wardogs_core psapi)
target_compile_options(ocr_benchmark PRIVATE /W4 /permissive- /utf-8)
add_custom_command(TARGET ocr_benchmark POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime/lib/x64/onnxruntime.dll"
        "$<TARGET_FILE_DIR:ocr_benchmark>/onnxruntime.dll"
)

if(WIN32)
    add_executable(windows_ocr_probe
        tools/windows_ocr_probe.cpp
        src/windows_ocr.cpp
    )
    target_include_directories(windows_ocr_probe PRIVATE include)
    target_compile_definitions(windows_ocr_probe PRIVATE UNICODE _UNICODE NOMINMAX)
    target_compile_options(windows_ocr_probe PRIVATE /W4 /permissive- /utf-8)
    target_link_libraries(windows_ocr_probe PRIVATE wardogs_core windowsapp psapi)
endif()
