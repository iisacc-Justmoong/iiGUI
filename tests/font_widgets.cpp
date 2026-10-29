#include <iiGUI/FontDatabase.h>
#include <iiGUI/WidgetState.h>
#include <iostream>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
struct Provider final:gui::FontProvider {
    std::vector<std::string> families()const override{return {"System","Arial"};}
    std::string system_family()const override{return "System";}
    std::string resolve(const gui::FontFace& face)const override{return face.family;}
    gui::FontLoadReport load(std::span<const gui::FontResource> resources)override{gui::FontLoadReport result;for(const auto& font:resources)result.resources.push_back({std::string(font.file),false,"No asset"});return result;}
};
struct Mounted final {
    std::shared_ptr<gui::FontDatabase> fonts=std::make_shared<gui::FontDatabase>(std::make_shared<Provider>(),gui::FontFace{"System",400,"Regular"});
    std::shared_ptr<gui::detail::WidgetState> tree;
    gui::Widget host,field,label;
    Mounted(){auto appearance=gui::lvrs_appearance();appearance.font_family="Missing";tree=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();},appearance,gui::detail::WidgetFrameHooks{.fonts=fonts});host=take(tree->content_host({},"host",{640,480}));
        good(host.set_content([&](gui::WidgetBuilder& b)->core::Result<void>{auto root=take(b.vstack({},"root"));label=take(b.label(root,"label",{.text="Preview",.font_size=15}));field=take(b.input_field(root,"input",{.text="Draft"}));return {};}));}
    ~Mounted(){tree->close();}
    gui::TextRun preview(){auto frame=take(tree->drawing_snapshot({640,480}));for(const auto& text:frame.texts)if(text.text=="Preview")return text;for(const auto& layer:frame.overlays)for(const auto& text:layer.texts)if(text.text=="Preview")return text;throw std::runtime_error("Missing font preview");}
};
void tokens(){Mounted m;check(m.preview().font_weight==600,"Pixel-size font token not used by the renderer");for(const auto& token:gui::lvrs_text_tokens()){good(m.label.set_label_options({.text="Preview",.font_size=double(token.pixels)}));check(m.preview().font_weight==unsigned(token.weight),"Renderer and policy token snapshot diverged");}good(m.label.set_label_options({.text="Preview",.font_size=15,.font_weight=700}));check(m.preview().font_weight==700,"Explicit label weight overwritten");}
void updates(){Mounted m;check(m.preview().font_family=="System","Initial font context ignored");take(m.fonts->set_application_font({"Arial",400,"Regular"}));check(m.preview().font_family=="Arial","Application font change did not reach the existing text run");
    good(m.host.set_content([&](gui::WidgetBuilder& b)->core::Result<void>{
        auto root=take(b.vstack({},"candidate"));m.label=take(b.label(root,"label",{.text="Preview"}));
        auto nested=take(b.content_host(root,"nested",{}));
        return nested.set_content([&](gui::WidgetBuilder& child)->core::Result<void>{
            take(child.label({},"nestedLabel",{.text="Nested preview"}));
            take(m.fonts->set_application_font({"System",400,"Regular"}));return {};
        });
    }));
    auto frame=take(m.tree->drawing_snapshot({640,480}));int checked=0;
    for(const auto& layer:frame.overlays)for(const auto& text:layer.texts)if(text.text=="Preview"||text.text=="Nested preview"){check(text.font_family=="System","A staged font change missed an unpublished ancestor or child");++checked;}
    for(const auto& text:frame.texts)if(text.text=="Preview"||text.text=="Nested preview"){check(text.font_family=="System","A staged font change missed the published tree");++checked;}
    check(checked==2,"Both nested staged font previews must be inspected");
}
void identity(){Mounted m;good(m.field.focus());good(m.field.set_text("Retained"));const auto generation=take(m.host.snapshot()).content_generation;const auto before=take(m.field.snapshot());take(m.fonts->set_application_font({"Arial",600,"SemiBold"}));const auto after=take(m.field.snapshot());check(after.id==before.id&&after.text=="Retained"&&after.focused&&take(m.host.snapshot()).content_generation==generation,"Font update rebuilt widgets or lost input/focus");}
void retirement(){Mounted m;good(m.host.clear_content());check(!m.label.snapshot(),"Font child did not retire");take(m.fonts->set_application_font({"Arial",400,"Regular"}));m.tree->close();take(m.fonts->set_application_font({"System",400,"Regular"}));}
void paint_context(){
    Mounted m;std::string selected;
    good(m.host.set_content([&](gui::WidgetBuilder& b)->core::Result<void>{
        auto shape=take(b.absolute_stack({},"paint",{100,30}));
        return shape.set_paint_handler([&](const gui::WidgetPaintState& state)->core::Result<std::shared_ptr<const gui::WidgetDrawing>>{
            selected=state.font_family;return std::make_shared<const gui::WidgetDrawing>();
        });
    }));
    take(m.tree->drawing_snapshot({640,480}));check(selected=="System","Paint context lost resolved fallback font");
    take(m.fonts->set_application_font({"Arial",400,"Regular"}));
    take(m.tree->drawing_snapshot({640,480}));check(selected=="Arial","Existing paint context did not receive font changes");
}
}
int main(int argc,char** argv){try{const std::string name=argc>1?argv[1]:"tokens";if(name=="tokens")tokens();else if(name=="updates")updates();else if(name=="identity")identity();else if(name=="retirement")retirement();else if(name=="paint_context")paint_context();else throw std::runtime_error("Unknown font widget test");std::cout<<"Font widget "<<name<<" passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
