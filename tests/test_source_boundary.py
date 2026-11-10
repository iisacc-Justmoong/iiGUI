"""Regression tests for iiGUI source and package dependency guards."""
import unittest

from source_boundary import (forbidden_cmake_dependencies, forbidden_component_rendering,
                             forbidden_source_dependencies)


class SourceBoundaryTests(unittest.TestCase):
    def test_component_surface_rejects_embedded_browsers_and_os_controls(self):
        for source in ('NSButton* button;', 'NSTextField* label;', 'UIButton *button;',
                       'UITextField* field;', 'WKWebView* browser;', 'ICoreWebView2* view;',
                       'CefBrowserHost* host;', 'CreateWindowW(L"BUTTON", title, style);',
                       'CreateWindowExW(0, L"Edit", title, style);',
                       'CreateWindowExW(0, WC_BUTTON, title, style);'):
            with self.subTest(source=source):
                self.assertTrue(forbidden_component_rendering(source))

    def test_component_surface_allows_os_services_and_web_target(self):
        for source in ('NSView* surface; NSTextInputContext* input;',
                       'UIView* surface; id<UITextInput> input;',
                       'CreateWindowExW(0, class_name.c_str(), title.c_str(), style);',
                       'WebSurfaceParent& parent; glDrawArraysInstanced(mode, 0, 6, count);'):
            with self.subTest(source=source):
                self.assertFalse(forbidden_component_rendering(source))

    def test_source_rejects_lvui_lvrs_and_framework_dependencies(self):
        for source in ('#include <LVUI/Button.h>', '#include "LVRS/Widgets.h"',
                       '#include <lvrs/Widgets.h>', 'lvui::Button b;', 'LVRS::Button b;',
                       'namespace lvrs { class Button; }', 'namespace LVRS { class Button; }',
                       '#include <QtCore/QObject>', '#include <QQuickItem>', '#include <qml/Component.h>',
                       '#include <WebKit/WebKit.h>', '#include <iiNetwork/Http.h>',
                       '#include <iiFileProviderNative.h>', '#include "iiVisualCodec/ImageCodec.h"'):
            with self.subTest(source=source):
                self.assertTrue(forbidden_source_dependencies(source))

    def test_package_rejects_direct_forbidden_dependencies(self):
        for source in ('find_package(LVUI CONFIG REQUIRED)', 'find_dependency(LVRS CONFIG)',
                       'find_package("Qt6" CONFIG REQUIRED)',
                       'find_package(Qt6 COMPONENTS Core)', 'find_dependency(QML)',
                       'target_link_libraries(iiGUI PRIVATE LVRS::Native)',
                       'target_link_libraries(iiGUI PUBLIC Qt6::Core)',
                       'target_link_libraries(iiGUI PRIVATE LVUI::Native)',
                       'target_link_libraries(iiGUI PRIVATE -lQt6Core)',
                       'find_package(qml CONFIG)',
                       'find_package(iiNetwork CONFIG REQUIRED)',
                       'find_dependency(iiFileProviderNative CONFIG)',
                       'target_link_libraries(iiGUI PRIVATE iiColorManager::iiColorManager)',
                       'target_link_libraries(iiGUI PRIVATE nested(test) LVRS::Native)'):
            with self.subTest(source=source):
                self.assertTrue(forbidden_cmake_dependencies(source))

    def test_windows_host_text_uses_widget_surface(self):
        from pathlib import Path
        source = (Path(__file__).resolve().parents[1] / 'src/Platform/HostWindows.cpp').read_text()
        self.assertNotIn('DrawTextLayout', source)
        self.assertNotIn('CreateHwndRenderTarget', source)
        self.assertIn('build_legacy_text', source)
        self.assertIn('legacy_text.set_text', source)

    def test_x11_host_text_uses_widget_surface(self):
        from pathlib import Path
        source = (Path(__file__).resolve().parents[1] / 'src/Platform/HostLinux.cpp').read_text()
        self.assertNotIn('pango_cairo_show_layout', source)
        self.assertNotIn('cairo_xlib_surface_create', source)
        self.assertIn('build_legacy_text', source)
        self.assertIn('legacy_text.set_text', source)

    def test_unrelated_text_and_iisdk_are_allowed(self):
        self.assertFalse(forbidden_source_dependencies('// LVRS migration history\n#include <iiGUI/Host.h>\n'))
        self.assertFalse(forbidden_source_dependencies('#include <iiSDKCore/Runtime/Runtime.h>\n'))
        self.assertFalse(forbidden_cmake_dependencies('find_dependency(iiSDKCore CONFIG)\n'
                                                      'target_link_libraries(iiGUI PUBLIC iiSDKCore::Runtime)\n'))


if __name__ == "__main__":
    unittest.main()
