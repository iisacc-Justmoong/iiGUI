"""Build-host regression tests for the native AOT desktop delivery contract."""
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
spec = importlib.util.spec_from_file_location("verify_aot_desktop", ROOT / "tools/verify_aot_desktop.py")
verify = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verify)


class AotDesktop(unittest.TestCase):
    def test_same_source_and_complete_dependency_graph(self):
        for platform in ("windows", "linux", "macos"):
            manifest = verify.make_manifest(ROOT, ROOT / "build/test-aot", platform, Path("/host/lvui-compile"))
            self.assertEqual(manifest["app"]["source"], str(ROOT / "examples/HelloWorld"))
            self.assertEqual(manifest["app"]["targets"], ["HelloWorld"])
            packages = {entry["name"]: entry for entry in manifest["sdks"]}
            self.assertEqual(set(packages), {"iiSDKCore", "iiColorManager", "LVUI", "iiGUI"})
            self.assertEqual(set(packages["LVUI"]["depends"]), {"iiSDKCore", "iiColorManager"})
            self.assertEqual(set(packages["iiGUI"]["depends"]), {"iiSDKCore"})
            self.assertEqual(set(manifest["app"]["depends"]), {"LVUI", "iiGUI"})
            self.assertEqual(manifest["kits"][0]["platform"], platform)
            self.assertEqual(manifest["kits"][0]["cache"]["LVUI_HOST_COMPILER"], "/host/lvui-compile")
            self.assertEqual(manifest["kits"][0]["cache"]["CMAKE_DISABLE_FIND_PACKAGE_Qt6"], "ON")

    def test_execution_is_never_implicitly_enabled(self):
        manifest = verify.make_manifest(ROOT, ROOT / "build/test-aot", "windows", Path("/host/lvui-compile"))
        self.assertNotIn("run", manifest)
        for package in manifest["sdks"]:
            self.assertEqual(package["cache"]["BUILD_TESTING"], "OFF")

    def test_target_toolchain_and_overrides_are_preserved(self):
        manifest = verify.make_manifest(ROOT, ROOT / "build/test-aot", "linux", Path("/host/lvui-compile"),
                                        Path("/target/toolchain.cmake"), {"Vulkan_LIBRARY": "/target/libvulkan.so"})
        self.assertEqual(manifest["kits"][0]["toolchain"], "/target/toolchain.cmake")
        self.assertEqual(manifest["kits"][0]["cache"]["Vulkan_LIBRARY"], "/target/libvulkan.so")

    def test_rejects_invalid_or_console_windows_image(self):
        image = bytearray(512)
        image[:2] = b"MZ"
        struct.pack_into("<I", image, 0x3c, 128)
        image[128:132] = b"PE\0\0"
        struct.pack_into("<H", image, 132, 0x8664)
        struct.pack_into("<H", image, 152, 0x20b)
        struct.pack_into("<H", image, 152 + 68, 2)
        self.assertEqual(verify.native_image(bytes(image), "windows")["architecture"], "x86_64")
        struct.pack_into("<H", image, 152 + 68, 3)
        with self.assertRaises(ValueError):
            verify.native_image(bytes(image), "windows")
        with self.assertRaises(ValueError):
            verify.native_image(b"MZ", "windows")

    def test_rejects_foreign_or_non_executable_linux_image(self):
        image = bytearray(64)
        image[:6] = b"\x7fELF\x02\x01"
        struct.pack_into("<HH", image, 16, 3, 183)
        self.assertEqual(verify.native_image(bytes(image), "linux")["architecture"], "aarch64")
        struct.pack_into("<H", image, 16, 1)
        with self.assertRaises(ValueError):
            verify.native_image(bytes(image), "linux")
        with self.assertRaises(ValueError):
            verify.native_image(bytes(image), "windows")

    def test_rejects_framework_interpreter_and_compiler_dependencies(self):
        symbols = "ii::runtime::Application::exec()\nii::gui::Host::mount_widgets()\nmain"
        verify.check_linkage("Name: USER32.dll", symbols)
        for libraries in ("Name: Qt6Core.dll", "libQt6Gui.so.6", "python314.dll", "libjvm.so"):
            with self.assertRaises(ValueError):
                verify.check_linkage(libraries, symbols)
        for extra in ("lvui::parse_view(", "lvui::compile_native(", "lvui::parse("):
            with self.assertRaises(ValueError):
                verify.check_linkage("", symbols + extra)
        with self.assertRaises(ValueError):
            verify.check_linkage("", "main")

    def test_exact_font_resources_are_required(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as temporary:
            root = Path(temporary)
            source = root / "prefix/share/iiGUI/resources/native"
            destination = root / "app/resources"
            source.mkdir(parents=True)
            destination.mkdir(parents=True)
            for name in verify.RESOURCES:
                (source / name).write_bytes(name.encode())
                (destination / name).write_bytes(name.encode())
            self.assertEqual(len(verify.check_resources(root / "prefix", root / "app", "linux")), len(verify.RESOURCES))
            (destination / "Pretendard-Regular.ttf").write_bytes(b"replaced")
            with self.assertRaises(ValueError):
                verify.check_resources(root / "prefix", root / "app", "linux")

    def test_delivery_manifest_is_portable(self):
        text = (ROOT / "examples/HelloWorld/iiSDKBuilder.json").read_text()
        data = json.loads(text)
        self.assertEqual({kit["platform"] for kit in data["kits"]}, {"windows", "linux", "macos"})
        self.assertNotIn("/Users/", text)
        self.assertNotIn("/Volumes/", text)


if __name__ == "__main__":
    unittest.main()
