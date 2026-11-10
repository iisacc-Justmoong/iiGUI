"""The verifier must never mistake a foreign binary build for execution."""
import importlib.util
import contextlib
import io
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace
import unittest
from unittest.mock import patch

root = Path(sys.argv.pop(1)).resolve()
spec = importlib.util.spec_from_file_location('verify_desktop', root/'tools/verify_desktop.py')
verify = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verify)


class VerifierTests(unittest.TestCase):
    def test_manual_verification_state_is_disposable_resource_data(self):
        with verify.verification_workspace.temporary(root, 'desktop-') as workspace:
            self.assertTrue(workspace.is_relative_to(root/'resources/.state'))
            self.assertTrue(workspace.is_dir())
        self.assertFalse(workspace.exists())
        with self.assertRaisesRegex(RuntimeError, 'intentional'):
            with verify.verification_workspace.temporary(root, 'desktop-') as failed:
                raise RuntimeError('intentional')
        self.assertFalse(failed.exists())

    def test_compile_only_verification_separates_build_and_resource_state(self):
        import resource_workspace
        calls = []
        def fake_run(command, **kwargs):
            calls.append([str(value) for value in command])
            return SimpleNamespace(returncode=0)

        with resource_workspace.temporary(root, 'tooling-') as owner:
            fake_build = owner/'build'
            with patch.object(verify, 'checked_build_root', return_value=fake_build), \
                 patch.object(verify.subprocess, 'run', side_effect=fake_run), \
                 contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(verify.main(['--platform', verify.host_platform(), '--compile-only', '--builder', 'fake-builder']), 0)
            manifest = Path(calls[0][calls[0].index('--manifest') + 1])
            self.assertTrue(manifest.is_relative_to(root/'resources/.state/verification'))
            self.assertFalse(manifest.exists())
            package = next(command for command in calls if command[-1].endswith('tests/package.cmake'))
            build_option = next(value for value in package if value.startswith('-DBUILD='))
            resource_option = next(value for value in package if value.startswith('-DRESOURCE_ROOT='))
            self.assertTrue(Path(build_option.removeprefix('-DBUILD=')).is_relative_to(fake_build))
            self.assertTrue(Path(resource_option.removeprefix('-DRESOURCE_ROOT=')).is_relative_to(root/'resources/.state/verification'))

    def test_failed_verification_removes_its_resource_workspace(self):
        import resource_workspace
        calls = []
        def failed(command, **kwargs):
            calls.append([str(value) for value in command])
            return SimpleNamespace(returncode=1)

        with resource_workspace.temporary(root, 'tooling-') as owner:
            with patch.object(verify, 'checked_build_root', return_value=owner/'build'), \
                 patch.object(verify.subprocess, 'run', side_effect=failed), \
                 contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(verify.main(['--platform', verify.host_platform(), '--compile-only', '--builder', 'fake-builder']), 1)
            manifest = Path(calls[0][calls[0].index('--manifest') + 1])
            self.assertFalse(manifest.exists())

    def test_package_verifier_rejects_escaped_locations(self):
        with verify.verification_workspace.temporary(root, 'package-') as workspace:
            common = ['cmake', '-DSOURCE='+str(root), '-DCORE_BUILD=missing', '-DLVUI_BUILD=missing',
                      '-DGUI_BUILD=missing', '-P', str(root/'tests/package.cmake')]
            for build, resources in ((root/'outside-build', workspace), (root/'build/fixture', root/'resources')):
                command = common[:1] + ['-DBUILD='+str(build), '-DRESOURCE_ROOT='+str(resources)] + common[1:]
                result = subprocess.run(command, text=True, capture_output=True, check=False)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('owned build/ and resource-state', result.stderr)

    def test_package_verifier_accepts_owned_locations_before_install(self):
        with verify.verification_workspace.temporary(root, 'package-') as workspace:
            build = root/'build'/'package-verifier-tests'/workspace.name
            command = ['cmake', '-DSOURCE='+str(root), '-DBUILD='+str(build),
                       '-DRESOURCE_ROOT='+str(workspace), '-DCORE_BUILD='+str(workspace/'missing'),
                       '-DLVUI_BUILD=missing', '-DGUI_BUILD=missing', '-P', str(root/'tests/package.cmake')]
            result = subprocess.run(command, text=True, capture_output=True, check=False)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Install failed', result.stderr)
            self.assertFalse(build.exists())

    def test_test_resources_are_isolated_and_cleaned_on_success_and_failure(self):
        import resource_workspace
        with resource_workspace.temporary(root, 'tooling-') as workspace:
            self.assertTrue(workspace.is_relative_to(root/'resources/.state'))
            self.assertTrue(workspace.is_dir())
        self.assertFalse(workspace.exists())
        with self.assertRaisesRegex(RuntimeError, 'intentional'):
            with resource_workspace.temporary(root, 'tooling-') as failed:
                self.assertNotEqual(workspace, failed)
                raise RuntimeError('intentional')
        self.assertFalse(failed.exists())
        for prefix in ('../escape-', '/escape-', 'dir\\escape-', ''):
            with self.assertRaises(ValueError):
                with resource_workspace.temporary(root, prefix):
                    self.fail('Unsafe prefix was accepted')

    def test_cross_package_search_cannot_inherit_host_pkgconfig(self):
        original = {'CPATH': '/host/include', 'PKG_CONFIG_PATH': '/host/pkgconfig', 'PATH': '/tools'}
        environment = verify.build_environment(Path('/target/toolchain.cmake'), original)
        self.assertNotIn('CPATH', environment)
        self.assertNotIn('PKG_CONFIG_PATH', environment)
        self.assertEqual(environment['PATH'], '/tools')
        self.assertIn('PKG_CONFIG_PATH', verify.build_environment(None, original))
        self.assertIn('CPATH', original)
        manifest=verify.make_manifest(root,root/'build/test-manifest','linux',Path('/target/toolchain.cmake'))
        self.assertEqual(manifest['kits'][0]['environment']['PKG_CONFIG_PATH'], '')

    def test_destination_cannot_escape_build(self):
        for path in (root, root/'docs', root/'build/../src'):
            with self.assertRaises(ValueError):
                verify.checked_build_root(root, path)
        self.assertEqual(verify.checked_build_root(root, root/'build/tests'), root/'build/tests')

    def test_native_loader_cannot_hide_missing_package_runtime(self):
        overrides=('LD_LIBRARY_PATH','DYLD_LIBRARY_PATH','DYLD_FALLBACK_LIBRARY_PATH',
                   'DYLD_FRAMEWORK_PATH','DYLD_FALLBACK_FRAMEWORK_PATH')
        original={key:'/hidden/runtime' for key in overrides}
        original['PATH']='/tools'
        for toolchain in (None,Path('/target/toolchain.cmake')):
            clean=verify.build_environment(toolchain,original)
            self.assertFalse(set(overrides) & clean.keys())
            self.assertEqual(clean['PATH'],'/tools')
        self.assertEqual(original['DYLD_LIBRARY_PATH'],'/hidden/runtime')

    def test_foreign_execution_requires_explicit_skip(self):
        for target in ('windows', 'macos', 'linux'):
            for host in ('windows', 'macos', 'linux'):
                if target != host:
                    with self.assertRaises(ValueError):
                        verify.validate_execution(target, False, host)
                verify.validate_execution(target, True, host)
                verify.validate_execution(target, False, target)

    def test_platforms_share_application_and_sdk_sources(self):
        for platform in ('windows', 'macos', 'linux'):
            manifest = verify.make_manifest(root, root/'build/test-manifest', platform)
            self.assertEqual(len(manifest['kits']), 1)
            self.assertEqual(manifest['kits'][0]['platform'], platform)
            self.assertEqual(Path(manifest['app']['source']), root/'examples/HelloGUI')
            self.assertEqual({sdk['name'] for sdk in manifest['sdks']}, {'iiSDKCore', 'LVUI', 'iiGUI'})
            self.assertTrue(all(not sdk.get('depends') for sdk in manifest['sdks']))
            self.assertEqual(set(manifest['app']['depends']), {'iiGUI', 'LVUI'})
            self.assertEqual(manifest['kits'][0]['environment']['CPLUS_INCLUDE_PATH'], '')
            self.assertEqual(manifest['kits'][0]['cache']['CMAKE_DISABLE_FIND_PACKAGE_Qt6'], 'ON')

    def test_builder_layout_and_explicit_target_prefixes(self):
        build = root/'build/test-manifest'
        prefix = root/'resources/target-sdk'
        manifest = verify.make_manifest(root, build, 'linux', prefixes=[prefix])
        self.assertEqual(manifest['kits'][0]['prefixes'], [str(prefix.resolve())])
        paths = verify.kit_builds(build, 'linux')
        self.assertEqual(paths['iiGUI'], build/'kits/linux/Release/sdk/iiGUI/isolated/build')
        self.assertEqual(paths['LVUI'], build/'kits/linux/Release/sdk/LVUI/isolated/build')
        self.assertEqual(paths['app'], build/'kits/linux/Release/app/isolated/build')


unittest.main()
