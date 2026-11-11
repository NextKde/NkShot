include_directories(${PROJECT_SOURCE_DIR}/src)

SET(FILENAME_TEST_SRCS
    FilenameTest.cpp
    ../src/ShortcutActions.cpp
    ../src/ExportManager.cpp
    ../src/Platforms/ImagePlatform.cpp
    ../src/RecordingPreparation.cpp
    ../src/RecordingPreparation.h
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
ecm_add_test(ScrollCaptureSessionTest.cpp ../src/Imaging/ScrollCaptureSession.cpp ../src/Imaging/ScrollFrameAligner.cpp ../src/Imaging/ScrollFrameCrop.cpp ../src/Imaging/VerticalStitcher.cpp
    TEST_NAME scroll_capture_session_test LINK_LIBRARIES Qt::Test Qt::Gui)
set_tests_properties(scroll_capture_session_test PROPERTIES TIMEOUT 20)

ecm_add_test(ScrollCaptureControllerTest.cpp ../src/Imaging/ScrollCaptureController.cpp ../src/Imaging/ScrollCaptureSession.cpp ../src/Imaging/ScrollFrameAligner.cpp ../src/Imaging/VerticalStitcher.cpp
    TEST_NAME scroll_capture_controller_test LINK_LIBRARIES Qt::Test Qt::Gui)
set_tests_properties(scroll_capture_controller_test PROPERTIES TIMEOUT 20)

# Bounded local subprocess protocol only; fake workers are not translation quality evidence.
ecm_add_test(TranslationManagerTest.cpp ../src/Translation/TranslationManager.cpp ../src/Translation/TranslationManager.h
    TEST_NAME translation_manager_test LINK_LIBRARIES Qt::Test Qt::Core KF6::I18n)
set_tests_properties(translation_manager_test PROPERTIES TIMEOUT 45 ENVIRONMENT "LANG=C.UTF-8")

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

# Actual linked KPipeWire failure/termination regression; requires the documented patch.
ecm_add_test(RecordingInitializeFailureTest.cpp TEST_NAME recording_initialize_failure_test LINK_LIBRARIES Qt::Test Qt::Gui K::KPipeWireRecord)
set_tests_properties(recording_initialize_failure_test PROPERTIES TIMEOUT 15 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

ecm_add_test(NoteCardTest.cpp ../src/Imaging/NoteCard.cpp TEST_NAME note_card_test LINK_LIBRARIES Qt::Test Qt::Gui)
set_tests_properties(note_card_test PROPERTIES TIMEOUT 15 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Local QR/barcode decode with generated Prison fixtures and no external actions.
ecm_add_test(CodeScannerTest.cpp ../src/Imaging/CodeScanner.cpp
    TEST_NAME code_scanner_test LINK_LIBRARIES Qt::Test Qt::Gui KF6::Prison KF6::PrisonScanner)
set_tests_properties(code_scanner_test PROPERTIES TIMEOUT 20 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Physical-pixel image sampling, color-space normalization and immutable snapshots.
ecm_add_test(ColorSampleTest.cpp ../src/Imaging/ColorSample.cpp
    TEST_NAME color_sample_test LINK_LIBRARIES Qt::Test Qt::Gui)
set_tests_properties(color_sample_test PROPERTIES TIMEOUT 20 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Real local FFmpeg trim/re-encode and atomic output; no desktop capture claim.
ecm_add_test(VideoClipTest.cpp ../src/Media/VideoClipJob.cpp ../src/Media/VideoClipJob.h
    TEST_NAME video_clip_test LINK_LIBRARIES Qt::Test Qt::Core)
set_tests_properties(video_clip_test PROPERTIES TIMEOUT 180)

# Portal protocol integration: explicitly supplied private session bus only.
add_executable(screen_color_picker_test ScreenColorPickerTest.cpp ../src/Platforms/ScreenColorPicker.cpp ../src/Platforms/ScreenColorPicker.h)
target_link_libraries(screen_color_picker_test PRIVATE Qt::Test Qt::Gui Qt::DBus)
find_program(DBUS_RUN_SESSION_EXECUTABLE dbus-run-session)
if(DBUS_RUN_SESSION_EXECUTABLE)
    add_test(NAME screen_color_picker_test COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} -- ${CMAKE_COMMAND} -E env
        NKSHOT_TEST_PRIVATE_BUS=1 $<TARGET_FILE:screen_color_picker_test>)
else()
    # Isolated environments can supply a private bus explicitly. Never silently
    # attach the test portal to a user's ordinary desktop session.
    add_test(NAME screen_color_picker_test COMMAND screen_color_picker_test)
endif()
set_tests_properties(screen_color_picker_test PROPERTIES TIMEOUT 45)

# Production startup QML, synthetic capability/settings fixtures, no desktop capture.
ecm_add_test(StartupSetupTest.cpp TEST_NAME startup_setup_test
    LINK_LIBRARIES Qt::Test Qt::Quick Qt::Qml KF6::I18n KF6::I18nQml Kos::Ui Kos::UiPlugin)
target_compile_definitions(startup_setup_test PRIVATE
    STARTUP_PANE_PATH="${PROJECT_SOURCE_DIR}/src/Gui/CaptureSetupPane.qml")
set_tests_properties(startup_setup_test PROPERTIES TIMEOUT 60
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")

# Post-selection recording preparation timing and producer lifetime contracts.
ecm_add_test(RecordingPreparationTest.cpp ../src/RecordingPreparation.cpp ../src/RecordingPreparation.h
    TEST_NAME recording_preparation_test LINK_LIBRARIES Qt::Test Qt::Core)
ecm_add_test(RecordingStartGuardTest.cpp TEST_NAME recording_start_guard_test LINK_LIBRARIES Qt::Test Qt::Core)
set_tests_properties(recording_preparation_test recording_start_guard_test PROPERTIES TIMEOUT 30)

# 077: exclusive, identity-bound private recording outputs.
ecm_add_test(OwnedRecordingOutputTest.cpp ../src/Platforms/OwnedRecordingOutput.cpp ../src/Platforms/OwnedRecordingOutput.h
    TEST_NAME owned_recording_output_test LINK_LIBRARIES Qt::Test Qt::Core)
set_tests_properties(owned_recording_output_test PROPERTIES TIMEOUT 30)

# 078: real private-bus status-host availability, not shell rendering acceptance.
ecm_add_test(StatusNotifierAvailabilityTest.cpp ../src/Platforms/StatusNotifierAvailability.cpp ../src/Platforms/StatusNotifierAvailability.h
    TEST_NAME status_notifier_availability_test LINK_LIBRARIES Qt::Test Qt::Core Qt::DBus)
set_tests_properties(status_notifier_availability_test PROPERTIES TIMEOUT 30)

ecm_add_test(ScrollFrameAlignerTest.cpp ../src/Imaging/ScrollFrameAligner.cpp
    TEST_NAME scroll_frame_aligner_test LINK_LIBRARIES Qt::Test Qt::Gui)
set_tests_properties(scroll_frame_aligner_test PROPERTIES TIMEOUT 45)
