#include <iiGUI/NativeWidgets.h>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace ii;
namespace {
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
void run(const std::string& name){
    runtime::EventLoop loop;gui::Host host({.event_loop=&loop});gui::WidgetMount mount;gui::Widget editor,other;
    auto provider=std::make_shared<gui::TextInputProvider>();gui::TextInputState state{.length=1000000000,.selection={12345,3},.caret={12,24,1,20}};
    unsigned reads=0,focus=0,blur=0;bool reentry_rejected=false;
    provider->state=[&]()->core::Result<gui::TextInputState>{if(name=="reentry"){auto r=editor.snapshot();reentry_rejected=!r&&r.error().code==core::ErrorCode::busy;}return state;};
    provider->read=[&](gui::TextRange r)->core::Result<gui::TextContent>{++reads;if(name=="bad_content")return gui::TextContent{r,"wrong"};return gui::TextContent{r,std::string(r.length,'a')};};
    provider->position=[](double x,double)->core::Result<std::size_t>{return std::size_t(x);};
    provider->focus_changed=[&](bool value)->core::Result<void>{value?++focus:++blur;return {};};
    if(name=="no_autofocus")state.auto_focus_on_press=false;
    good(host.run({.opened=[&]{
        mount=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{
            check(&take(b.event_loop()).get()==&loop,"Builder lost the Core loop identity");
            auto root=take(b.vstack({},"root",{.padding=20}));
            editor=take(b.absolute_stack(root,"editor",{300,160}));other=take(b.button(root,"other"));
            good(editor.set_input_handler([](const auto& e)->core::Result<bool>{return e.kind==gui::WidgetInputKind::pointer_down;}));
            return editor.set_text_input_provider(provider);
        }));
        if(name=="bounded"){auto s=take(editor.snapshot());check(s.text.empty()&&s.text_input&&s.multiline&&s.text_length==1000000000&&reads==0,"Snapshot materialized the virtual document");auto content=take(editor.text_content({500000,1000000}));check(content.range==gui::TextRange{500000,65536}&&content.text.size()==65536&&reads==1,"Native text service range was not bounded");}
        else if(name=="geometry"){auto s=take(editor.snapshot());check(s.caret_bounds.x==s.bounds.x+12&&s.caret_bounds.y==s.bounds.y+24&&take(editor.text_position(s.bounds.x+42,s.bounds.y+50))==42,"Text caret/hit lost native surface coordinates");}
        else if(name=="focus"){good(editor.focus());good(other.focus());check(focus==1&&blur==1,"Provider focus lifetime notifications changed");}
        else if(name=="no_autofocus"){auto s=take(editor.snapshot());good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=s.bounds.x+5,.y=s.bounds.y+5}));check(!take(editor.snapshot()).focused&&focus==0,"Pointer handler overrode autoFocusOnPress=false");}
        else if(name=="reentry"){take(editor.snapshot());check(reentry_rejected,"Text provider reentered native UI queries");}
        else if(name=="bad_content"){auto r=editor.text_content({10,20});check(!r&&r.error().code==core::ErrorCode::invalid_argument,"Mismatched native text range accepted");}
        else if(name=="bad_state"){state.selection={state.length,1};auto r=mount.snapshot();check(!r&&r.error().code==core::ErrorCode::invalid_argument,"Invalid provider state was unchecked during tree snapshot");state.selection={0,0};}
        else if(name=="owner"){bool rejected=false;std::thread worker([&]{auto r=editor.text_content({0,1});rejected=!r&&r.error().code==core::ErrorCode::wrong_executor;});worker.join();check(rejected,"Text provider crossed its native owner thread");}
        else if(name=="retirement"){auto old=editor;good(host.unmount_widgets());check(!old.text_content({0,1}),"Retired text provider still addressed its old view");}
        else if(name=="legacy"){good(host.unmount_widgets());mount=take(host.mount_widgets([&](auto& b)->core::Result<void>{editor=take(b.input_field({},"legacy",{.text="A한😀Z",.width=200}));return {};}));auto content=take(editor.text_content({1,3}));check(content.text=="한😀"&&take(editor.snapshot()).text_length==5&&!editor.text_content({3,1}),"Legacy input field lost the bounded UTF-16 reader");}
        else throw std::runtime_error("Unknown text input provider case");
        good(host.request_close());
    }}));
}
}
int main(int argc,char** argv){try{run(argc>1?argv[1]:"bounded");std::cout<<"TEXT_INPUT_PROVIDER_OK\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
