#include <iiGUI/Application.h>
#include <atomic>
#include <iostream>
#include <stdexcept>

using namespace ii;
namespace {
void check(bool value,const char* why) { if (!value) throw std::runtime_error(why); }
template<class T> T take(core::Result<T> result) {
    if (!result) { if (result.error().cause) std::rethrow_exception(result.error().cause); throw std::runtime_error(result.error().message); }
    return std::move(*result);
}
void good(core::Result<void> result) { if (!result) throw std::runtime_error(result.error().message); }
// The SDK GUI contract is also directly usable by ordinary C++ widgets.
struct CppWidget final {
    gui::WidgetMount native;
    int attached=0,detached=0;
    core::Result<void> attach(gui::Host& host) {
        auto result=host.mount_widgets([](gui::WidgetBuilder& b) -> core::Result<void> {
            auto widget=b.label({},"cpp",{.text="Native C++ widget"});
            if (!widget) return std::unexpected(widget.error());
            return {};
        },gui::lvrs_appearance());
        if (!result) return std::unexpected(result.error());
        native=*result; ++attached; return {};
    }
    core::Result<void> detach(gui::Host& host) {
        ++detached;
        if (native.snapshot()) return host.unmount_widgets(native);
        return {};
    }
};
void shared_loop() {
    runtime::Application app;
    auto widget=std::make_shared<CppWidget>();
    bool closed=false,delivered=false;
    std::shared_ptr<gui::Host> host;
    auto ui=app.event_loop().sender();
    host=take(gui::attach_application(app,{}, {.opened=[&] {
        check(&take(host->event_loop()).get()==&app.event_loop(),"iiGUI created another app event loop");
        check(widget->attached==1,"Core did not activate C++ widget before opened callback");
        check(bool(app.post([&] {
            auto wrong=app.quit();
            check(!wrong && wrong.error().code==core::ErrorCode::wrong_executor,"Worker closed native app directly");
            check(bool(ui.post([&] {
                auto label=take(widget->native.find("cpp"));
                good(label.set_text("Core result: 42"));
                check(take(label.snapshot()).text=="Core result: 42","Core delivery did not update C++ native widget");
                delivered=true; good(app.quit(4));
            })),"Core UI delivery rejected");
        })),"Core worker rejected");
    },.closed=[&] { closed=true; }}));
    auto duplicate=gui::attach_application(app);
    check(!duplicate && app.event_loop().state()!=runtime::LoopState::stopped,"Rejected driver stopped the borrowed app loop");
    good(app.attach(gui::view_attachment("cpp-widget",host,widget)));
    auto result=app.exec();
    check(result && *result==4 && closed && delivered && widget->attached==1 && widget->detached==1,"Core GUI lifecycle incomplete");
    check(ui.post([]{}).error==runtime::SubmitError::stopped,"Core app accepted delivery after teardown");
}
void failed_attachment() {
    runtime::Application app;
    auto host=take(gui::attach_application(app));
    int detached=0;
    good(app.attach({"failed-native-view",[&](auto&) -> core::Result<void> {
        return core::fail(core::ErrorCode::invalid_argument,"native activation rejected");
    },[&]() -> core::Result<void> { ++detached; return {}; }}));
    auto result=app.exec();
    check(!result && result.error().message=="native activation rejected" && detached==1,"Core did not preserve activation failure and unwind");
    check(take(host->snapshot()).closed,"Failed native attachment left its window open");
}
void callback_failure() {
    runtime::Application app;
    auto host=take(gui::attach_application(app,{}, {.opened=[] { throw std::runtime_error("GUI callback failure"); }}));
    auto widget=std::make_shared<CppWidget>();
    good(app.attach(gui::view_attachment("cpp-widget",host,widget)));
    auto result=app.exec();
    check(!result && result.error().cause && widget->detached==1,"Native exception did not return through Core teardown");
    check(take(host->snapshot()).closed,"Native callback failure leaked a window");
}
}
int main(int argc,char** argv) {
    try {
        std::string_view name=argc>1?argv[1]:"shared_loop";
        if (name=="shared_loop") shared_loop(); else if (name=="failed_attachment") failed_attachment();
        else if (name=="callback_failure") callback_failure(); else throw std::runtime_error("Unknown case");
        std::cout<<"CORE_GUI_ATTACHMENT_OK "<<name<<'\n';
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
