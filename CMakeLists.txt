cmake_minimum_required(VERSION 3.24)
project(WarDogsDistanceCalculator VERSION 1.3.0 LANGUAGES C CXX)
set(CMAKE_CL_SHOWINCLUDES_PREFIX "注意: 包含文件:")

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(wardogs_zstd STATIC
    third_party/zstd/common/debug.c
    third_party/zstd/common/entropy_common.c
    third_party/zstd/common/error_private.c
    third_party/zstd/common/fse_decompress.c
    third_party/zstd/common/xxhash.c
    third_party/zstd/common/zstd_common.c
    third_party/zstd/decompress/huf_decompress.c
    third_party/zstd/decompress/zstd_ddict.c
    third_party/zstd/decompress/zstd_decompress_block.c
    third_party/zstd/decompress/zstd_decompress.c
)
target_include_directories(wardogs_zstd PUBLIC third_party/zstd)
target_include_directories(wardogs_zstd PRIVATE third_party/zstd/common)
target_compile_definitions(wardogs_zstd PRIVATE ZSTD_DISABLE_ASM)

add_library(wardogs_terrain src/terrain_package.cpp)
target_include_directories(wardogs_terrain PUBLIC include)
target_compile_definitions(wardogs_terrain PUBLIC UNICODE _UNICODE NOMINMAX)
target_link_libraries(wardogs_terrain PRIVATE Qt6::Core wardogs_zstd bcrypt)
if(MSVC)
    target_compile_options(wardogs_terrain PRIVATE /W4 /permissive- /utf-8)
endif()

add_library(wardogs_core
    src/calculator.cpp
    src/coordinates.cpp
    src/hotkeys.cpp
    src/image.cpp
    src/logger.cpp
    src/ocr.cpp
    src/presentation.cpp
    src/vehicle_ballistics.cpp
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
        src/main_window.cpp
        src/pinned_result_window.cpp
        src/selection_overlay.cpp
        src/settings.cpp
        src/settings_dialog.cpp
        src/vehicle_solution_widget.cpp
        src/window_title_bar.cpp
        src/windows_ocr.cpp
        "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
    )
    target_include_directories(WarDogsDistanceCalculator PRIVATE include)
    target_compile_definitions(WarDogsDistanceCalculator PRIVATE
        UNICODE _UNICODE NOMINMAX WARDOGS_VERSION="${PROJECT_VERSION}"
    )
    target_compile_options(WarDogsDistanceCalculator PRIVATE /W4 /permissive- /utf-8)
    target_link_libraries(WarDogsDistanceCalculator PRIVATE
        wardogs_core wardogs_terrain Qt6::Widgets dwmapi gdi32 user32 windowsapp
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
    "${CMAKE_CURRENT_SOURCE_DIR}/TERRAIN_DATA_NOTICE.md"
    DESTINATION .
)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/TERRAIN_DATA_NOTICE.md"
    DESTINATION terrain-packs
)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/zstd/LICENSE.txt"
    DESTINATION licenses/zstd
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

add_executable(wardogs_vehicle_ballistics_tests tests/vehicle_ballistics_tests.cpp)
target_link_libraries(wardogs_vehicle_ballistics_tests PRIVATE wardogs_core)
if(MSVC)
    target_compile_options(wardogs_vehicle_ballistics_tests PRIVATE /W4 /permissive- /utf-8)
endif()
add_test(NAME wardogs_vehicle_ballistics_tests COMMAND wardogs_vehicle_ballistics_tests)

add_executable(wardogs_terrain_tests tests/terrain_package_tests.cpp)
target_link_libraries(wardogs_terrain_tests PRIVATE wardogs_terrain Qt6::Core)
target_compile_definitions(wardogs_terrain_tests PRIVATE
    WARDOGS_TERRAIN_TEST_PACKAGE=L"${CMAKE_CURRENT_SOURCE_DIR}/tests/data/terrain_test.wdt"
)
if(MSVC)
    target_compile_options(wardogs_terrain_tests PRIVATE /W4 /permissive- /utf-8)
endif()
add_test(NAME wardogs_terrain_tests COMMAND wardogs_terrain_tests)
add_custom_command(TARGET wardogs_terrain_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:Qt6::Core>"
        "$<TARGET_FILE_DIR:wardogs_terrain_tests>"
)

add_executable(wardogs_vehicle_ui_tests
    tests/vehicle_ui_tests.cpp
    src/pinned_result_window.cpp
    src/vehicle_solution_widget.cpp
    src/window_title_bar.cpp
)
target_include_directories(wardogs_vehicle_ui_tests PRIVATE include src)
target_link_libraries(wardogs_vehicle_ui_tests PRIVATE
    wardogs_core Qt6::Widgets dwmapi
)
if(MSVC)
    target_compile_options(wardogs_vehicle_ui_tests PRIVATE /W4 /permissive- /utf-8)
endif()
add_test(NAME wardogs_vehicle_ui_tests COMMAND wardogs_vehicle_ui_tests)
add_custom_command(TARGET wardogs_vehicle_ui_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:Qt6::Core>"
        "$<TARGET_FILE:Qt6::Gui>"
        "$<TARGET_FILE:Qt6::Widgets>"
        "$<TARGET_FILE_DIR:wardogs_vehicle_ui_tests>"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:wardogs_vehicle_ui_tests>/platforms"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
        "$<TARGET_FILE_DIR:wardogs_vehicle_ui_tests>/platforms"
)

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
