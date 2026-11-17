#include <iiGUI/NativeText.h>
#include <iiGUI/FontDatabase.h>
#include <iiGUI/TextLine.h>
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
           lines[2][0]==='gamma' && lines[0][1]===8 && lines[1][1]===18 && lines[2][1]===28;
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
EM_JS(int,font_styles_are_expected,(),{
    const fonts=globalThis.iiGUIWebTextTestFonts;
    return fonts.some(font=>font==='400 10px sans-serif') &&
           fonts.some(font=>font==='400 10px "Custom Font"');
});
EM_JS(int,letter_spacing_was_drawn,(),{
    return globalThis.iiGUIWebTextTestLines.some(line => line[2] === '2px');
});
EM_JS(int,centered_line_box_was_drawn,(),{
    const lines=globalThis.iiGUIWebTextTestLines;
    return lines.length===1 && lines[0][1]===13 && lines[0][3]==='alphabetic';
});
EM_JS(int,underline_is_below_baseline,(),{return globalThis.iiGUIWebTextTestUnderlines.at(-1)===14;});
EM_JS(void,remove_font_metrics,(),{globalThis.iiGUINoFontMetrics=true;});
EM_JS(int,missing_metrics_use_centered_top,(),{
    const lines=globalThis.iiGUIWebTextTestLines;
    return lines.length===1 && lines[0][1]===5 && lines[0][3]==='top';
});
EM_JS(void,remove_letter_spacing_support,(),{globalThis.iiGUINoLetterSpacing=true;});
#if defined(IIGUI_WEB_TEXT_TEST_SHARED)
EM_JS(int,heap_is_shared,(),{ return HEAPU8.buffer instanceof SharedArrayBuffer; });
#endif

int main(){
    auto index=ii::gui::TextIndex::create("editable text");
    if(!index)throw std::runtime_error("Cannot create text index");
    auto editable=ii::gui::TextLine::create(std::move(*index));
    if(editable||editable.error().code!=ii::core::ErrorCode::unsupported)
        throw std::runtime_error("Unavailable browser text layout must report unsupported");
#if defined(IIGUI_WEB_TEXT_TEST_SHARED)
    if(!heap_is_shared())throw std::runtime_error("Web text test did not use shared WebAssembly memory");
#endif
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
    run.font_family="Custom Font";
    (void)ii::gui::detail::measure_text_width(run);
    frame.texts={run};(void)ii::gui::detail::rasterize_text(frame,1);
    if(!font_styles_are_expected())throw std::runtime_error("Web text did not preserve generic and named font families");
    clear_painted_lines();
    run.text="ABCD";run.bounds.width=20;run.letter_spacing=2;
    if(std::abs(ii::gui::detail::measure_text_width(run)-10)>0.01)
        throw std::runtime_error("Web text measurement did not apply letter spacing");
    frame.viewport.width=20;frame.texts={run};(void)ii::gui::detail::rasterize_text(frame,1);
    if(!letter_spacing_was_drawn())throw std::runtime_error("Web text drawing did not apply letter spacing");
    clear_painted_lines();run.text="A";run.letter_spacing=0;run.line_height=20;run.underline=true;frame.texts={run};
    (void)ii::gui::detail::rasterize_text(frame,1);
    if(!centered_line_box_was_drawn())throw std::runtime_error("Web text was not centered in its line box");
    if(!underline_is_below_baseline())throw std::runtime_error("Web underline was not placed below the baseline");
    clear_painted_lines();remove_font_metrics();frame.texts={run};
    (void)ii::gui::detail::rasterize_text(frame,1);
    if(!missing_metrics_use_centered_top())throw std::runtime_error("Web text did not fall back when font metrics were unavailable");
    run.letter_spacing=2;
    remove_letter_spacing_support();
    try {
        (void)ii::gui::detail::measure_text_width(run);
        throw std::runtime_error("Missing browser letter-spacing support was silently accepted");
    } catch(const std::runtime_error& error) {
        if(std::string(error.what()).find("unavailable")==std::string::npos)throw;
    }
}
