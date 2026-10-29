#include <iiGUI/Application.h>
#include <stdexcept>

namespace ii::gui {
core::Result<std::shared_ptr<Host>> attach_application(runtime::Application& application,
        HostOptions options, HostCallbacks callbacks) {
    options.event_loop=&application.event_loop();
    auto host=std::make_shared<Host>(std::move(options));
    auto installed=application.set_driver({
        [host,callbacks=std::move(callbacks)](const runtime::ApplicationStart& start) {
            auto native=callbacks;
            native.opened=[&start,opened=callbacks.opened] {
                if (auto result=start(); !result) {
                    if (result.error().cause) std::rethrow_exception(result.error().cause);
                    throw std::runtime_error(result.error().message);
                }
                if (opened) opened();
            };
            return host->run(std::move(native));
        },
        [weak=std::weak_ptr<Host>(host)]() -> core::Result<void> {
            auto target=weak.lock();
            if (!target) return core::fail(core::ErrorCode::closed,"Native application host expired");
            return target->request_close();
        }});
    if (!installed) return std::unexpected(installed.error());
    return host;
}
} // namespace ii::gui
