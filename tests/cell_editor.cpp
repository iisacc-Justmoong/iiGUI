#include <iiGUI/iiGUI.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
template<class T>T take(core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
void run(std::string name){
    gui::Host host({.size={420,300}});gui::Widget root,field,label;gui::WidgetMount mount;int edits=0;
    good(host.run({.opened=[&]{
        mount=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{
            root=take(b.absolute_stack({},"root",{120,24}));good(root.set_clip_content(true));
            field=take(b.input_field(root,"field",{.text="123",.width=200,.style=gui::InputStyle{.height=20,.horizontal_padding=0,.corner_radius=0,.background=gui::WidgetColor{0,0,0,0},.focus_ring=false}},[&](std::string){++edits;}));
            good(field.set_frame({0,2,200,20}));label=take(b.label(root,"label",{.text="caption"}));good(label.set_frame({0,22,200,20}));return {};
        }));
        auto f=take(field.snapshot());check(f.size.height==20&&f.clip_bounds==std::optional{take(root.snapshot()).bounds},"Cell editor geometry/clip missing");
        if(name=="editing"){
            good(field.set_accessibility_name("Native table editor"));check(take(field.snapshot()).accessibility_name=="Native table editor","Accessible editor label did not update");
            good(field.focus());good(mount.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::select_all}));good(mount.dispatch_input({.kind=gui::WidgetInputKind::text,.text="한글"}));check(edits==1&&take(field.snapshot()).text=="한글","Styled input changed editing semantics");
            good(field.set_input_options({.text="12",.width=100,.style=gui::InputStyle{.height=24}}));check(take(field.snapshot()).text=="12"&&edits==1,"Options replacement emitted a user edit");
        }else if(name=="validation"){
            check(!field.set_input_options({.width=0})&&!field.set_input_options({.style=gui::InputStyle{.height=std::nan("")}})&&!label.set_input_options({}),"Invalid style or wrong target accepted");
            check(!field.set_label_options({})&&!label.set_label_options({.font_size=0})&&!field.set_clip_content(true),"Invalid label or clip mutation accepted");
            good(label.set_label_options({.text="New",.font_size=17,.color=gui::WidgetColor{1,0,0,1},.font_weight=700}));check(take(label.snapshot()).text=="New"&&take(label.snapshot()).font_size==17,"Label options did not update");
        }else if(name=="ellipsis"){
            good(field.set_visible(false));good(label.set_frame({0,0,80,20}));good(label.set_label_options({.text="A very long 한글 cell value",.font_size=13,.line_height=13}));auto before=take(host.capture_canvas());good(label.set_label_options({.text="A very long 한글 cell value",.font_size=13,.line_height=13,.elide_right=true}));auto after=take(host.capture_canvas());check(before.rgba!=after.rgba,"Native ellipsis did not change truncated text pixels");
        }else if(name=="clipping"){
            auto before=take(host.capture_canvas());good(field.set_text("ABCABCABCABCABCABCABCABC"));auto after=take(host.capture_canvas());auto r=take(root.snapshot()).bounds;auto scale=take(host.snapshot()).backing_scale;const auto x=std::size_t((r.x+r.width+8)*scale),y=std::size_t((r.y+12)*scale),p=(y*after.width+x)*4;
            check(before.rgba!=after.rgba&&std::equal(before.rgba.begin()+p,before.rgba.begin()+p+4,after.rgba.begin()+p),"Cell text escaped clipped container");
            good(root.set_clip_content(false));check(!take(field.snapshot()).clip_bounds,"Clip mutation was not applied");
        }else throw std::runtime_error("Unknown cell editor test");
        good(host.request_close());
    }}));check(!field.snapshot(),"Cell editor outlived native mount");
}
}
int main(int n,char** v){try{run(n>1?v[1]:"editing");std::cout<<"CELL_EDITOR_OK\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
