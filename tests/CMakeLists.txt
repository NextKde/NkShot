include_directories(${PROJECT_SOURCE_DIR}/src)

SET(FILENAME_TEST_SRCS
    FilenameTest.cpp
    ../src/ShortcutActions.cpp
    ../src/ExportManager.cpp
    ../src/RecentMediaModel.cpp
    ../src/RecentMediaModel.h
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
    TEST_NAME video_clip_test LINK_LIBRARIES Qt::Test Qt::Core Qt::Gui)
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

# 084: cumulative recording clock, pause/resume without new capture.
ecm_add_test(RecordingPauseClockTest.cpp ../src/Platforms/VideoPlatform.cpp ../src/Platforms/VideoPlatform.h
    ../src/RecordingPreparation.cpp ../src/RecordingPreparation.h
    TEST_NAME recording_pause_clock_test LINK_LIBRARIES Qt::Test Qt::Core Qt::Gui Qt::Qml)
set_tests_properties(recording_pause_clock_test PROPERTIES TIMEOUT 15)

# 085: local-only staged media loading, without mutating the active document.
ecm_add_test(LocalMediaLoaderTest.cpp ../src/LocalMediaLoader.cpp ../src/LocalMediaLoader.h
    TEST_NAME local_media_loader_test LINK_LIBRARIES Qt::Test Qt::Core Qt::Gui KF6::I18n)
target_include_directories(local_media_loader_test PRIVATE ../src)
set_tests_properties(local_media_loader_test PROPERTIES TIMEOUT 60)

# 096: bounded local-only recent-file metadata, without deleting media files.
ecm_add_test(RecentMediaModelTest.cpp ../src/RecentMediaModel.cpp ../src/RecentMediaModel.h
    TEST_NAME recent_media_model_test LINK_LIBRARIES Qt::Test Qt::Core)
set_tests_properties(recent_media_model_test PROPERTIES TIMEOUT 30)

# Immutable sharing-preview session and bounded exact-composition renderer.
ecm_add_test(PresentationPreviewTest.cpp ../src/Gui/PresentationPreview.cpp ../src/Gui/PresentationPreview.h ../src/Imaging/ImageTransforms.cpp
    TEST_NAME presentation_preview_test LINK_LIBRARIES Qt::Test Qt::Quick KF6::I18n)
set_tests_properties(presentation_preview_test PROPERTIES TIMEOUT 20 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Reusable atomic JPEG / lossless WebP sharing encoder; no native capture claim.
ecm_add_test(ImageExportTest.cpp ../src/Imaging/ImageExport.cpp
    TEST_NAME image_export_test LINK_LIBRARIES Qt::Test Qt::Gui)
set_tests_properties(image_export_test PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Lexical local-file drag admission; no filesystem or network reads on hover.
ecm_add_test(MediaDropPolicyTest.cpp ../src/MediaDropPolicy.cpp
    TEST_NAME media_drop_policy_test LINK_LIBRARIES Qt::Test Qt::Core)
set_tests_properties(media_drop_policy_test PROPERTIES TIMEOUT 10)

# Reusable bounded local light/color adjustment; no upstream editor mutation.
ecm_add_test(ImageAdjustmentsTest.cpp ../src/Imaging/ImageAdjustments.cpp
    TEST_NAME image_adjustments_test LINK_LIBRARIES Qt::Test Qt::Gui)
set_tests_properties(image_adjustments_test PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

ecm_add_test(ImageAdjustmentsPreviewTest.cpp ../src/Gui/ImageAdjustmentsPreview.cpp ../src/Gui/ImageAdjustmentsPreview.h ../src/Imaging/ImageAdjustments.cpp
    TEST_NAME image_adjustments_preview_test LINK_LIBRARIES Qt::Test Qt::Quick KF6::I18n)
set_tests_properties(image_adjustments_preview_test PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Bounded, read-only local recent-file previews.
ecm_add_test(MediaThumbnailTest.cpp ../src/Imaging/MediaThumbnail.cpp
    TEST_NAME media_thumbnail_test LINK_LIBRARIES Qt::Test Qt::Gui)
set_tests_properties(media_thumbnail_test PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

ecm_add_test(RecentThumbnailsTest.cpp ../src/Gui/RecentThumbnails.cpp ../src/Gui/RecentThumbnails.h ../src/Imaging/MediaThumbnail.cpp
    TEST_NAME recent_thumbnails_test LINK_LIBRARIES Qt::Test Qt::Quick Qt::Concurrent)
set_tests_properties(recent_thumbnails_test PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Literal OCR-word matching and independent opaque covering renderer.
ecm_add_test(TextCoverTest.cpp ../src/Imaging/TextCover.cpp
    TEST_NAME text_cover_test LINK_LIBRARIES Qt::Test Qt::Gui)
set_tests_properties(text_cover_test PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
ecm_add_test(TextCoverSessionTest.cpp ../src/Gui/TextCoverSession.cpp ../src/Gui/TextCoverSession.h ../src/Imaging/TextCover.cpp ../src/Imaging/LocalTextExtraction.cpp
    TEST_NAME text_cover_session_test LINK_LIBRARIES Qt::Test Qt::Quick Qt::Concurrent KF6::I18n PkgConfig::TESSERACT)
target_compile_options(text_cover_session_test PRIVATE -fexceptions)
set_tests_properties(text_cover_session_test PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# GUI recording active-time limit, not native device acceptance.
ecm_add_test(RecordingAutoStopTest.cpp ../src/RecordingAutoStop.cpp ../src/RecordingAutoStop.h
    TEST_NAME recording_auto_stop_test LINK_LIBRARIES Qt::Test Qt::Core)
set_tests_properties(recording_auto_stop_test PROPERTIES TIMEOUT 30)

# Reviewed immutable local image composition.
ecm_add_test(ImageCombineSessionTest.cpp ../src/Gui/ImageCombineSession.cpp ../src/Gui/ImageCombineSession.h ../src/Imaging/VerticalStitcher.cpp
    TEST_NAME image_combine_session_test LINK_LIBRARIES Qt::Test Qt::Quick Qt::Concurrent)
set_tests_properties(image_combine_session_test PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
