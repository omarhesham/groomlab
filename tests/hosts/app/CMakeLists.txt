# SPDX-License-Identifier: Apache-2.0
# The app's tests (ctest label quick, with the core's): the non-UI logic (the document over a
# session, the OBJ reader, the camera), a headless render of the sample groom through QRhi
# into a texture, read back and checked for pixels, the panels (the param editors, the look
# card, the maps panel) driven without a window, and the live benchmark. No window:
# QT_QPA_PLATFORM=offscreen.
add_executable(groomlab_app_tests test_app.cpp test_panels.cpp bench_app.cpp)
target_link_libraries(groomlab_app_tests PRIVATE groomlab_app_ui groomlab::warnings GTest::gtest nlohmann_json::nlohmann_json)
groomlab_path_literal(_test_data "${PROJECT_SOURCE_DIR}/tests/data")
target_compile_definitions(groomlab_app_tests PRIVATE "GROOMLAB_TEST_DATA=${_test_data}")
groomlab_precompile(groomlab_app_tests <gtest/gtest.h> <nlohmann/json.hpp> <QImage> <QSize> <QString>)
groomlab_app_deploy(groomlab_app_tests)
gtest_discover_tests(groomlab_app_tests DISCOVERY_MODE PRE_TEST TEST_LIST groomlab_app_test_list
                     PROPERTIES LABELS quick ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
groomlab_label_discovered(groomlab_app_test_list quick app)
# The scripted exports and card bake through the executable's flags (no window).
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME app.cli_exports
             COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/cli_exports.py" "$<TARGET_FILE:groomlab_app>"
                     "${PROJECT_SOURCE_DIR}/tests/data/strokes/strokes.groom" "${CMAKE_CURRENT_BINARY_DIR}/cli_exports")
    set_tests_properties(app.cli_exports PROPERTIES LABELS "quick;app" TIMEOUT 600
                         ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    # The app's package (tools/package_app.py --stage-only): the App component installed into a
    # scratch folder and checked against the manifest (every file it needs, nothing it must not
    # ship, every DLL each binary imports beside it or in Windows, Qt's files Qt's own).
    if(WIN32)
        add_test(NAME app.package_manifest
                 COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/package_app.py" --build "${CMAKE_BINARY_DIR}"
                         --config $<CONFIG> --cmake "${CMAKE_COMMAND}" --out "${CMAKE_CURRENT_BINARY_DIR}/package_manifest" --stage-only)
        set_tests_properties(app.package_manifest PROPERTIES LABELS "quick;app" TIMEOUT 300)
    endif()
endif()
