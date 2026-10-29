# HelloHosts

The same LVUI view and C++23 application exercise iiGUI's host, Core timer,
persistent background pool, display frames, resize, capture and orderly shutdown.
Build with iiSDKBuilder. `--self-test` (browser query `?self-test`) runs the bounded
five-second lifecycle check. The Web target is AOT WebAssembly with a pthread
application and an OffscreenCanvas/WebGL2 renderer; it requires cross-origin
isolation and a secure browser context. No LVUI source interpreter or DOM widget
tree is deployed. See `docs/HostPlatforms.md` for platform contracts and limits.
