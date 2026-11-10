#include <iiGUI/Application.h>
#include <stdexcept>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#include "DeferredDriverStop.h"
#include <dispatch/dispatch.h>
#include <cstdio>
#if !defined(IISDKCORE_APPLICATION_DRIVER_STOPPED_API) || IISDKCORE_APPLICATION_DRIVER_STOPPED_API < 1
#error "UIKit application lifecycle requires iiSDKCore driver completion support"
#endif
#endif
#endif

namespace ii::gui {
core::Result<std::shared_ptr<Host>> attach_application(runtime::Application& application,
        HostOptions options, HostCallbacks callbacks) {
    options.event_loop=&application.event_loop();
    auto host=std::make_shared<Host>(std::move(options));
    auto installed=application.set_driver({
        [host,callbacks=std::move(callbacks),owner=&application](const runtime::ApplicationStart& start) {
            (void)owner;
            auto native=callbacks;
            native.opened=[&start,opened=callbacks.opened] {
                if (auto result=start(); !result) {
                    if (result.error().cause) std::rethrow_exception(result.error().cause);
                    throw std::runtime_error(result.error().message);
                }
                if (opened) opened();
            };
#if defined(__APPLE__) && TARGET_OS_IPHONE
            auto completion=std::make_shared<detail::DeferredDriverStop>([owner] {
                auto result=owner->driver_stopped();
                if (!result) {
                    if (result.error().cause) std::rethrow_exception(result.error().cause);
                    throw std::runtime_error(result.error().message);
                }
            });
            struct CancelCompletion final {
                std::shared_ptr<detail::DeferredDriverStop> value;
                ~CancelCompletion() { value->cancel(); }
            } cancel{completion};
            native.closed=[completion,closed=callbacks.closed] {
                detail::notify_closed_then_schedule([&] { if (closed) closed(); }, [&] {
                    auto work=std::make_unique<detail::DeferredDriverStop::Task>(completion->delivery());
                    dispatch_async_f(dispatch_get_main_queue(),work.release(),[](void* pointer) {
                        std::unique_ptr<detail::DeferredDriverStop::Task> task(static_cast<detail::DeferredDriverStop::Task*>(pointer));
                        try { (*task)(); }
                        catch (const std::exception& error) { std::fprintf(stderr,"iiGUI driver completion failed: %s\n",error.what()); }
                        catch (...) { std::fputs("iiGUI driver completion failed\n",stderr); }
                    });
                });
            };
#endif
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
