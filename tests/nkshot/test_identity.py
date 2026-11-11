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
                     'EllipseTool', 'PixelateTool', 'BlurTool', 'TextTool']:
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
        start = source[source.index('auto startRecordingTask ='):source.index('void VideoPlatformWayland::finishRecording()')]
        stream = start.index('m_screencasting->createOutputStream')
        # Structural boundary, not a claim of real compositor execution.
        for marker in ['m_recorder->setOutput(QString())', 'mkDirPath(tempUrl)',
                       'mkDirPath(fileUrl)', 'm_recorder->setOutput(localFile)',
                       'setRecordMicrophone(Settings::videoRecordMicrophone() && audioSupported)']:
            self.assertLess(start.index(marker), stream)
        self.assertEqual(start.count('encoder == Encoder::NoEncoder'), 2)
        self.assertIn('if (!stream)', start)


if __name__ == '__main__':
    unittest.main()
