"""Check the UIKit scene-to-widget layout contract without launching an app."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
HOST = (ROOT / "src/Platform/HostIOS.mm").read_text(encoding="utf-8")
WIDGETS = (ROOT / "src/Widgets/NativeWidgetsIOS.mm").read_text(encoding="utf-8")
OPEN = HOST[HOST.index("void open(UIWindowScene* scene)"):HOST.index("void cleanup()")]
RESIZE = OPEN[OPEN.index("controller->resize="):OPEN.index("controller->frame=")]


class FullSceneLayout(unittest.TestCase):
    def test_scene_window_respects_its_own_bounds(self):
        self.assertRegex(HOST, r"available\s*=\s*scene\.coordinateSpace\.bounds")
        self.assertRegex(HOST, r"window\.frame\s*=\s*available")

    def test_initial_content_fills_controller(self):
        self.assertRegex(OPEN, r"initWithFrame\s*:\s*controller\.view\.bounds")
        self.assertRegex(OPEN, r"content\.autoresizingMask\s*=\s*UIViewAutoresizingFlexibleWidth\s*\|\s*UIViewAutoresizingFlexibleHeight")

    def test_resize_keeps_the_entire_controller_bounds(self):
        self.assertRegex(RESIZE, r"content\.frame\s*=\s*controller\.view\.bounds\s*;")
        self.assertNotRegex(RESIZE, r"safeArea(?:Insets|LayoutGuide)")
        self.assertIn("update_snapshot()", RESIZE)
        self.assertIn("callbacks.resized(state.size)", RESIZE)

    def test_native_widget_surface_follows_content(self):
        self.assertRegex(WIDGETS, r"initWithFrame\s*:\s*parent\.bounds")
        self.assertRegex(WIDGETS, r"view\.autoresizingMask\s*=\s*UIViewAutoresizingFlexibleWidth\s*\|\s*UIViewAutoresizingFlexibleHeight")
        self.assertRegex(HOST, r"auto\s+r\s*=\s*content\.bounds\s*;\s*state\.size\s*=\s*\{\s*r\.size\.width\s*,\s*r\.size\.height")


if __name__ == "__main__":
    unittest.main()
