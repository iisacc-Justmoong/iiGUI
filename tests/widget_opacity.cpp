#include "../src/Widgets/WidgetState.h"
#include <cmath>
#include <future>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void near(double actual,double expected){check(std::abs(actual-expected)<1e-6,"Opacity product differs");}
template<class T>T take(core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
double text_opacity(const gui::CanvasFrame& scene,std::string_view text){
    for(const auto& item:scene.texts)if(item.text==text)return 1;
    for(const auto& layer:scene.overlays)for(const auto& item:layer.texts)if(item.text==text)return layer.content_transform.opacity;
    std::string message="Drawing text not found: "+std::string(text)+"; present:";
    for(const auto& layer:scene.overlays)for(const auto& item:layer.texts)message+=" ["+item.text+"]";
    throw std::runtime_error(message);
}
struct Fixture final {
    std::shared_ptr<gui::detail::WidgetState> state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host=take(state->content_host({},"host",{640,480})),root,group,child,sibling,button,field,slot,inner;
    int clicks=0,retired=0;
    Fixture(){
        good(host.set_content([&](gui::WidgetBuilder& builder)->core::Result<void>{
            root=take(builder.vstack({},"root",{.spacing=4,.alignment=gui::StackAlignment::leading}));
            group=take(builder.vstack(root,"group",{.spacing=2,.alignment=gui::StackAlignment::leading}));
            child=take(builder.label(group,"child",{.text="Child"}));
            button=take(builder.button(group,"button",{.text="Action"},[&]{++clicks;}));
            field=take(builder.input_field(group,"field",{.text="Draft",.width=180}));
            slot=take(builder.content_host(group,"slot",{200,40}));
            good(slot.set_content([&](gui::WidgetBuilder& nested)->core::Result<void>{inner=take(nested.label({},"inner",{.text="Inner"}));return nested.on_retire([&]{++retired;});}));
            sibling=take(builder.label(root,"sibling",{.text="Sibling"}));return {};
        }));
    }
    ~Fixture(){state->close();}
    gui::CanvasFrame drawing(){return take(state->drawing_snapshot({640,480}));}
};
void values(){Fixture f;near(take(f.child.snapshot()).opacity,1);near(take(f.child.snapshot()).effective_opacity,1);good(f.child.set_opacity(.3));near(take(f.child.snapshot()).opacity,.3);good(f.child.set_opacity(.3));near(take(f.child.snapshot()).effective_opacity,.3);good(f.child.set_opacity(1));near(take(f.child.snapshot()).opacity,1);}
void inheritance(){Fixture f;good(f.root.set_opacity(.8));good(f.group.set_opacity(.5));good(f.child.set_opacity(.5));near(take(f.child.snapshot()).effective_opacity,.2);near(take(f.child.snapshot()).opacity,.5);near(take(f.sibling.snapshot()).effective_opacity,.8);good(f.group.set_opacity(.25));near(take(f.child.snapshot()).effective_opacity,.1);near(take(f.sibling.snapshot()).effective_opacity,.8);}
void content(){Fixture f;good(f.host.set_opacity(.8));good(f.root.set_opacity(.5));good(f.slot.set_opacity(.5));good(f.inner.set_opacity(.5));near(take(f.inner.snapshot()).effective_opacity,.1);near(text_opacity(f.drawing(),"Inner"),.1);const auto generation=take(f.slot.snapshot()).content_generation;good(f.host.set_opacity(.4));near(take(f.inner.snapshot()).effective_opacity,.05);near(text_opacity(f.drawing(),"Inner"),.05);check(take(f.slot.snapshot()).content_generation==generation&&f.retired==0,"Opacity recreated owned content");}
void batching(){Fixture f;good(f.root.set_opacity(.8));good(f.group.set_opacity(.5));good(f.child.set_opacity(.5));auto scene=f.drawing();near(text_opacity(scene,"Child"),.2);near(text_opacity(scene,"Action"),.4);near(text_opacity(scene,"Sibling"),.8);good(f.group.set_clip_content(true));scene=f.drawing();near(text_opacity(scene,"Child"),.2);near(text_opacity(scene,"Sibling"),.8);good(f.group.set_opacity(1));scene=f.drawing();near(text_opacity(scene,"Child"),.4);near(text_opacity(scene,"Sibling"),.8);}
void drawing(){
    Fixture f;const auto image=take(gui::rgba_image(1,1,{255,255,255,255}));
    good(f.group.set_opacity(.4));
    good(f.group.set_paint_handler([image](const gui::WidgetPaintState&)->core::Result<std::shared_ptr<const gui::WidgetDrawing>>{
        auto ink=std::make_shared<gui::WidgetDrawing>();ink->panels.push_back({{0,0,10,10},{1,0,0,.6f},{1,0,0,.6f}});
        ink->texts.push_back({.bounds={0,0,80,20},.text="Ink"});ink->images.push_back({{0,0,8,8},image,{},.7f});
        ink->images_before_text=true;return ink;
    }));
    auto scene=f.drawing();near(text_opacity(scene,"Ink"),.4);
    bool found=false;for(const auto& layer:scene.overlays)if(!layer.images.empty()&&layer.images.front().image==image){near(layer.content_transform.opacity,.4);near(layer.images.front().opacity,.7);near(layer.panels.front().top.alpha,.6);found=true;}
    check(found,"Native drawing lost image/panel composition");
    good(f.group.set_opacity(0));scene=f.drawing();for(const auto& layer:scene.overlays)for(const auto& text:layer.texts)check(text.text!="Ink"&&text.text!="Child","Transparent subtree still emitted ink");near(text_opacity(scene,"Sibling"),1);
}
void interaction(){
    Fixture f;good(f.field.set_text("Working"));good(f.field.focus());const auto before=take(f.field.snapshot());const auto generation=take(f.host.snapshot()).content_generation;
    good(f.group.set_opacity(0));const auto after=take(f.field.snapshot());check(after.visible&&after.enabled&&after.focused&&after.text=="Working"&&after.bounds==before.bounds,"Transparency changed editing or layout state");
    good(f.state->input({.kind=gui::WidgetInputKind::text,.text="!"}));check(take(f.field.snapshot()).text!=before.text,"Transparent input lost editing");
    const auto bounds=take(f.button.snapshot()).bounds;
    good(f.state->input({.kind=gui::WidgetInputKind::pointer_down,.x=bounds.x+bounds.width/2,.y=bounds.y+bounds.height/2}));
    good(f.state->input({.kind=gui::WidgetInputKind::pointer_up,.x=bounds.x+bounds.width/2,.y=bounds.y+bounds.height/2}));
    check(f.clicks==1&&take(f.host.snapshot()).content_generation==generation&&f.retired==0,"Opacity disabled interaction or replaced a generation");
}
void popup(){
    Fixture f;gui::Widget menu,first,second,third;
    good(f.slot.set_content([&](gui::WidgetBuilder& b)->core::Result<void>{
        auto root=take(b.vstack({},"menu-root"));menu=take(b.menu(root,"menu",{.opacity=.3}));
        first=take(b.menu_item(menu,"first",{.label="First"}));second=take(b.menu_item(menu,"second",{.label="Second"}));third=take(b.menu_item(menu,"third",{.label="Third"}));return {};
    }));
    good(f.root.set_opacity(.8));good(f.slot.set_opacity(.5));good(menu.set_opacity(.5));good(second.set_opacity(.5));good(menu.open_at(20,20));
    const auto scene=f.drawing();near(take(first.snapshot()).effective_opacity,.2);near(text_opacity(scene,"First"),.2);near(text_opacity(scene,"Second"),.1);near(text_opacity(scene,"Third"),.2);
    good(menu.set_opacity(0));check(take(menu.snapshot()).opened&&take(first.snapshot()).enabled,"Popup transparency changed its lifecycle");const auto hidden=f.drawing();for(const auto& layer:hidden.overlays)for(const auto& item:layer.texts)check(item.text!="First","Transparent popup emitted ink");
}
void validation(){
    Fixture f;good(f.child.set_opacity(.6));for(double value:{-1.,1.01,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){auto result=f.child.set_opacity(value);check(!result&&result.error().code==core::ErrorCode::invalid_argument,"Invalid opacity was accepted");near(take(f.child.snapshot()).opacity,.6);}
    for(auto value:std::initializer_list<gui::Size>{{0,20},{20,0},{8193,20},{20,std::numeric_limits<double>::quiet_NaN()}})check(!f.state->drawing_snapshot(value),"Invalid drawing viewport was accepted");
    auto result=std::async(std::launch::async,[&]{return f.child.set_opacity(.2);}).get();check(!result&&result.error().code==core::ErrorCode::wrong_executor,"Worker changed native opacity");near(take(f.child.snapshot()).opacity,.6);
}
void retirement(){Fixture f;auto old=f.child;good(f.host.clear_content());check(!old.set_opacity(.5)&&f.retired==1,"Retired opacity target affected a successor");good(f.host.set_content([&](gui::WidgetBuilder& b)->core::Result<void>{f.child=take(b.label({},"child",{.text="New"}));return {};}));near(take(f.child.snapshot()).opacity,1);check(!old.set_opacity(0),"Old generation admitted a new opacity command");}
}
int main(int argc,char** argv){try{const std::string name=argc>1?argv[1]:"values";if(name=="values")values();else if(name=="inheritance")inheritance();else if(name=="content")content();else if(name=="batching")batching();else if(name=="drawing")drawing();else if(name=="interaction")interaction();else if(name=="popup")popup();else if(name=="validation")validation();else if(name=="retirement")retirement();else throw std::runtime_error("Unknown opacity test");std::cout<<"Widget opacity "<<name<<" passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
