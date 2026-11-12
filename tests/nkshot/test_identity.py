"""Source-level regression checks; these do not replace a Plasma runtime test."""
from pathlib import Path
import configparser
import hashlib
import json
import re
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
APP_ID = 'org.nextkde.nkshot'


def read(path):
    return (ROOT / path).read_text()


class IdentityTests(unittest.TestCase):
    def test_shared_editor_navigation_is_view_only(self):
        nav = read('src/Gui/ImageViewportNavigation.qml')
        editor = read('src/Gui/ImageCopyEditor.qml')
        self.assertIn('acceptedButtons: Qt.MiddleButton', nav)
        self.assertIn('textInputFocused', nav)
        self.assertIn('mapFromItem', nav)
        self.assertIn('cancelPan', nav)
        self.assertNotIn('SpectacleCore', nav)
        self.assertNotIn('AnnotationDocument', nav)
        self.assertIn('nkshotCopyEditorActualSize', editor)
        self.assertIn('enabled: !copyEditor.comparing', editor)
        self.assertIn('Gui/ImageViewportNavigation.qml', read('src/CMakeLists.txt'))

    def test_before_preview_is_immutable_and_output_safe(self):
        h = read('src/Gui/ImageCopyEditor.h')
        cpp = read('src/Gui/ImageCopyEditor.cpp')
        qml = read('src/Gui/ImageCopyEditor.qml')
        for token in ('originalImage', 'compareAvailable', 'beginComparison', 'endComparison'):
            self.assertIn(token, h)
        self.assertIn('m_originalImage', cpp)
        self.assertIn('QEvent::InputMethod', cpp)
        self.assertIn('nkshotCopyEditorCompare', qml)
        self.assertIn('Before editing', qml)
        self.assertIn('Qt.TabFocus', qml)
        self.assertNotIn('setBaseImage', qml)

    def test_export_speed_is_separate_from_playback(self):
        job = read('src/Media/VideoClipJob.h')
        popup = read('src/Gui/VideoClipPopup.qml')
        viewer = read('src/Gui/ViewerWindow.cpp')
        for token in ('exportSpeed', 'outputDurationMs', 'setExportRange'):
            self.assertIn(token, job)
        self.assertIn('nkshotClipExportSpeed', popup)
        self.assertIn('syncExportRange', popup)
        self.assertIn('exportSpeed', viewer)
        self.assertNotIn('player.playbackRate =', popup)

    def test_mp4_share_resolution_is_export_scoped(self):
        job = read('src/Media/VideoClipJob.h')
        popup = read('src/Gui/VideoClipPopup.qml')
        viewer = read('src/Gui/ViewerWindow.cpp')
        self.assertIn('outputResolution', job)
        self.assertIn('outputSize', job)
        self.assertIn('nkshotClipResolution', popup)
        self.assertIn('Max 1920 px', popup)
        self.assertIn('Max 1280 px', popup)
        self.assertIn('resolutionValid', popup)
        self.assertIn('parent: detailScroll', popup)
        self.assertIn('height: detailScroll.availableHeight', popup)
        self.assertIn('outputResolution', viewer)

    def test_horizontal_composition_has_one_contextual_entry(self):
        page = read('src/Gui/ViewerPage.qml')
        popup = read('src/Gui/ImageCombinePopup.qml')
        core = read('src/Gui/ViewerWindow.cpp')
        api = read('src/Imaging/VerticalStitcher.h')
        self.assertIn('ImageCombinePopup', page)
        self.assertNotIn('nkshotStackImagesAction', page)
        for name in ('Vertical', 'Horizontal', 'Match', 'Stack', 'Choose', 'Cancel'):
            self.assertIn('nkshotCombine' + name, popup)
        self.assertIn('autoExclusive: true', popup)
        self.assertIn('not selection order', popup)
        self.assertIn('stackHorizontal', api)
        self.assertIn('stitchHorizontal', api)
        self.assertIn('same height', core)
        self.assertIn('32000', core)

    def test_global_shortcuts_are_opt_in_and_preserve_custom_keys(self):
        core = read('src/SpectacleCore.cpp')
        self.assertNotIn('setGlobalShortcut(', core)
        self.assertIn('setDefaultShortcut(action, {})', core)
        self.assertIn('setShortcut(action, {}, KGlobalAccel::Autoloading)', core)
        self.assertNotIn('KGlobalAccel::NoAutoloading', core)

    def test_desktop_identity_and_actions(self):
        config = configparser.ConfigParser(interpolation=None, strict=False)
        config.read_string(read(f'desktop/{APP_ID}.desktop.cmake'))
        main = config['Desktop Entry']
        self.assertEqual(main['Name'], 'NkShot')
        self.assertEqual(main['Icon'], 'nkshot')
        self.assertEqual(main['X-DBUS-ServiceName'], APP_ID)
        for action in main['Actions'].split(';'):
            if action:
                self.assertIn(f'Desktop Action {action}', config)
        for section in config.values():
            if 'Exec' in section:
                self.assertIn('/nkshot', section['Exec'])
            self.assertNotIn('X-KDE-Shortcuts', section)

    def test_kwin_permissions_retained(self):
        desktop = read(f'desktop/{APP_ID}.desktop.cmake')
        self.assertIn('X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2', desktop)
        self.assertIn('X-KDE-Wayland-Interfaces=org_kde_plasma_window_management,zkde_screencast_unstable_v1', desktop)

    def test_dbus_identity_matches(self):
        xml = ET.fromstring(read(f'dbus/{APP_ID}.xml'))
        self.assertEqual(xml.find('interface').attrib['name'], APP_ID)
        self.assertIn(f'"D-Bus Interface", "{APP_ID}"', read('src/SpectacleDBusAdapter.h'))
        self.assertIn(f'BusName={APP_ID}', read(f'dbus/app-{APP_ID}.service.in'))
        self.assertIn('/nkshot --dbus', read('dbus/CMakeLists.txt'))
        self.assertNotIn('org.kde.Spectacle', read('dbus/CMakeLists.txt'))

    def test_config_is_separate(self):
        tree = ET.fromstring(read('src/Gui/SettingsDialog/nkshot.kcfg'))
        cfg = next(e for e in tree.iter() if e.tag.endswith('kcfgfile'))
        self.assertEqual(cfg.attrib['name'], 'nkshotrc')
        self.assertIn('File=nkshot.kcfg', read('src/Gui/SettingsDialog/settings.kcfgc'))
        self.assertNotIn('add_subdirectory(kconf_update)', read('CMakeLists.txt'))

    def test_appstream_identity(self):
        tree = ET.fromstring(read(f'desktop/{APP_ID}.appdata.xml'))
        self.assertEqual(tree.findtext('id'), APP_ID)
        self.assertEqual(tree.findtext('launchable'), APP_ID + '.desktop')
        self.assertEqual(tree.findtext('name'), 'NkShot')
        self.assertEqual(tree.findtext('project_license'), 'GPL-3.0-only')

    def test_install_identity(self):
        self.assertIn('OUTPUT_NAME nkshot', read('src/CMakeLists.txt'))
        self.assertIn('Gui/SettingsDialog/nkshot.kcfg', read('src/CMakeLists.txt'))
        self.assertIn('sc-apps-nkshot.svg', read('icons/CMakeLists.txt'))
        self.assertTrue((ROOT / 'icons/sc-apps-nkshot.svg').is_file())
        self.assertIn('nkshot.notifyrc', read('desktop/CMakeLists.txt'))
        self.assertNotIn('org.kde.spectacle', read('desktop/CMakeLists.txt'))
        self.assertIn('FILE nkshot.categories', read('CMakeLists.txt'))

    def test_runtime_identity(self):
        main = read('src/Main.cpp')
        self.assertIn('KAboutData aboutData(u"nkshot"_s', main)
        self.assertIn('setOrganizationDomain(u"nextkde.org"_s)', main)
        self.assertIn(f'aboutData.setDesktopFileName(u"{APP_ID}"_s)', main)
        self.assertIn('aboutData.setOrganizationDomain(QByteArrayLiteral("nextkde.org"))', main)
        self.assertIn('setApplicationDomain(QByteArrayLiteral("nkshot"))', main)
        self.assertIn(f'registerService(u"{APP_ID}"_s)', main)

    def test_independent_copy_editor_owns_document(self):
        implementation = read('src/Gui/ImageCopyEditor.cpp')
        self.assertNotIn('SpectacleCore', implementation)
        self.assertIn('new AnnotationDocument(this)', implementation)
        editor = read('src/Gui/ImageCopyEditor.qml')
        self.assertIn('readonly property var document: copyEditor.document', editor)
        self.assertGreaterEqual(editor.count('editorDocument: root.document'), 4)
        self.assertIn('QMessageBox::Discard|QMessageBox::Cancel', implementation)
        self.assertIn('pinnedWindow.editCopy()', read('src/Gui/PinnedImage.qml'))
        self.assertIn('QSaveFile file(paths.first())', implementation)
        self.assertIn('m_savedImage=snapshot', implementation)
        self.assertIn('hasUnsavedChanges()', implementation)
        self.assertIn('StandardKey.Save', editor)

    def test_clipboard_import_is_explicit_and_isolated(self):
        impl = read('src/Gui/ViewerWindow.cpp')
        body = impl.split('QQuickWindow *ViewerWindow::editClipboardImage()')[1].split('void ViewerWindow::finishOpeningMedia()')[0]
        self.assertIn('if(!canOpenMedia())return nullptr', body)
        self.assertIn('mime->hasImage()', body)
        self.assertIn('new ImageCopyEditor(image,engine())', body)
        self.assertNotIn('setBaseImage', body)
        self.assertNotIn('setImage(', body)
        self.assertNotIn('dataChanged', impl)
        for page in ('ViewerPage.qml', 'DialogPage.qml'):
            self.assertIn('contextWindow.editClipboardImage()', read('src/Gui/' + page))

    def test_video_crop_preview_is_injected(self):
        overlay = read('src/Gui/CropPreviewOverlay.qml')
        self.assertNotIn('SpectacleCore', overlay)
        for prop in ('sourceSize', 'cropRect', 'displayRect'):
            self.assertRegex(overlay, r'required property \w+ ' + prop)
        self.assertIn('videoOutput.contentRect', read('src/Gui/RecordingView.qml'))
        self.assertIn('m_clipJob->cropAspect() != cropAspect', read('src/Gui/ViewerWindow.cpp'))
        self.assertIn('m_clipJob->cropRect() != cropRect', read('src/Gui/ViewerWindow.cpp'))
        self.assertIn('signal positionRequested', overlay)
        self.assertIn('signal centerRequested', overlay)

    def test_auto_stop_requires_explicit_gui_request(self):
        core = read('src/SpectacleCore.h')
        self.assertIn('int autoStopSeconds = 0', core)
        impl = read('src/SpectacleCore.cpp')
        self.assertIn('m_startMode==StartMode::Gui', impl)
        self.assertIn('updateRecordingAutoStop', impl)
        for filename in ('DialogPage.qml', 'RecordingModeButtonsColumn.qml'):
            self.assertIn('Settings.recordingAutoStopSeconds', read('src/Gui/' + filename))
        self.assertNotIn('recordingAutoStopSeconds', read('src/SpectacleDBusAdapter.cpp'))
        self.assertIn('m_recordingAutoStopEpoch', impl)
        screenshot = impl.split('void SpectacleCore::takeNewScreenshot(ImagePlatform::GrabMode', 1)[1].split('void SpectacleCore::takeNewScreenshot(int', 1)[0]
        cancel = impl.split('void SpectacleCore::cancelScreenshot()', 1)[1].split('\n}', 1)[0]
        self.assertNotIn('m_recordingAutoStop.invalidate', screenshot)
        self.assertNotIn('m_recordingAutoStop.invalidate', cancel)
        policy = read('src/RecordingAutoStop.cpp')
        self.assertIn('QPointer<RecordingAutoStop>', policy)
        self.assertIn('QTimer::singleShot', policy)
        self.assertNotIn('QElapsedTimer', policy)
        self.assertIn('recordingAutoStopSeconds', read('src/Gui/SettingsDialog/nkshot.kcfg'))

    def test_free_crop_keeps_export_and_preview_contract(self):
        overlay = read('src/Gui/CropPreviewOverlay.qml')
        self.assertIn('signal rectRequested', overlay)
        self.assertNotIn('VideoClipJob', overlay)
        self.assertNotIn('SpectacleCore', overlay)
        view = read('src/Gui/RecordingView.qml')
        self.assertIn('setCropRect', view)
        self.assertIn('!clipPopup.locked', view)
        self.assertIn('!clipPopup.editingPrivacy', view)
        job = read('src/Media/VideoClipJob.cpp')
        self.assertIn('void VideoClipJob::setCropRect(', job)
        self.assertIn('m_freeCropRect', job)
        self.assertIn('m_clipJob->cropRect() != cropRect', read('src/Gui/ViewerWindow.cpp'))

    def test_video_annotations_reuse_isolated_editor(self):
        impl = read('src/Gui/ImageCopyEditor.cpp')
        self.assertIn('annotationsImage().copy()', impl)
        self.assertIn('m_document->transform().isIdentity()', impl)
        self.assertIn('annotationsCommitted(overlay)', impl)
        toolbar = read('src/Gui/AnnotationsToolBarContents.qml')
        self.assertIn('property bool annotationOnly: false', toolbar)
        self.assertIn('m_clipJob->annotationRevision() != annotationRevision', read('src/Gui/ViewerWindow.cpp'))
        preview = read('src/Gui/ImageOverlayPreview.cpp')
        self.assertNotIn('SpectacleCore', preview)
        self.assertNotIn('VideoClipJob', preview)

    def test_video_privacy_overlay_is_injected(self):
        overlay = read('src/Gui/PrivacyRegionOverlay.qml')
        self.assertNotIn('SpectacleCore', overlay)
        self.assertNotIn('contextWindow', overlay)
        for prop in ('sourceSize', 'region', 'displayRect'):
            self.assertRegex(overlay, r'required property \w+ ' + prop)
        self.assertIn('signal rectRequested', overlay)
        self.assertIn('color: "black"', overlay)
        impl = read('src/Gui/ViewerWindow.cpp')
        self.assertIn('m_clipJob->privacyEnabled() != privacyEnabled', impl)
        self.assertIn('m_clipJob->privacyRect() != privacyRect', impl)

    def test_recent_files_reuse_validated_open(self):
        impl = read('src/Gui/ViewerWindow.cpp')
        self.assertIn('void ViewerWindow::loadOpenedMedia(const QUrl &url)', impl)
        self.assertGreaterEqual(impl.count('loadOpenedMedia(url)'), 2)
        self.assertIn('m_mediaLoader->validatedSourceUnchanged()', impl)
        self.assertIn('if(core->commitOpenedMedia(source, image, video))', impl)
        popup = read('src/Gui/RecentMediaPopup.qml')
        self.assertIn('Text.PlainText', popup)
        self.assertIn('Remove from list, not from disk', popup)
        for page in ('ViewerPage.qml', 'DialogPage.qml'):
            self.assertIn('nkshotRecentMediaAction', read('src/Gui/' + page))

    def test_capture_actions_still_present(self):
        overlay = read('src/Gui/CaptureOverlay.qml')
        for action in ['AcceptAction', 'CancelAction', 'SaveAction', 'SaveAsAction',
                       'CopyImageAction', 'OcrAction', 'RecordAction']:
            self.assertRegex(overlay, r'action:\s*' + action + r'\s*\{')
        button = read('src/Gui/TtToolButton.qml')
        for state in ['root.visualFocus', 'root.down', 'root.checked', 'root.hovered', 'root.enabled']:
            self.assertIn(state, button)

    def test_post_capture_editing_route(self):
        viewer = read('src/Gui/ViewerPage.qml')
        self.assertIn('action: EditAction {}', viewer)
        self.assertIn('contextWindow.annotating', viewer)
        self.assertIn('onToggled: contextWindow.annotating = checked', read('src/Gui/EditAction.qml'))
        self.assertIn('required property AnnotationDocument editorDocument', read('src/Gui/AnnotationEditor.qml'))
        self.assertIn('document: editorDocument', read('src/Gui/AnnotationEditor.qml'))
        for path in ['ScreenshotView.qml', 'CaptureOverlay.qml']:
            self.assertIn('editorDocument: SpectacleCore.annotationDocument', read('src/Gui/' + path))

    def test_annotation_tools_and_undo_redo_retained(self):
        toolbar = read('src/Gui/AnnotationsToolBarContents.qml')
        for tool in ['CropTool', 'FreehandTool', 'ArrowTool', 'RectangleTool',
                     'EllipseTool', 'PixelateTool', 'BlurTool', 'TextTool',
                     'SelectTool', 'HighlighterTool', 'LineTool', 'NumberTool']:
            self.assertIn('AnnotationTool.' + tool, toolbar)
        editor = read('src/Gui/AnnotationEditor.qml')
        self.assertIn('onActivated: root.document.undo()', editor)
        self.assertIn('onActivated: root.document.redo()', editor)

    def test_editor_controls_receive_document_explicitly(self):
        for name in ['AnnotationsToolBarContents.qml', 'AnnotationOptionsToolBarContents.qml', 'UndoRedoGroup.qml']:
            content = read('src/Gui/' + name)
            self.assertIn('required property AnnotationDocument editorDocument', content)
            self.assertNotIn('SpectacleCore.annotationDocument', content)
        self.assertIn('editorDocument: root.editorDocument', read('src/Gui/AnnotationsToolBarContents.qml'))

    def test_save_and_copy_routes_retained(self):
        self.assertIn('onTriggered: contextWindow.save()', read('src/Gui/SaveAction.qml'))
        self.assertIn('onTriggered: contextWindow.copyImage()', read('src/Gui/CopyImageAction.qml'))
        for name in ['SaveAction.qml', 'CopyImageAction.qml', 'EditAction.qml']:
            self.assertIn('enabled: !SpectacleCore.videoMode', read('src/Gui/' + name))

    def test_recording_and_ocr_gates_retained(self):
        viewer = read('src/Gui/ViewerPage.qml')
        self.assertIn('RecordingModeMenu.popup(moreButton)', viewer)
        self.assertIn('supportedRecordingModes !== VideoPlatform.NoRecordingModes', viewer)
        self.assertIn('action: OcrAction {}', viewer)
        self.assertIn('SpectacleCore.ocrAvailable', viewer)
        self.assertIn('!SpectacleCore.videoMode', viewer)

    def test_kos_sources_match_pinned_snapshot(self):
        base = ROOT / 'third_party/kos-ui'
        manifest = json.loads((base / 'SOURCE.json').read_text())
        for name, digest in manifest['files'].items():
            self.assertEqual(hashlib.sha256((base / name).read_bytes()).hexdigest(), digest, name)
        cmake = (base / 'shared/qml/CMakeLists.txt').read_text()
        for name in re.findall(r'^\s+((?:foundation|controls|i18n)/[^\s)]+)', cmake, re.M):
            self.assertTrue((base / 'shared/qml' / name).is_file(), name)

    def test_material_scene_and_accessibility_settings(self):
        capture = read('src/Gui/CaptureOverlay.qml')
        self.assertIn('sourceItem: annotationsLoader.item', capture)
        self.assertIn('Kos.LiquidBackdrop', capture)
        toolbar = read('src/Gui/FloatingToolBar.qml')
        self.assertIn('Kos.LiquidPanelSurface', toolbar)
        self.assertIn('reduceTransparency: !Settings.useLiquidMaterial', toolbar)
        ui = ET.fromstring(read('src/Gui/SettingsDialog/GeneralOptions.ui'))
        names = {widget.attrib.get('name') for widget in ui.iter('widget')}
        self.assertIn('kcfg_useLiquidMaterial', names)
        self.assertIn('kcfg_reduceUiMotion', names)

    def test_recording_output_precedes_capture_stream(self):
        source = read('src/Platforms/VideoPlatformWayland.cpp')
        start = source[source.index('void VideoPlatformWayland::commitRecording'):source.index('void VideoPlatformWayland::finishRecording()')]
        stream = start.index('m_screencasting->createOutputStream')
        # Structural boundary, not a claim of real compositor execution.
        for marker in ['m_recorder->setOutput(QString())', 'm_ownedPreparedOutput->reserve(',
                       'mkDirPath(fileUrl)', 'm_recorder->setOutput(localFile)',
                       'setRecordMicrophone(m_requestMicrophone && audioSupported)']:
            self.assertLess(start.index(marker), stream)
        self.assertEqual(start.count('encoder == Encoder::NoEncoder'), 2)
        self.assertIn('QIODevice::WriteOnly|QIODevice::NewOnly', start)
        self.assertIn('m_ownedPreparedOutput->identityMatches()', start)
        self.assertIn('if (!stream)', start)

    def test_media_drop_reuses_guarded_open_and_copy_only(self):
        source = read('src/Gui/ViewerWindow.cpp')
        body = source[source.index('bool ViewerWindow::openDroppedMedia'):source.index('QQuickWindow *ViewerWindow::editClipboardImage')]
        self.assertIn('canAcceptMediaDrop(urls)', body)
        self.assertIn('openRecentMedia(urls.first())', body)
        qml = read('src/Gui/MediaDropTarget.qml')
        self.assertIn('drop.accept(Qt.CopyAction)', qml)
        self.assertNotIn('Qt.MoveAction', qml)
        policy = read('src/MediaDropPolicy.cpp')
        self.assertNotIn('QFile', policy)
        self.assertIn('urls.size() != 1', policy)
        self.assertIn('!url.isLocalFile()', policy)

    def test_explicit_image_export_keeps_png_save_point(self):
        source = read('src/Gui/ImageCopyEditor.cpp')
        body = source[source.index('bool ImageCopyEditor::exportAs'):source.index('QQuickWindow *ImageCopyEditor::finishCopy')]
        self.assertIn('const QImage snapshot = m_document->renderToImage()', body)
        self.assertIn('saveImageCopy(snapshot', body)
        self.assertNotIn('m_savedImage=', body)
        self.assertNotIn('m_savedImage =', body)
        encoder = read('src/Imaging/ImageExport.cpp')
        self.assertIn('setDirectWriteFallback(false)', encoder)
        self.assertIn('flattened.fill(Qt::white)', encoder)
        self.assertIn('format == ImageExportFormat::Jpeg ? quality : 100', encoder)

    def test_presentation_preview_uses_immutable_bounded_shared_renderer(self):
        source = read('src/Gui/PresentationPreview.cpp')
        self.assertIn('source.copy()', source)
        self.assertIn('Imaging::previewPresentation(m_source, m_options)', source)
        renderer = read('src/Imaging/ImageTransforms.cpp')
        self.assertIn('QImage image(renderSize', renderer)
        self.assertIn('return renderPresentation(source, options, maximumPixels, maximumEdge)', renderer)
        viewer = read('src/Gui/ViewerWindow.cpp')
        self.assertIn('const auto source = m_presentationPreview->source()', viewer)
        self.assertIn('const auto options = m_presentationPreview->options()', viewer)
        self.assertIn('sourceSnapshot.isNull() ?', viewer)

    def test_adjustments_are_isolated_and_reusable(self):
        helper = read('src/Imaging/ImageAdjustments.cpp')
        editor = read('src/Gui/ImageCopyEditor.cpp')
        popup = read('src/Gui/ImageAdjustmentsPopup.qml')
        self.assertNotIn('applyColorAdjustment', helper)
        self.assertNotIn('SpectacleCore', helper)
        self.assertIn('return source.copy()', helper)
        self.assertIn('const auto snapshot = m_adjustmentPreview->source()', editor)
        self.assertIn('const auto options = m_adjustmentPreview->options()', editor)
        self.assertIn('QtConcurrent::run([snapshot, options]', editor)
        self.assertIn('if (editToolsBlocked()) return', editor)
        self.assertIn('root.updatePreview()', popup)
        self.assertIn('resetValues()', popup)

    def test_recent_previews_are_explicit_bounded_and_volatile(self):
        decoder = read('src/Imaging/MediaThumbnail.cpp')
        cache = read('src/Gui/RecentThumbnails.cpp')
        popup = read('src/Gui/RecentMediaPopup.qml')
        self.assertIn('O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC', decoder)
        self.assertIn('same(opened,after)', decoder)
        self.assertIn('pool.setMaxThreadCount(1)', cache)
        self.assertIn('epoch==m_epoch', cache)
        self.assertIn('cancelled->load()', cache)
        self.assertIn('contextWindow.beginRecentThumbnails()', popup)
        self.assertIn('contextWindow.clearRecentThumbnails()', popup)
        self.assertIn('contextWindow.recentThumbnails.urlFor(row.url)', popup)
        self.assertNotIn('source: row.url', popup)

    def test_text_cover_is_reviewed_and_separate(self):
        renderer = read('src/Imaging/TextCover.cpp')
        session = read('src/Gui/TextCoverSession.cpp')
        editor = read('src/Gui/ImageCopyEditor.cpp')
        popup = read('src/Gui/TextCoverPopup.qml')
        self.assertIn('QImage::Format_RGB32', renderer)
        self.assertIn('result.fill(Qt::white)', renderer)
        self.assertIn('painter.fillRect(rect,Qt::black)', renderer)
        self.assertIn('!result.wordsComplete', session)
        self.assertIn('epoch==m_epoch', session)
        self.assertIn('const auto regions = m_textCoverSession->selectedRegions()', editor)
        self.assertIn('QtConcurrent::run([snapshot, regions]', editor)
        self.assertIn('clearMatches()', popup)
        self.assertIn('OCR can miss text', popup)

    def test_qml_returned_windows_keep_cpp_lifetime(self):
        for name in ('PinnedImageWindow', 'ImageCopyEditor', 'QuickNoteWindow', 'ColorPickerWindow', 'ScrollCaptureWindow'):
            source = read('src/Gui/' + name + '.cpp')
            self.assertIn('QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership)', source)

    def test_recording_callbacks_are_scoped_and_connected_once(self):
        source = read('src/Platforms/VideoPlatformWayland.cpp')
        initialize = source[source.index('void VideoPlatformWayland::initialize()'):source.index('VideoPlatform::RecordingModes VideoPlatformWayland::supportedRecordingModes')]
        start = source[source.index('void VideoPlatformWayland::commitRecording'):source.index('void VideoPlatformWayland::failRecording')]
        self.assertEqual(source.count('&NkShot::RecordingRecorder::stateChanged'), 1)
        self.assertEqual(source.count('&NkShot::RecordingRecorder::errorFound'), 1)
        self.assertIn('&NkShot::RecordingRecorder::stateChanged', initialize)
        self.assertIn('&NkShot::RecordingRecorder::errorFound', initialize)
        self.assertEqual(start.count('m_streamLease.accepts(stream,generation)'), 3)
        self.assertIn('Qt::QueuedConnection', source)
        self.assertIn('if (m_attemptFailed)', initialize)
        self.assertIn('!m_startGuard.allowsNewAttempt()', source)
        self.assertIn('!m_stopRequested && state == PipeWireRecord::Recording', initialize)
        self.assertIn('preparation()->isCurrent(token)', start)
        selection = source[source.index('void VideoPlatformWayland::selectAndRecord'): ]
        self.assertIn('preparation()->isCurrent(token)', selection)
        self.assertIn('m_recorderGeneration', initialize)


if __name__ == '__main__':
    unittest.main()
