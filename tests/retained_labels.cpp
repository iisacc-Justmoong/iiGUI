#include "../src/iiGUI/WidgetState.h"
#include <iostream>
#include <stdexcept>

using namespace ii;
template<class T>T take(core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
int main()try{
    auto state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget status;
    good(state->construct([&](gui::WidgetBuilder& builder)->core::Result<void>{
        auto root=take(builder.vstack({},"root",{.spacing=16,.padding=32,.alignment=gui::StackAlignment::center}));
        take(builder.label(root,"greeting",{.text="Hello World",.font_size=32,.selectable=true,.role=gui::TextRole::title}));
        take(builder.label(root,"description",{.text="iiSDKCore + LVUI + Vulkan",.font_size=16,.selectable=true,.role=gui::TextRole::body}));
        status=take(builder.label(root,"status",{.text="Native C++23",.font_size=14,.selectable=true,.role=gui::TextRole::description}));return {};
    }));
    for(int tick:{0,1,2,9,10,99,100,135,999,1000}){
        const auto value="Native C++23 - "+std::to_string(tick)+" s";good(status.set_text(value));
        for(int repeat=0;repeat<2;++repeat){
            auto frame=take(state->drawing_snapshot({360,640}));
            std::vector<std::string> texts;
            const auto collect=[&](const auto& runs){for(const auto& run:runs){
                if(run.bounds.width<=0||run.bounds.height<=0)throw std::runtime_error("Retained label lost its bounds");
                texts.push_back(run.text);
            }};
            collect(frame.texts);for(const auto& layer:frame.overlays)collect(layer.texts);
            if(texts!=std::vector<std::string>{"Hello World","iiSDKCore + LVUI + Vulkan",value})
                throw std::runtime_error("A changing label removed static sibling text from the retained scene");
        }
    }
    state->close();std::cout<<"RETAINED_LABELS_OK digit_transitions=true siblings=true repeated_frames=true\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
