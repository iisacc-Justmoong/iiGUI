"""The verifier must never mistake a foreign binary build for execution."""
import importlib.util
from pathlib import Path
import sys
import unittest

root = Path(sys.argv.pop(1)).resolve()
spec = importlib.util.spec_from_file_location('verify_desktop', root/'tools/verify_desktop.py')
verify = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verify)


class VerifierTests(unittest.TestCase):
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
            gui = next(sdk for sdk in manifest['sdks'] if sdk['name'] == 'iiGUI')
            self.assertEqual(set(gui['depends']), {'iiSDKCore', 'LVUI'})
            self.assertEqual(manifest['kits'][0]['environment']['CPLUS_INCLUDE_PATH'], '')
            self.assertEqual(manifest['kits'][0]['cache']['CMAKE_DISABLE_FIND_PACKAGE_Qt6'], 'ON')


unittest.main()
