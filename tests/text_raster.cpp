#include <iiGUI/Host.h>
#include <iiGUI/Text.h>
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
int main(){try{ii::gui::Host host;auto run=host.run({.opened=[&]{
    ii::gui::TextRun text{.bounds={10,20,256,96},.text="한글 Native\nText",.color={1,0,0,.5f},.font_size=24};auto bitmap=ii::gui::render_text(text,2);if(!bitmap)throw std::runtime_error(bitmap.error().message);
    if(bitmap->width!=512||bitmap->height!=192)throw std::runtime_error("Text device scale changed");
    std::size_t painted=0,clear=0;for(std::size_t i=0;i<bitmap->rgba.size();i+=4){const auto alpha=bitmap->rgba[i+3];if(alpha){++painted;if(alpha>129)throw std::runtime_error("Text alpha exceeded its paint");if(alpha>64&&bitmap->rgba[i]<200)throw std::runtime_error("Text pixels are premultiplied instead of straight alpha");}else ++clear;}
    if(!painted||!clear)throw std::runtime_error("Native glyphs or transparent background missing");
    text.clip=ii::gui::Rect{70,20,100,96};auto cropped=ii::gui::render_text(text,2);
    if(!cropped)throw std::runtime_error(cropped.error().message);
    std::size_t visible_ink=0;
    for(std::size_t y=0;y<bitmap->height;++y)for(std::size_t x=0;x<bitmap->width;++x){const auto i=(y*bitmap->width+x)*4;
        if(x>=120&&x<320){for(unsigned c=0;c<4;++c)if(cropped->rgba[i+c]!=bitmap->rgba[i+c])throw std::runtime_error("Text crop changed shaping origin");visible_ink+=cropped->rgba[i+3]!=0;}
        else if(cropped->rgba[i+3])throw std::runtime_error("Text escaped per-run crop");
    }
    if(!visible_ink)throw std::runtime_error("Text crop removed all visible ink");
    auto decorated=text;decorated.clip.reset();decorated.text="Underline Native";decorated.bounds={0,0,256,60};decorated.line_height=30;
    auto plain=ii::gui::render_text(decorated,2);decorated.underline=true;auto underlined=ii::gui::render_text(decorated,2);
    if(!plain||!underlined||plain->width!=underlined->width||plain->height!=underlined->height||plain->rgba==underlined->rgba)throw std::runtime_error("Underline changed bitmap geometry or failed to paint");
    text.clip->width=std::numeric_limits<float>::quiet_NaN();if(ii::gui::render_text(text))throw std::runtime_error("Invalid text clip accepted");text.clip.reset();
    text.font_size=std::numeric_limits<float>::quiet_NaN();if(ii::gui::render_text(text))throw std::runtime_error("Invalid text paint accepted");if(!host.request_close())throw std::runtime_error("Cannot close text test");
}});if(!run){if(run.error().cause)std::rethrow_exception(run.error().cause);throw std::runtime_error(run.error().message);}std::cout<<"NATIVE_TEXT_OK shaping=true rgba=straight scale=2\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
