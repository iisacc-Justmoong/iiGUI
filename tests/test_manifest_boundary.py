"""Regression tests for the lower-level SDK and application composition boundary."""
from copy import deepcopy
from pathlib import Path
import unittest

from manifest_boundary import hello_manifest_violations

ROOT = Path(__file__).resolve().parents[1]
MANIFEST_DIR = ROOT / "examples/HelloGUI"


def sample():
    return {
        "app": {"name": "HelloGUI", "source": ".", "depends": ["iiGUI", "LVUI"]},
        "sdks": [
            {"name": "iiSDKCore", "source": "../../../iiSDKCore"},
            {"name": "LVUI", "source": "../../../LVUI"},
            {"name": "iiGUI", "source": "../.."},
        ],
    }


class ManifestBoundaryTests(unittest.TestCase):
    def errors(self, manifest):
        return hello_manifest_violations(manifest, MANIFEST_DIR, ROOT)

    def test_app_can_compose_iigui_and_lvui(self):
        self.assertEqual(self.errors(sample()), [])
        value = sample()
        value["app"]["depends"] = ["iiGUI"]
        value["sdks"] = [sdk for sdk in value["sdks"] if sdk["name"] != "LVUI"]
        self.assertEqual(self.errors(value), [])

    def test_sdk_to_sdk_dependency_is_rejected(self):
        for index, dependency in ((2, "LVUI"), (0, "iiGUI"), (1, "iiSDKCore")):
            with self.subTest(index=index, dependency=dependency):
                value = sample()
                value["sdks"][index]["depends"] = [dependency]
                self.assertTrue(self.errors(value))

    def test_duplicate_missing_or_misdirected_iigui_is_rejected(self):
        variants = []
        value = sample()
        value["sdks"].append(deepcopy(value["sdks"][2]))
        variants.append(value)
        value = sample()
        value["sdks"].pop()
        variants.append(value)
        value = sample()
        value["sdks"][2]["source"] = "../../../LVUI"
        variants.append(value)
        for value in variants:
            with self.subTest(value=value):
                self.assertTrue(self.errors(value))

    def test_unknown_app_sdk_and_invalid_dependency_shape_are_rejected(self):
        value = sample()
        value["app"]["depends"].append("UnknownSDK")
        self.assertTrue(self.errors(value))
        value = sample()
        value["sdks"][2]["depends"] = "LVUI"
        self.assertTrue(self.errors(value))
        value = sample()
        value["app"]["depends"] = ["LVUI"]
        self.assertTrue(self.errors(value))

    def test_qt_qml_and_lvrs_sdk_selections_are_rejected(self):
        for name in ("Qt", "Qt6Core", "Qt5Widgets", "Qml", "LVRS"):
            with self.subTest(name=name):
                value = sample()
                value["sdks"].append({"name": name, "source": "ignored"})
                value["app"]["depends"].append(name)
                self.assertTrue(self.errors(value))


if __name__ == "__main__":
    unittest.main()
