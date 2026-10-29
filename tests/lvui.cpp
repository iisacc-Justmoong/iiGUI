#include <iiGUI/iiGUI.h>
#include <LVUI/NativeDocument.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace std::chrono_literals;
namespace {
void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> void good(const T& value,const char* message) { check(bool(value),message); }
void good(const ii::core::Result<void>& value,const char*) {
    if (value) return;
    if (value.error().cause) std::rethrow_exception(value.error().cause);
    throw std::runtime_error(value.error().message);
}
lvui::Document document(std::string_view source) {
    auto parsed=lvui::parse(source);
    if (!parsed) throw std::runtime_error(parsed.error().message);
    return std::move(*parsed);
}
const ii::gui::Rect& box(const lvui::native::DocumentLayout& layout,std::string_view id) {
    auto found=std::find_if(layout.elements.begin(),layout.elements.end(),[&](const auto& b) { return b.id==id; });
    if (found==layout.elements.end()) throw std::runtime_error("Missing element ID");
    return found->bounds;
}
void layout() {
    auto doc=document(R"(<main style="padding:20px; gap:10px; color:#ffffff; font-size:20px">
      <div id="row" style="flex-direction:row; gap:10px; height:80px">
        <section id="fixed" style="width:100px; background:#123456"/>
        <section id="flex" style="background:#abcdef"/>
        <section id="percent" style="width:25%; background:#000"/>
      </div><p id="label">Native 한글</p>
    </main>)");
    auto result=lvui::native::layout_document(doc,{640,420}); good(result,"Layout succeeds");
    check(box(*result,"row")==ii::gui::Rect{20,20,600,80},"Column padding and height");
    check(box(*result,"fixed")==ii::gui::Rect{20,20,100,0},"Empty automatic-height box");
    check(box(*result,"flex").x==130 && box(*result,"flex").width==330,"Remaining row space");
    check(box(*result,"percent").x==470 && box(*result,"percent").width==150,"Percent width");
    check(box(*result,"label").y==110 && box(*result,"label").height>=20,"Native text metrics");
    check(result->frame.texts.size()==1 && result->frame.texts[0].font_size==20 &&
        result->frame.texts[0].color.red==1,"Typography inheritance");
    auto narrower=lvui::native::layout_document(doc,{440,420}); good(narrower,"Responsive layout");
    check(box(*narrower,"flex").width==180 && box(*narrower,"percent").width==100,"Reflow uses new viewport");
    auto wrapped=document("<view><p id='p'>A long text that wraps naturally across multiple lines.</p></view>");
    auto wide=lvui::native::layout_document(wrapped,{600,400}), narrow=lvui::native::layout_document(wrapped,{80,400});
    good(wide,"Wide metrics"); good(narrow,"Narrow metrics");
    check(box(*narrow,"p").height>box(*wide,"p").height,"Native word wrapping changes content height");
    auto padding=lvui::native::layout_document(document("<view style='padding:8px'><p id='p' style='height:48px; padding:4px; border-radius:90px; background:#abc'>x</p></view>"),{200,100});
    good(padding,"Text border-box");
    const auto shape=std::find_if(padding->frame.panels.begin(),padding->frame.panels.end(),
        [&](const auto& panel) { return panel.bounds==box(*padding,"p"); });
    check(shape!=padding->frame.panels.end() && box(*padding,"p").height==48 &&
        padding->frame.texts[0].bounds.height==40 && shape->radius==24,
        "Explicit text height clips and radius clamps");
}
void invalid() {
    auto empty=document("<view/>");
    for (auto size:{ii::gui::Size{0,10},{10,-1},{8193,10},{std::numeric_limits<double>::infinity(),1}})
        check(!lvui::native::layout_document(empty,size),"Invalid viewport rejected");
    for (auto source:{"<view style='padding:30px'/>","<view style='width:200px'/>",
        "<view style='flex-direction:row; gap:101px'><view/><view/></view>",
        "<view><view style='height:120px'/></view>","<view><p style='width:0px'>text</p></view>",
        "<view style='flex-direction:row'><view style='width:60%'/><view style='width:60%'/></view>"})
        if (lvui::native::layout_document(document(source),{100,50}))
            throw std::runtime_error(std::string("Overflow accepted: ")+source);
    ii::gui::CanvasFrame frame{.viewport={0,0,100,100}};
    frame.texts.push_back({{0,0,100,30},"Valid"});
    check(ii::gui::valid_canvas(frame),"Valid text frame");
    const auto invalid=[&](auto change) { auto copy=frame; change(copy.texts[0]); check(!ii::gui::valid_canvas(copy),"Text validation"); };
    invalid([](auto& t) { t.text="\xc0\x80"; }); invalid([](auto& t) { t.text.assign(65537,'x'); });
    invalid([](auto& t) { t.text=std::string("a\0b",3); }); invalid([](auto& t) { t.bounds.width=0; });
    for(unsigned weight:{400,500,600,700}) { auto copy=frame; copy.texts[0].font_weight=weight; check(ii::gui::valid_canvas(copy),"LVRS font weight rejected"); }
    invalid([](auto& t) { t.font_size=0; }); invalid([](auto& t) { t.font_weight=550; });
    invalid([](auto& t) { t.font_family=std::string("bad\0font",8); });
    invalid([](auto& t) { t.alignment=static_cast<ii::gui::TextAlignment>(42); });
    invalid([](auto& t) { t.color.alpha=2; });
    frame.texts.resize(4097,frame.texts.front()); check(!ii::gui::valid_canvas(frame),"Bounded run count");
}
void native(ii::gui::Renderer renderer=ii::gui::Renderer::automatic) {
    auto doc=document(R"(<main style="padding:20px; gap:16px; background:#102030; color:#ffffff; font-size:28px">
      <p id="glyphs" style="height:60px">LVUI 한글 &amp; native graphics</p>
      <div style="flex-direction:row; gap:12px; height:80px">
        <section style="height:100%; background:#ff0000; border-radius:12px"/>
        <section style="height:100%; background:#00ff00; border-radius:12px"/>
      </div></main>)");
    ii::gui::Host host({.title="LVUI native rendering test",.size={640,420}});
    auto ui=host.executor(); bool verified=false;
    auto run=host.run({.opened=[&] {
        good(host.set_view(doc,renderer),"Submit parsed LVUI");
        // Driver/pipeline initialization is covered by the outer CTest timeout.
        // The callback watchdog measures the running view after admission.
        good(ui.after(5s,[] { throw std::runtime_error("LVUI native test timeout"); }),"Watchdog");
        good(ui.after(40ms,[&] {
            auto capture=host.capture_canvas(); good(capture,"Capture text and panels from GPU");
            const auto info=host.graphics_info(); good(info,"Inspect GPU");
            if (renderer==ii::gui::Renderer::vulkan)
                check(info->api==ii::gui::GraphicsApi::vulkan && info->presentations>0,"Vulkan LVUI renders and presents through native surface");
            const auto state=host.snapshot(); good(state,"Inspect window");
            const double scale=state->backing_scale;
            auto pixel=[&](int x,int y,int c) { return capture->rgba[(std::size_t(y*scale)*capture->width+std::size_t(x*scale))*4+c]; };
            check(std::abs(int(pixel(0,0,0))-16)<=1 && std::abs(int(pixel(0,0,1))-32)<=1 &&
                std::abs(int(pixel(0,0,2))-48)<=1,"LVUI sRGB colors survive linear GPU composition");
            check(pixel(80,120,0)>250 && pixel(80,120,1)<2 && pixel(480,120,1)>250,"Row backgrounds reach GPU");
            std::size_t bright=0,outside=0;
            for (std::size_t y=0;y<capture->height;++y) for (std::size_t x=0;x<capture->width;++x) {
                const auto* p=capture->rgba.data()+(y*capture->width+x)*4;
                if (p[0]>200 && p[1]>200 && p[2]>200) {
                    if (x>=20*scale && y>=20*scale && y<80*scale) ++bright; else ++outside;
                }
            }
            check(bright>100 && outside==0,"Actual shaped glyph pixels and rectangular clipping");
            const auto renders=info->scene_renders;
            good(host.set_view(doc,renderer),"Equal LVUI re-submit"); good(host.capture_canvas(),"Flush unchanged view");
            check(host.graphics_info()->scene_renders==renders,"Equal views reuse retained graphics");
            check(!host.set_view(document("<view style='padding:8192px'/>"),renderer),"Invalid replacement rejected");
            auto preserved=host.capture_canvas(); good(preserved,"Capture previous valid scene");
            check(preserved->rgba==capture->rgba,"Invalid layout preserves pixels");
            auto layout=lvui::native::layout_document(doc,state->size); good(layout,"Inspect text frame");
            layout->frame.renderer=renderer;
            layout->frame.texts.clear(); good(host.set_canvas(layout->frame),"Remove text layer");
            auto baseline=host.capture_canvas(); good(baseline,"Capture panels without glyphs");
            check(baseline->rgba!=capture->rgba,"Text is in captured GPU attachment, not a separate window label");
            ii::gui::CanvasFrame quality{.viewport={0,0,240,100},.background={0,0,0,1}};
            quality.renderer=renderer;
            quality.texts.push_back({{20,10,180,60},"MMMMMM",{0.5f,0,0,0.5f},52});
            good(host.set_canvas(quality),"Linear text alpha sample");
            auto alpha=host.capture_canvas(); good(alpha,"Read composed text alpha");
            std::uint8_t maximum=0;
            for (std::size_t i=0;i<alpha->rgba.size();i+=4) maximum=std::max(maximum,alpha->rgba[i]);
            check(maximum>=135 && maximum<=140,"Text alpha composes in linear light: 0.5 * 0.5 to sRGB 137");
            std::size_t first_ink=alpha->height;
            for(std::size_t y=0;y<alpha->height&&first_ink==alpha->height;++y)
                for(std::size_t x=0;x<alpha->width;++x)
                    if(alpha->rgba[(y*alpha->width+x)*4]>=10){first_ink=y;break;}
            check(first_ink<alpha->height,"Unclipped native glyph ink must exist");
            const auto clip_bottom=std::min(69.,std::ceil((first_ink+8)/scale));
            check(clip_bottom>first_ink/scale&&clip_bottom<70,"Native font fixture must retain and crop glyph ink");
            quality.texts[0].bounds.height=float(clip_bottom-10);
            good(host.set_canvas(quality),"Clip oversized glyphs");
            auto clipped=host.capture_canvas(); good(clipped,"Read clipped glyphs");
            std::size_t inside=0;
            for (std::size_t y=0;y<clipped->height;++y) for (std::size_t x=0;x<clipped->width;++x) {
                if (clipped->rgba[(y*clipped->width+x)*4]<10) continue;
                check(x>=20*scale && x<200*scale && y>=10*scale && y<clip_bottom*scale,"Glyph pixels respect explicit clipping rectangle");
                check(clipped->rgba[(y*clipped->width+x)*4]==alpha->rgba[(y*alpha->width+x)*4],"Clipping must retain native glyph shaping and origin");
                ++inside;
            }
            check(inside>10,"Clipping retains visible glyph coverage");
            good(host.set_content_size({480,420}),"Resize native viewport"); good(host.set_view(doc,renderer),"Reflow on resize");
            auto resized=host.capture_canvas(); good(resized,"Capture resized view");
            check(resized->width==std::ceil(480*scale),"Physical canvas tracks resized view");
            std::cout << "LVUI_NATIVE_OK adapter=" << info->adapter << " hardware=" << info->hardware
                << " glyph_pixels=" << bright << " resize=true retained=true\n";
            verified=true; good(host.request_close(),"Close native view");
        }),"Native capture task");
    }});
    good(run,"Native LVUI lifecycle"); check(verified,"Native verification ran");
}
}
int main(int argc,char** argv) {
    try {
        const std::string name=argc>1?argv[1]:"layout";
        if (name=="layout") layout(); else if (name=="invalid") invalid(); else if (name=="native") native();
        else if (name=="vulkan") native(ii::gui::Renderer::vulkan);
        else throw std::runtime_error("Unknown LVUI case");
        std::cout<<"LVUI "<<name<<" passed\n";
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
