#include <iiGUI/NativeText.h>
#include <iiGUI/FontDatabase.h>
#include <emscripten.h>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

// The standalone WebAssembly check can link only the text backend without
// loading an application font service. Full iiGUI builds use the real one.
#if defined(IIGUI_WEB_TEXT_TEST_FONT)
namespace ii::gui {
FontDatabase::FontDatabase(std::shared_ptr<FontProvider>,std::optional<FontFace>){}
FontDatabase::~FontDatabase()=default;
std::shared_ptr<FontDatabase> FontDatabase::application(){
    static auto value=std::make_shared<FontDatabase>(std::shared_ptr<FontProvider>{},std::nullopt);return value;
}
std::string FontDatabase::resolve_family(std::string_view value)const{return value.empty()?"sans-serif":std::string(value);}
}
#endif

EM_JS(int,painted_lines_are_expected,(),{
    const lines=globalThis.iiGUIWebTextTestLines;
    return lines.length===3 && lines[0][0]==='Alpha ' && lines[1][0]==='beta ' &&
           lines[2][0]==='gamma' && lines[0][1]===0 && lines[1][1]===10 && lines[2][1]===20;
});
EM_JS(void,clear_painted_lines,(),{globalThis.iiGUIWebTextTestLines.length=0;});
EM_JS(int,explicit_lines_are_expected,(),{
    const lines=globalThis.iiGUIWebTextTestLines;
    return lines.length===3 && lines[0][0]==='A' && lines[1][0]==='B' && lines[2][0].length===0;
});
EM_JS(int,ellipsized_line_is_expected,(),{
    const lines=globalThis.iiGUIWebTextTestLines;
    return lines.length===1 && lines[0][0]==='Alph…';
});
EM_JS(int,korean_lines_are_expected,(),{
    const lines=globalThis.iiGUIWebTextTestLines;
    return lines.length===2 && lines[0][0]==='가나' && lines[1][0]==='다라';
});

int main(){
    ii::gui::TextRun run{.bounds={0,0,5,30},.text="Alpha beta gamma",.font_size=10,.line_height=10};
    if(std::abs(ii::gui::detail::measure_text(run)-30)>0.01)throw std::runtime_error("Web measurement did not use wrapped lines");
    ii::gui::CanvasFrame frame{.viewport={0,0,5,30},.texts={run}};
    (void)ii::gui::detail::rasterize_text(frame,1);
    if(!painted_lines_are_expected())throw std::runtime_error("Web drawing did not use the measured lines");
    clear_painted_lines();
    run.text="A\r\nB\n";run.bounds.width=20;
    if(std::abs(ii::gui::detail::measure_text(run)-30)>0.01)throw std::runtime_error("Web measurement lost explicit lines");
    frame.viewport.width=20;frame.texts={run};
    (void)ii::gui::detail::rasterize_text(frame,1);
    if(!explicit_lines_are_expected())throw std::runtime_error("Web drawing lost explicit empty lines");
    clear_painted_lines();
    run.text="Alpha beta";run.bounds.width=5;run.elide_right=true;
    if(std::abs(ii::gui::detail::measure_text(run)-10)>0.01)throw std::runtime_error("Web ellipsis mode unexpectedly wrapped");
    frame.viewport.width=5;frame.texts={run};
    (void)ii::gui::detail::rasterize_text(frame,1);
    if(!ellipsized_line_is_expected())throw std::runtime_error("Web ellipsis drawing changed");
    clear_painted_lines();
    run.text="가나다라";run.bounds.width=2;run.elide_right=false;
    if(std::abs(ii::gui::detail::measure_text(run)-20)>0.01)throw std::runtime_error("Web UTF-8 measurement did not wrap");
    frame.viewport.width=2;frame.texts={run};
    (void)ii::gui::detail::rasterize_text(frame,1);
    if(!korean_lines_are_expected())throw std::runtime_error("Web UTF-8 drawing changed characters");
}
