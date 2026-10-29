"""The runtime evidence validator must reject partial runs and failed exits."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("web_runtime", Path(__file__).resolve().parents[1] / "tools/verify_web_runtime.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def completed(frames=59):
    return [{"level": "log", "message": message} for message in (
        "HOST_SMOKE_READY backend=6 qt=false aot=true",
        f"HOST_SMOKE_OK backend=6 timer=true background=42 resize=true frames={frames} synced=1 capture=true",
        "HOST_SMOKE_CLOSED")]


class RuntimeEvidenceTest(unittest.TestCase):
    def test_complete_run(self):
        self.assertTrue(module.verify(completed())["normalCloseObserved"])

    def test_async_exit_error_is_failure_even_after_closed(self):
        logs = completed() + [{"level": "error", "message": "Uncaught unwind"}]
        with self.assertRaisesRegex(ValueError, "Browser runtime errors"):
            module.verify(logs)

    def test_missing_close_is_not_success(self):
        with self.assertRaisesRegex(ValueError, "ordered"):
            module.verify(completed()[:-1])

    def test_starved_frames_are_not_success(self):
        with self.assertRaisesRegex(ValueError, "every host check"):
            module.verify(completed(frames=0))


if __name__ == "__main__":
    unittest.main()
