"""Verify host-consumer workspace cleanup without executing a GUI or device."""
import contextlib
import errno
import importlib.util
import io
import json
from pathlib import Path
import sys
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import resource_workspace

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
spec = importlib.util.spec_from_file_location("verify_host_consumer", ROOT / "tools/verify_host_consumer.py")
verify = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verify)


class HostConsumerWorkspaceTests(unittest.TestCase):
    def test_cleanup_retries_a_late_provider_sidecar_but_not_other_errors(self):
        with resource_workspace.temporary(ROOT, "host-test-") as fixture:
            owned = fixture / "owned"
            resource_workspace.provider.mkdir(owned)
            original = verify.verification_workspace.shutil.rmtree
            with patch.object(verify.verification_workspace.shutil, "rmtree",
                              side_effect=[OSError(errno.ENOTEMPTY, "late sidecar"), None]) as remove:
                verify.verification_workspace.remove_owned_tree(owned)
                self.assertEqual(remove.call_count, 2)
            with patch.object(verify.verification_workspace.shutil, "rmtree",
                              side_effect=OSError(errno.EACCES, "denied")):
                with self.assertRaises(PermissionError):
                    verify.verification_workspace.remove_owned_tree(owned)
            original(owned)

    def test_consumer_inputs_and_outputs_use_owned_separate_roots(self):
        with verify.verification_workspace.temporary(ROOT, "host-") as workspace:
            output = ROOT / "build/host-test"
            paths = verify.consumer_paths(output, workspace)
            for name in ("prefix", "example", "contract", "install"):
                self.assertTrue(paths[name].is_relative_to(workspace))
            for name in ("example_build", "contract_build"):
                self.assertTrue(paths[name].is_relative_to(output))
        self.assertFalse(workspace.exists())

    def test_failure_does_not_leave_build_or_resource_workspace(self):
        provider = resource_workspace.provider
        with resource_workspace.temporary(ROOT, "host-test-") as fixture:
            prefix = fixture / "prefix"
            config = prefix / "lib/cmake/iiGUI/iiGUIConfig.cmake"
            provider.mkdir(config.parent, parents=True)
            provider.write_text(config, "fixture")
            examples = prefix / "share/iiGUI/examples/HelloHosts"
            provider.mkdir(examples, parents=True)
            provider.write_text(examples / "CMakeLists.txt", "fixture")
            compiler = fixture / "lvui-compile"
            provider.write_text(compiler, "fixture")
            output = ROOT / "build" / ("host-test-" + fixture.name)
            with patch.object(verify.subprocess, "run", return_value=SimpleNamespace(returncode=1)), \
                 contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(RuntimeError):
                    verify.main(["--prefix", str(prefix), "--backend", "cocoa", "--build-root", str(output),
                                 "--host-compiler", str(compiler)])
            self.assertFalse(output.exists())

    def test_success_reports_native_artifact_then_removes_temporary_build(self):
        provider = resource_workspace.provider
        with resource_workspace.temporary(ROOT, "host-test-") as fixture:
            prefix = fixture / "prefix"
            config = prefix / "lib/cmake/iiGUI/iiGUIConfig.cmake"
            provider.mkdir(config.parent, parents=True)
            provider.write_text(config, "fixture")
            examples = prefix / "share/iiGUI/examples/HelloHosts"
            provider.mkdir(examples, parents=True)
            provider.write_text(examples / "CMakeLists.txt", "fixture")
            compiler = fixture / "lvui-compile"
            provider.write_text(compiler, "fixture")
            output = ROOT / "build" / ("host-test-" + fixture.name)
            def success(command, **kwargs):
                if '-B' in command:
                    build = Path(command[command.index('-B') + 1])
                    provider.mkdir(build, parents=True, exist_ok=True)
                    provider.write_text(build/'compile_commands.json', '[]')
                if '--build' in command and '--target' in command:
                    build = Path(command[command.index('--build') + 1])
                    provider.write_bytes(build/'HelloHosts.wasm', bytes.fromhex('0061736d01000000'))
                return SimpleNamespace(returncode=0)
            text = io.StringIO()
            with patch.object(verify.subprocess, "run", side_effect=success), contextlib.redirect_stdout(text):
                self.assertEqual(verify.main(["--prefix",str(prefix),"--backend","web","--build-root",str(output),
                                              "--host-compiler",str(compiler)]),0)
            report = json.loads(text.getvalue())
            self.assertEqual(report['status'], 'installed_consumers_built')
            self.assertFalse(report['guiLaunched'])
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
