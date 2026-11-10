"""Safety and negative controls for the independent install-consumer check."""
from pathlib import Path
import unittest

from independent_consumer import forbidden_direct_library, redirect_install_manifest, safe_install_dir


class IndependentConsumerTests(unittest.TestCase):
    def test_install_directories_stay_below_the_test_prefix(self):
        for value in ("lib", "lib64/sdk", "include/sdk"):
            with self.subTest(value=value):
                self.assertEqual(safe_install_dir(value), Path(value))
        for value in ("", "/tmp/escape", "../escape", "lib/../../escape", "C:/escape", "C:escape", "\\escape"):
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    safe_install_dir(value)

    def test_install_manifest_is_redirected_without_touching_the_real_build(self):
        build = Path("/workspace/iiGUI/build")
        temp = Path("/workspace/iiGUI/resources/.test")
        source = ('file(WRITE "/workspace/iiGUI/build/install_local_manifest.txt" "x")\n'
                  'file(WRITE "/workspace/iiGUI/build/${CMAKE_INSTALL_MANIFEST}" "x")\n')
        changed = redirect_install_manifest(source, build, temp)
        self.assertNotIn('/workspace/iiGUI/build/install_local_manifest.txt', changed)
        self.assertIn('/workspace/iiGUI/resources/.test/install_local_manifest.txt', changed)
        self.assertIn('/workspace/iiGUI/resources/.test/${CMAKE_INSTALL_MANIFEST}', changed)
        with self.assertRaises(ValueError):
            redirect_install_manifest(source + source, build, temp)
        with self.assertRaises(ValueError):
            redirect_install_manifest("file(WRITE /elsewhere)", build, temp)

    def test_direct_link_checker_distinguishes_qt_lvui_and_system_libraries(self):
        for name in ("@rpath/libQt6Core.dylib", "/opt/QtGui.framework/Versions/A/QtGui",
                     "C:/sdk/LVUI.dll", "libLVRS.so", "libQml.so"):
            with self.subTest(name=name):
                self.assertTrue(forbidden_direct_library(name))
        for name in ("/System/Library/Frameworks/QuartzCore.framework/Versions/A/QuartzCore",
                     "@rpath/libiiSDKCore.dylib", "/usr/lib/libc++.1.dylib"):
            with self.subTest(name=name):
                self.assertFalse(forbidden_direct_library(name))


if __name__ == "__main__":
    unittest.main()
