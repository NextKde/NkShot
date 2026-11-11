include_directories(${PROJECT_SOURCE_DIR}/src)

SET(FILENAME_TEST_SRCS
    FilenameTest.cpp
    ../src/ShortcutActions.cpp
    ../src/ExportManager.cpp
    ../src/Platforms/ImagePlatform.cpp
    ../src/Platforms/VideoPlatform.cpp
)

ecm_qt_declare_logging_category(FILENAME_TEST_SRCS
    HEADER spectacle_debug.h
    IDENTIFIER SPECTACLE_LOG
    CATEGORY_NAME spectacle
    DESCRIPTION "spectacle (general)"
    EXPORT SPECTACLE
)

kconfig_add_kcfg_files(FILENAME_TEST_SRCS GENERATE_MOC ${PROJECT_SOURCE_DIR}/src/Gui/SettingsDialog/settings.kcfgc)

ecm_add_test(
    ${FILENAME_TEST_SRCS}
    TEST_NAME "filename_test"
    LINK_LIBRARIES  Qt::Test
        Qt::PrintSupport Qt::Qml KF6::I18n KF6::ConfigCore KF6::GlobalAccel KF6::KIOCore KF6::WindowSystem KF6::XmlGui KF6::GuiAddons KF6::PrisonScanner
)

# Reusable editor model and actual software viewport regression tests.
ecm_add_test(
    EditorDocumentTest.cpp
    ../src/Gui/PortableAnnotationViewport.cpp
    ../src/Gui/PortableAnnotationViewport.h
    TEST_NAME "editor_document_test"
    LINK_LIBRARIES Qt::Test Qt::Quick KQuickImageEditor
)
set_tests_properties(editor_document_test PROPERTIES
    TIMEOUT 20
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
)

# Detect upstream fractional-DPR shadow repaint clipping; see third_party/patches.
ecm_add_test(ShadowRepaintTest.cpp TEST_NAME "shadow_repaint_test"
    LINK_LIBRARIES Qt::Test Qt::Gui KQuickImageEditor)
set_tests_properties(shadow_repaint_test PROPERTIES TIMEOUT 20
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")

# Real asynchronous OCR; requires installed Tesseract English data and a sans font.
ecm_add_test(
    OcrManagerTest.cpp
    ../src/OcrManager.cpp
    ../src/OcrManager.h
    ${CMAKE_CURRENT_BINARY_DIR}/settings.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/spectacle_debug.cpp
    TEST_NAME "ocr_manager_test"
    LINK_LIBRARIES Qt::Test Qt::Widgets Qt::Qml KF6::I18n KF6::ConfigGui PkgConfig::TESSERACT
)
target_compile_options(ocr_manager_test PRIVATE -fexceptions)
target_include_directories(ocr_manager_test PRIVATE ${PROJECT_BINARY_DIR}/src)
set_tests_properties(ocr_manager_test PROPERTIES TIMEOUT 30
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Explicit opt-in component capture; no real desktop/Wayland claim.
add_executable(kos_material_probe EXCLUDE_FROM_ALL ui/KosMaterialProbe.cpp)
target_link_libraries(kos_material_probe PRIVATE Qt::Quick Qt::GuiPrivate Kos::Ui Kos::UiPlugin)

# Local exact-overlap core, not an automatic desktop scrolling claim.
ecm_add_test(VerticalStitcherTest.cpp ../src/Imaging/VerticalStitcher.cpp
    TEST_NAME vertical_stitcher_test LINK_LIBRARIES Qt::Test Qt::Gui)
set_tests_properties(vertical_stitcher_test PROPERTIES TIMEOUT 20)

# Reusable immutable image export transforms.
ecm_add_test(ImageTransformsTest.cpp ../src/Imaging/ImageTransforms.cpp
    TEST_NAME image_transforms_test LINK_LIBRARIES Qt::Test Qt::Gui)
set_tests_properties(image_transforms_test PROPERTIES TIMEOUT 20 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Isolated frame-session core; no live desktop capture acceptance claim.
ecm_add_test(ScrollCaptureSessionTest.cpp ../src/Imaging/ScrollCaptureSession.cpp ../src/Imaging/ScrollFrameCrop.cpp ../src/Imaging/VerticalStitcher.cpp
    TEST_NAME scroll_capture_session_test LINK_LIBRARIES Qt::Test Qt::Gui)
set_tests_properties(scroll_capture_session_test PROPERTIES TIMEOUT 20)

ecm_add_test(ScrollCaptureControllerTest.cpp ../src/Imaging/ScrollCaptureController.cpp ../src/Imaging/ScrollCaptureSession.cpp ../src/Imaging/VerticalStitcher.cpp
    TEST_NAME scroll_capture_controller_test LINK_LIBRARIES Qt::Test Qt::Gui)
set_tests_properties(scroll_capture_controller_test PROPERTIES TIMEOUT 20)

# Independent OCR job: no singleton/clipboard/configuration side effects.
ecm_add_test(LocalTextExtractionTest.cpp ../src/Imaging/LocalTextExtraction.cpp
    TEST_NAME local_text_extraction_test LINK_LIBRARIES Qt::Test Qt::Gui PkgConfig::TESSERACT)
target_compile_options(local_text_extraction_test PRIVATE -fexceptions)
set_tests_properties(local_text_extraction_test PROPERTIES TIMEOUT 40 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Real filesystem preflight only; does not establish native recording acceptance.
ecm_add_test(RecordingOutputPathTest.cpp TEST_NAME recording_output_path_test LINK_LIBRARIES Qt::Test Qt::Core)
set_tests_properties(recording_output_path_test PROPERTIES TIMEOUT 10)

# Queued Qt callback lifetime tests, not compositor capture tests.
ecm_add_test(CaptureStreamLeaseTest.cpp TEST_NAME capture_stream_lease_test LINK_LIBRARIES Qt::Test Qt::Core)
set_tests_properties(capture_stream_lease_test PROPERTIES TIMEOUT 10)
