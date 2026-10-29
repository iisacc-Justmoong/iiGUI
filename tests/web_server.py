"""Exercise real HTTP response headers, isolation and Wasm MIME delivery."""
import importlib.util
from http.server import ThreadingHTTPServer
from pathlib import Path
import threading
import unittest
import urllib.request
from functools import partial

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("iigui_web_server", root / "tools/serve_web.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class WebServerTest(unittest.TestCase):
    def test_cross_origin_isolation_headers(self):
        server = ThreadingHTTPServer(("127.0.0.1", 0), partial(module.IsolatedHandler, directory=str(root / "examples/HelloHosts")))
        thread = threading.Thread(target=server.serve_forever)
        thread.start()
        try:
            with urllib.request.urlopen(f"http://127.0.0.1:{server.server_port}/shell.html") as response:
                self.assertEqual(response.headers["Cross-Origin-Opener-Policy"], "same-origin")
                self.assertEqual(response.headers["Cross-Origin-Embedder-Policy"], "require-corp")
                self.assertEqual(response.headers["Cross-Origin-Resource-Policy"], "same-origin")
                self.assertEqual(response.headers["Cache-Control"], "no-store")
                self.assertIn(b"canvas", response.read())
        finally:
            server.shutdown()
            thread.join()
            server.server_close()

    def test_wasm_mime(self):
        import mimetypes
        self.assertEqual(mimetypes.guess_type("HelloHosts.wasm")[0], "application/wasm")


if __name__ == "__main__":
    unittest.main()
