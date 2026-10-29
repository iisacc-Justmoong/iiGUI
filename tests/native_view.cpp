#include <iiGUI/NativeView.h>
#include <iiGUI/WidgetState.h>
#include <iostream>
#include <limits>
#include <thread>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
template<class T>T take(core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
struct Source final:gui::NativeViewSource {
    mutable int builds=0,retires=0;
    int file_revision=7;
    std::function<core::Result<void>(gui::WidgetBuilder&)> body;
    core::Result<void> build(gui::WidgetBuilder& b)const override{
        ++builds;if(auto r=b.on_retire([this]{++retires;});!r)return r;
        if(body)return body(b);
        auto root=b.vstack({},"page");if(!root)return std::unexpected(root.error());
        auto field=b.input_field(*root,"field",{.text="Draft"});if(!field)return std::unexpected(field.error());
        auto button=b.button(*root,"action",{.text="Native command"});if(!button)return std::unexpected(button.error());
        return {};
    }
};
struct Mounted final {
    std::shared_ptr<gui::detail::WidgetState> tree=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host=take(tree->content_host({},"host",{640,480}));
    gui::NativeView view,other;
    void mount(std::shared_ptr<gui::NativeViewSource> source,bool twice=false){
        good(host.set_content([&](gui::WidgetBuilder& b)->core::Result<void>{
            auto row=take(b.hstack({},"row",{.spacing=8}));
            view=take(gui::NativeView::create(b,row,"custom",{.source=source,.width=240,.height=180}));
            if(twice)other=take(gui::NativeView::create(b,row,"other",{.source=source,.width=240,.height=180}));
            return {};
        }));
    }
    gui::Widget field(){return take(view.content_item("field"));}
    ~Mounted(){tree->close();}
};
void initial(){Mounted f;auto s=std::make_shared<Source>();f.mount(s);check(s->builds==1&&take(f.field().snapshot()).text=="Draft","Native provider did not create a real native input");check(take(f.view.options()).source==s&&s->file_revision==7,"Mount mutated its document model");}
void empty(){Mounted f;f.mount({});check(f.view.root().snapshot().has_value()&&!f.view.content_item(),"Empty source did not keep an empty retained host");}
void identity(){Mounted f;auto s=std::make_shared<Source>();f.mount(s);auto field=f.field();good(field.focus());good(field.set_text("Working"));auto o=take(f.view.options());good(f.view.set_options(o));o.width=280;good(f.view.set_options(o));check(s->builds==1&&s->retires==0&&take(field.snapshot()).focused&&take(field.snapshot()).text=="Working","Equal source or layout update rebuilt native content");check(s->file_revision==7&&take(f.view.options()).width==280,"View-only layout changed file data");}
void replace(){Mounted f;auto a=std::make_shared<Source>(),b=std::make_shared<Source>();f.mount(a);auto old=f.field();good(old.set_text("Original"));good(f.view.set_source(b));check(!old.snapshot()&&a->retires==1&&b->builds==1&&take(f.field().snapshot()).text=="Draft","Source replacement did not retire exactly its old generation");}
void failed(std::string kind){Mounted f;auto a=std::make_shared<Source>(),b=std::make_shared<Source>();f.mount(a);auto old=f.field();good(old.focus());good(old.set_text("Preserved"));b->body=[&](gui::WidgetBuilder& builder)->core::Result<void>{if(kind=="empty_candidate")return {};take(builder.vstack({},"candidate"));if(kind=="exception")throw std::runtime_error("Provider fault");return core::fail(core::ErrorCode::failed,"Provider rejected candidate");};auto changed=f.view.set_source(b);check(!changed&&take(f.view.options()).source==a&&take(old.snapshot()).text=="Preserved"&&take(old.snapshot()).focused&&a->retires==0&&b->retires==1,"Invalid candidate replaced old content, focus or ownership");if(kind=="exception")check(bool(changed.error().cause),"Provider exception was lost");}
void invalid(){Mounted f;auto a=std::make_shared<Source>(),b=std::make_shared<Source>();f.mount(a);auto old=f.field();for(double width:{-1.,8193.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}){auto o=take(f.view.options());o.width=width;o.source=b;check(!f.view.set_options(o)&&old.snapshot()&&take(f.view.options()).source==a&&b->builds==0,"Invalid options reached a provider or mutated existing content");}}
void independent(){Mounted f;auto s=std::make_shared<Source>();f.mount(s,true);auto a=f.field(),b=take(f.other.content_item("field"));good(a.set_text("First"));check(s->builds==2&&take(b.snapshot()).text=="Draft","Two activations shared their input state");good(f.view.set_source(std::make_shared<Source>()));check(!a.snapshot()&&b.snapshot()&&s->retires==1,"Replacing one native view retired another");}
void clear(){Mounted f;auto s=std::make_shared<Source>();f.mount(s);auto old=f.field();good(f.view.set_source({}));check(!old.snapshot()&&!f.view.content_item()&&f.view.root().snapshot()&&s->retires==1,"Clearing native source destroyed its retained host or kept old content");good(f.view.set_source(s));check(s->builds==2&&f.field().snapshot(),"Cleared native source could not reactivate");}
void retirement(){Mounted f;auto source=std::make_shared<Source>();std::weak_ptr<Source> weak=source;f.mount(source);source.reset();check(!weak.expired(),"Native generation did not own its source");auto endpoint=f.view;good(f.host.clear_content());check(weak.expired()&&!endpoint.options()&&!endpoint.content_item(),"External endpoint retained a retired provider or remained writable");}
void foreign(){Mounted f;auto s=std::make_shared<Source>();f.mount(s);bool rejected=false;std::thread worker([&]{auto r=f.view.set_source({});rejected=!r&&r.error().code==core::ErrorCode::wrong_executor;});worker.join();check(rejected&&take(f.view.options()).source==s&&s->retires==0,"Foreign thread mutated native view state");}
void reentry(){Mounted f;auto a=std::make_shared<Source>(),b=std::make_shared<Source>();f.mount(a);bool rejected=false;b->body=[&](gui::WidgetBuilder& builder)->core::Result<void>{auto r=f.view.set_source({});rejected=!r&&r.error().code==core::ErrorCode::busy;take(builder.vstack({},"candidate"));return {};};good(f.view.set_source(b));check(rejected&&take(f.view.options()).source==b&&a->retires==1,"Provider reentry corrupted candidate ownership");}
void retire_during_build(){Mounted f;auto a=std::make_shared<Source>(),b=std::make_shared<Source>();f.mount(a);b->body=[&](gui::WidgetBuilder& builder)->core::Result<void>{take(builder.vstack({},"candidate"));f.tree->close();return {};};auto r=f.view.set_source(b);check(!r&&r.error().code==core::ErrorCode::closed&&!f.view.options()&&a->retires==1&&b->retires==1,"Retirement during provider admission published a successor");}
void nested(){Mounted f;auto inner=std::make_shared<Source>(),outer=std::make_shared<Source>();gui::NativeView child;outer->body=[&](gui::WidgetBuilder& builder)->core::Result<void>{child=take(gui::NativeView::create(builder,{},"nested",{.source=inner,.width=160,.height=120}));return {};};f.mount(outer);auto field=take(child.content_item("field"));good(field.set_text("Nested"));good(f.view.set_source({}));check(!field.snapshot()&&!child.options()&&inner->retires==1&&outer->retires==1,"Nested provider survived parent retirement");}
void drawing(){Mounted f;auto s=std::make_shared<Source>();gui::Widget canvas;int moves=0,ups=0,wheels=0;double pressure=0;
    s->body=[&](gui::WidgetBuilder& builder)->core::Result<void>{canvas=take(builder.absolute_stack({},"canvas",{240,180}));gui::WidgetDrawing ink;ink.panels.push_back({.bounds={0,0,240,180},.top={1,0,0,1},.bottom={1,0,0,1}});good(canvas.set_drawing(std::move(ink)));return canvas.set_input_handler([&](const gui::WidgetInput& e)->core::Result<bool>{if(e.kind==gui::WidgetInputKind::pointer_down)return true;if(e.kind==gui::WidgetInputKind::pointer_move){++moves;pressure=e.pressure;return true;}if(e.kind==gui::WidgetInputKind::pointer_up){++ups;return true;}if(e.kind==gui::WidgetInputKind::scroll){++wheels;return true;}return false;});};
    f.mount(s);auto frame=take(f.tree->drawing_snapshot({640,480}));bool red=false;auto inspect=[&](const auto& panels){for(const auto& p:panels)if(p.top.red==1&&p.top.green==0&&p.top.blue==0)red=true;};inspect(frame.panels);for(const auto& layer:frame.overlays)inspect(layer.panels);check(red,"Native source did not reach actual drawing data");auto bounds=take(canvas.snapshot()).bounds;good(f.tree->input({.kind=gui::WidgetInputKind::pointer_down,.x=bounds.x+4,.y=bounds.y+4}));good(f.tree->input({.kind=gui::WidgetInputKind::pointer_move,.x=620,.y=460,.device=gui::PointerDevice::pen,.pressure=.4}));good(f.tree->input({.kind=gui::WidgetInputKind::pointer_up,.x=620,.y=460}));good(f.tree->input({.kind=gui::WidgetInputKind::scroll,.x=bounds.x+4,.y=bounds.y+4,.scroll_y=20}));check(moves>0&&ups==1&&wheels==1&&pressure==.4&&s->file_revision==7,"Native drawing lost pointer capture, pen pressure, wheel or view/file separation");
    good(canvas.set_paint_handler([](const gui::WidgetPaintState&)->core::Result<std::shared_ptr<const gui::WidgetDrawing>>{auto d=std::make_shared<gui::WidgetDrawing>();d->panels.push_back({.bounds={0,0,0,10}});return std::shared_ptr<const gui::WidgetDrawing>(std::move(d));}));
    bool diagnosed=false;try{(void)take(f.tree->drawing_snapshot({640,480}));}catch(const std::runtime_error& e){std::string text=e.what();diagnosed=text.find("canvas")!=text.npos&&text.find("panels=1")!=text.npos&&text.find("texts=0")!=text.npos;}
    check(diagnosed,"Invalid drawing omitted the responsible widget or primitive counts");
    gui::WidgetDrawing recovered;recovered.panels.push_back({.bounds={0,0,20,20},.top={1,0,0,1},.bottom={1,0,0,1}});good(canvas.set_drawing(std::move(recovered)));check(f.tree->drawing_snapshot({640,480}).has_value(),"Invalid drawing left the native paint admission closed");

}
}
void frame_data(){
    Mounted f;auto s=std::make_shared<Source>();gui::Widget image;
    auto current=take(gui::rgba_image(2,2,std::vector<std::uint8_t>(16,255)));
    s->body=[&](gui::WidgetBuilder& b)->core::Result<void>{image=take(b.image({},"frame",{.image=current,.width=240,.height=180}));return {};};
    f.mount(s);
    const auto nodes=take(f.tree->snapshots()).size();
    const auto scene_image=[&]{auto scene=take(f.tree->drawing_snapshot({640,480}));for(const auto& run:scene.images)if(run.image==current)return true;for(const auto& layer:scene.overlays)for(const auto& run:layer.images)if(run.image==current)return true;return false;};
    for(unsigned frame=0;frame<24;++frame){
        std::vector<std::uint8_t> pixels(16,255);for(unsigned p=0;p<4;++p){pixels[p*4]=std::uint8_t(frame*7);pixels[p*4+1]=std::uint8_t(255-frame*7);}
        current=take(gui::rgba_image(2,2,std::move(pixels)));good(image.set_image(current));
        check(scene_image()&&scene_image(),"A frame update or delayed next frame cleared the current native image");
        check(take(f.tree->snapshots()).size()==nodes&&image.snapshot()&&s->builds==1&&s->file_revision==7,"Native video-frame data recreated a view or changed file state");
    }
}
int main(int argc,char** argv){try{std::string n=argc>1?argv[1]:"initial";if(n=="initial")initial();else if(n=="empty")empty();else if(n=="identity")identity();else if(n=="replace")replace();else if(n=="returned"||n=="exception"||n=="empty_candidate")failed(n);else if(n=="invalid")invalid();else if(n=="independent")independent();else if(n=="clear")clear();else if(n=="retirement")retirement();else if(n=="foreign")foreign();else if(n=="reentry")reentry();else if(n=="retire_during_build")retire_during_build();else if(n=="nested")nested();else if(n=="drawing")drawing();else if(n=="frame_data")frame_data();else throw std::runtime_error("Unknown native view test");std::cout<<"Native C++ view "<<n<<" passed without a window\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
