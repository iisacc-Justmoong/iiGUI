"""Exercise layout package workspace ownership without executing a consumer."""
import contextlib
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch
import layout_install
import resource_workspace

ROOT = Path(__file__).resolve().parents[1]


class LayoutPackageWorkspaceTests(unittest.TestCase):
    def test_install_manifest_paths_are_redirected_to_owned_workspace(self):
        source='file(WRITE "/kit/build/install_local_manifest.txt" "x")\nfile(WRITE "/kit/build/${CMAKE_INSTALL_MANIFEST}" "y")\n'
        redirected=layout_install.redirect_install_manifest(source,Path('/kit/build'),Path('/owned'))
        self.assertIn('/owned/install_local_manifest.txt',redirected)
        self.assertIn('/owned/${CMAKE_INSTALL_MANIFEST}',redirected)
        self.assertNotIn('/kit/build/install_',redirected)
        with self.assertRaises(ValueError):
            layout_install.redirect_install_manifest(source+'\n'+source,Path('/kit/build'),Path('/owned'))

    def test_consumer_uses_color_dependency_from_the_selected_lvui_build(self):
        gui={'iiSDKCore_DIR':'/gui/core','CMAKE_CXX_COMPILER':'/compiler'}
        lvui={'iiColorManager_DIR':'/lvui/color','iiSDKCore_DIR':'/lvui/core'}
        options=layout_install.dependency_options(gui,lvui)
        self.assertIn('-DiiColorManager_DIR=/lvui/color',options)
        self.assertIn('-DiiSDKCore_DIR=/gui/core',options)
        self.assertIn('-DCMAKE_CXX_COMPILER=/compiler',options)
        self.assertNotIn('-DiiSDKCore_DIR=/lvui/core',options)

    def test_temporary_build_and_resource_workspace_are_removed(self):
        with resource_workspace.temporary(ROOT, "layout-test-") as test_root:
            build = test_root / "build"
            resource_workspace.provider.mkdir(build)
            with layout_install.consumer_workspace(ROOT, build) as (resources, target):
                self.assertTrue(resources.is_relative_to(ROOT / "resources/.state"))
                self.assertTrue(target.is_relative_to(build))
                resource_workspace.provider.mkdir(target, parents=True)
                resource_workspace.provider.write_text(target / "compiled-test", "test")
                resource_workspace.provider.write_text(resources / "temporary-data", "test")
            self.assertFalse(resources.exists())
            self.assertFalse(target.exists())
            self.assertFalse((build / "layout-consumer").exists())

    def test_failure_still_removes_only_owned_workspace(self):
        with resource_workspace.temporary(ROOT, "layout-test-") as test_root:
            build = test_root / "build"
            resource_workspace.provider.mkdir(build)
            keep = build / "keep"
            resource_workspace.provider.write_text(keep, "user data")
            with self.assertRaisesRegex(RuntimeError, "intentional"):
                with layout_install.consumer_workspace(ROOT, build) as (resources, target):
                    resource_workspace.provider.mkdir(target, parents=True)
                    raise RuntimeError("intentional")
            self.assertFalse(resources.exists())
            self.assertFalse(target.exists())
            self.assertEqual(keep.read_text(), "user data")

    def test_report_is_printed_and_intermediate_files_are_cleaned(self):
        with resource_workspace.temporary(ROOT, "layout-test-") as test_root:
            build = test_root / "build"
            resource_workspace.provider.mkdir(build)
            resource_workspace.provider.write_text(build / "CMakeCache.txt", "")
            lvui = test_root / "lvui-build"
            resource_workspace.provider.mkdir(lvui)
            for package in (build,lvui):
                script=package/'cmake_install.cmake'
                resource_workspace.provider.write_text(script,
                    f'file(WRITE "{package}/install_local_manifest.txt" "x")\n'
                    f'file(WRITE "{package}/${{CMAKE_INSTALL_MANIFEST}}" "y")\n')
                resource_workspace.provider.write_text(package/'install_manifest.txt','preserve')
            header_names = ('NativeWidgets.h', 'Host.h', 'Graphics.h', 'WidgetInteraction.h',
                            'WidgetLayout.h', 'WindowProperties.h', 'Appearance.h', 'FontDatabase.h',
                            'Image.h', 'Presentation.h', 'NativeView.h')
            headers = {p.name: p for area in ('API', 'Core', 'Graphics', 'Platform', 'Presentation', 'Text', 'Widgets')
                       for p in (ROOT / 'src' / area).glob('*.h')}
            values = ''.join(f'{name}:{hashlib.sha256(headers[name].read_bytes()).hexdigest()};' for name in header_names)
            abi = hashlib.sha256(values.encode()).hexdigest()
            calls = []

            def fake_run(command, **_):
                calls.append(command)
                if '-P' in command:
                    stage = Path(next(arg.split('=',1)[1] for arg in command if arg.startswith('-DCMAKE_INSTALL_PREFIX=')))
                    for relative in ('include/iiGUI/WidgetLayout.h', 'include/LVUI/NativeWidgetLayout.h',
                                     'include/iiGUI/WindowProperties.h', 'include/LVUI/NativeWindowValues.h'):
                        path = stage / relative
                        resource_workspace.provider.mkdir(path.parent, parents=True, exist_ok=True)
                        resource_workspace.provider.write_text(path, 'fixture')
                    config = stage / 'lib/cmake/iiGUI/iiGUIConfig.cmake'
                    resource_workspace.provider.mkdir(config.parent, parents=True, exist_ok=True)
                    resource_workspace.provider.write_text(config, f'set(iiGUI_NATIVE_ABI_ID "{abi}")\n')
                if '-B' in command:
                    resource_workspace.provider.mkdir(Path(command[command.index('-B') + 1]), parents=True)
                return subprocess.CompletedProcess(command, 0, stdout='')

            output = io.StringIO()
            argv = ['layout_install.py', '--build', str(build), '--lvui-build', str(lvui), '--cmake', 'cmake']
            with patch.object(sys, 'argv', argv), patch.object(layout_install.subprocess, 'run', side_effect=fake_run), \
                 contextlib.redirect_stdout(output):
                layout_install.main()
            self.assertTrue(json.loads(output.getvalue())['temporary_stage_removed_on_exit'])
            self.assertTrue(json.loads(output.getvalue())['wrapped_text_consumer_compiled_linked_executed'])
            self.assertEqual(len(calls), 5)
            configure=next(command for command in calls if '-S' in command)
            for package in ('Qt5','Qt6','LVRS'):
                self.assertIn(f'-DCMAKE_DISABLE_FIND_PACKAGE_{package}=ON',configure)
            self.assertFalse((build / 'native-layout-installed-consumer.json').exists())
            self.assertFalse((build / 'layout-consumer').exists())
            self.assertEqual((build/'install_manifest.txt').read_text(),'preserve')
            self.assertEqual((lvui/'install_manifest.txt').read_text(),'preserve')


if __name__ == "__main__":
    unittest.main()
