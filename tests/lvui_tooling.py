"""Exercise iiGUI's build-time LVUI tool boundary without launching a GUI."""
import argparse
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class BuildToolTests(unittest.TestCase):
    def test_aot_and_failed_recompile_preserve_existing_output(self):
        source = ROOT / "tests/lvui_tooling"
        with tempfile.TemporaryDirectory(prefix="iigui lvui ", dir=args.build_root) as temporary:
            build = Path(temporary)
            subprocess.run([args.cmake, "-S", str(source), "-B", str(build),
                            f"-DIIGUI_SOURCE_ROOT={ROOT}", f"-DIIGUI_TEST_COMPILER={args.compiler}",
                            f"-DIIGUI_TEST_HEADERS={args.iigui_include}", f"-DIIGUI_CORE_HEADERS={args.core_include}"],
                           check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            subprocess.run([args.cmake, "--build", str(build)], check=True,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            output = build / "Example.generated.h"
            before = output.read_bytes()
            self.assertIn(b"ii::gui::WidgetBuilder", before)
            self.assertNotIn(b"parse_view", before)
            self.assertNotIn(b"#include <iiGUI/Host.h>", before)
            self.assertNotIn(b"ii::gui::Host&", before)
            runner = ROOT / "cmake/iiGUIRunLvui.cmake"
            command = [args.cmake, f"-DIIGUI_LVUI_COMPILER={args.compiler}",
                       f"-DIIGUI_LVUI_OUTPUT={output}", "-DIIGUI_LVUI_NAME=Example"]
            mtime = output.stat().st_mtime_ns
            subprocess.run(command + [f"-DIIGUI_LVUI_SOURCE={source / 'Hello.lvui'}", "-P", str(runner)],
                           check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            self.assertEqual(output.stat().st_mtime_ns, mtime)
            failure = subprocess.run(command + [f"-DIIGUI_LVUI_SOURCE={source / 'Invalid.lvui'}", "-P", str(runner)],
                                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            self.assertNotEqual(failure.returncode, 0)
            self.assertEqual(output.read_bytes(), before)
            missing = [args.cmake, f"-DIIGUI_LVUI_COMPILER={build / 'missing-compiler'}",
                       f"-DIIGUI_LVUI_OUTPUT={output}", "-DIIGUI_LVUI_NAME=Example",
                       f"-DIIGUI_LVUI_SOURCE={source / 'Hello.lvui'}", "-P", str(runner)]
            failure = subprocess.run(missing, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            self.assertNotEqual(failure.returncode, 0)
            self.assertEqual(output.read_bytes(), before)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--cmake", required=True)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--build-root", type=Path, required=True)
    parser.add_argument("--iigui-include", type=Path, required=True)
    parser.add_argument("--core-include", type=Path, required=True)
    args, remaining = parser.parse_known_args()
    unittest.main(argv=[__file__, *remaining])
