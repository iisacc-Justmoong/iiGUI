#pragma once
#include <iiGUI/InputClearDrawing.h>
#include <iiGUI/TextFieldMaterial.h>
#include <iiGUI/NativeTextLayout.h>
#include <iiGUI/TextInputModel.h>
#include <iiGUI/Desktop.h>
#include <iiGUI/Image.h>
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/TextBoundary.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <expected>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
namespace ii::gui {
template<class Policy>
class BasicNativeInputField final {
    using InputFieldConfiguration=typename Policy::Configuration;using InputFieldPalette=typename Policy::Palette;
    using InputFieldSurface=typename Policy::Surface;using StyleColor=typename Policy::Color;using TextInput=typename Policy::TextInput;
    using TextInputRange=typename Policy::Range;using TextInputError=typename Policy::Error;using TextInputErrorCode=typename Policy::ErrorCode;
    template<class T>using TextInputResult=std::expected<T,TextInputError>;
    using NativeInputFieldCallbacks=typename Policy::Callbacks;using NativeInputFieldClipboard=typename Policy::Clipboard;
    using NativeInputFieldSnapshot=typename Policy::Snapshot;using TextFieldMaterial=typename Policy::Material;
    using MaterialVariant=typename Policy::MaterialVariant;using Defaults=typename Policy::Defaults;
    struct State final {
        struct Click final {
            double time=0,x=0,y=0;
            ii::gui::PointerDevice device=ii::gui::PointerDevice::mouse;
            std::uint64_t device_id=0;
            bool moved=false,native_sequence=false;
        };
        InputFieldConfiguration options;
        std::optional<TextInput> input;
        NativeTextLayout layout;
        NativeInputFieldCallbacks callbacks;
        NativeInputFieldClipboard clipboard;
        TextFieldMaterial material;
        ii::gui::Widget root,ink,clear;
        ii::gui::Size extent;
        std::string last_input_text,inherited_font,clear_id;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0;
        double scroll_x=0,scroll_y=0;
        NativeTextAffinity affinity=NativeTextAffinity::downstream;
        bool wrapping_fallback=false;
        std::optional<ii::core::Error> layout_failure;
        std::optional<double> reported_content_height;
        std::optional<double> preferred_x;
        std::optional<Click> press,last_click;
        std::optional<TextInputRange> word_drag;
        bool dragging=false,hovered=false,effective_enabled=true,composition_cursor_hidden=false,retired=false;
        std::size_t activities=0;
        State(InputFieldConfiguration value,TextInput text,NativeTextLayout shaped,
              NativeInputFieldCallbacks events,NativeInputFieldClipboard port,std::string font)
            :options(std::move(value)),input(std::move(text)),layout(std::move(shaped)),
             callbacks(std::move(events)),clipboard(std::move(port)),extent{options.width,options.height},last_input_text(options.text),inherited_font(std::move(font)){}
    };
    static void release_retired(State& state)noexcept{
        if(!state.retired||state.activities)return;
        state.input.reset();state.layout=NativeTextLayout{};state.layout_failure.reset();
        state.options.text.clear();state.options.placeholder.clear();state.options.accessibility_name.clear();
        state.options.clear_accessibility_name.clear();state.options.font_family.clear();
        std::string{}.swap(state.last_input_text);std::string{}.swap(state.inherited_font);std::string{}.swap(state.clear_id);
        try{state.material=TextFieldMaterial{};}catch(...){}
    }
    struct Activity final {
        std::shared_ptr<State> state;
        explicit Activity(std::shared_ptr<State> value):state(std::move(value)){++state->activities;}
        ~Activity(){--state->activities;release_retired(*state);}
    };
    struct Viewport final {double x=0,y=0,width=0,height=0;};
    struct Scroll final {double x=0,y=0;};
    struct Shaped final {NativeTextLayout layout;bool wrapping_fallback=false;};
    std::shared_ptr<State> state_;
    explicit BasicNativeInputField(std::shared_ptr<State> state):state_(std::move(state)){}

    [[nodiscard]] static ii::core::Result<void> publish_content_height(const std::shared_ptr<State>& state){
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Input field has retired");Activity activity(state);
        if(!state->callbacks.content_height_changed)return {};
        if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());
        const auto height=state->layout.height();
        if(state->reported_content_height&&*state->reported_content_height==height)return {};
        state->reported_content_height=height;
        try{auto callback=state->callbacks.content_height_changed;callback(height);return {};}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI input height callback failed",std::current_exception());}
    }

    [[nodiscard]] static ii::core::Error input_error(const TextInputError& error){
        const auto code=error.code==TextInputErrorCode::too_large?ii::core::ErrorCode::out_of_range:
                        error.code==TextInputErrorCode::unavailable?ii::core::ErrorCode::failed:ii::core::ErrorCode::invalid_argument;
        return {code,error.message};
    }
    [[nodiscard]] static ii::gui::TextLineStyle line_style(const InputFieldConfiguration& value,std::string_view inherited){
        return {.font_family=value.font_family.empty()?std::string(inherited):value.font_family,.font_size=value.font_size,.line_height=value.line_height};
    }
    [[nodiscard]] static double clear_reservation(const InputFieldConfiguration& value,ii::gui::Size size)noexcept{
        if(!value.clear_button_visible||value.multiline)return 0;
        const auto padding=(std::min)(value.horizontal_padding,(std::max)(0.,size.width/2));
        const auto icon=Defaults::icon_size,gap=Defaults::gap;
        return size.height>=icon&&size.width>=padding*2+icon+gap?icon+gap:0;
    }
    [[nodiscard]] static Viewport viewport(const InputFieldConfiguration& value,ii::gui::Size size,const NativeTextLayout& layout){
        const auto x=(std::min)(value.horizontal_padding,(std::max)(0.,size.width/2));
        const auto y=value.multiline?(std::min)(value.vertical_padding,(std::max)(0.,size.height/2)):
            (std::max)(0.,(size.height-layout.lines().front().height)/2);
        return {x,y,(std::max)(0.,size.width-x*2-clear_reservation(value,size)),(std::max)(0.,size.height-y*2)};
    }
    [[nodiscard]] static ii::core::Result<Shaped> shape(const TextInput& input,const InputFieldConfiguration& options,
            ii::gui::Size extent,std::string_view inherited){
        const auto style=line_style(options,inherited);
        const auto x=(std::min)(options.horizontal_padding,(std::max)(0.,extent.width/2));
        const auto width=(std::max)(0.,extent.width-x*2-clear_reservation(options,extent));
        const bool wrapping=options.multiline&&options.wrap_enabled;
        if(wrapping&&width>0){
            auto result=NativeTextLayout::create(input.text(),style,options.password,(std::min)(8192.,width));
            if(result)return Shaped{std::move(*result),false};
            if(result.error().code!=ii::core::ErrorCode::full)return std::unexpected(result.error());
        }
        auto unwrapped=NativeTextLayout::create(input.text(),style,options.password);
        if(!unwrapped)return std::unexpected(unwrapped.error());
        return Shaped{std::move(*unwrapped),wrapping};
    }
    [[nodiscard]] static ii::core::Result<Scroll> visible_scroll(const InputFieldConfiguration& options,ii::gui::Size size,
            const TextInput& input,const NativeTextLayout& layout,Scroll previous,NativeTextAffinity affinity){
        const auto area=viewport(options,size,layout);auto caret=layout.caret(input.caret(),affinity);if(!caret)return std::unexpected(caret.error());
        auto x=previous.x,y=previous.y;
        if(area.width>0){if(caret->x<x)x=caret->x;else if(caret->x>x+area.width-1)x=caret->x-area.width+1;}
        if(area.height>0){if(caret->y<y)y=caret->y;else if(caret->y+caret->height>y+area.height)y=caret->y+caret->height-area.height;}
        return Scroll{std::clamp(x,0.,(std::max)(0.,layout.width()-area.width+1)),
                      std::clamp(y,0.,(std::max)(0.,layout.height()-area.height))};
    }
    [[nodiscard]] static ii::gui::Color color(StyleColor value,bool enabled=true){
        const auto linear=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
        return {float(linear(value.red)),float(linear(value.green)),float(linear(value.blue)),float(value.alpha*(enabled?1:.5))};
    }
    [[nodiscard]] static ii::core::Result<void> update_frames(const std::shared_ptr<State>& state){
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Input field has retired");
        const auto width=(std::max)(0.,state->extent.width),height=(std::max)(0.,state->extent.height);
        if(!state->ink.is_null()){
            if(auto frame=state->ink.set_frame({0,0,float(width),float(height)});!frame)return frame;
            if(auto visible=state->ink.set_visible(state->options.surface!=InputFieldSurface::flat);!visible)return visible;
        }
        if(state->clear.is_null())return {};
        const auto size=(std::min)({Defaults::icon_size,width,height});
        const auto padding=(std::min)(state->options.horizontal_padding,(std::max)(0.,(width-size)/2));
        const auto x=(std::max)(0.,width-padding-size),y=(std::max)(0.,(height-size)/2);
        const bool visible=clear_reservation(state->options,state->extent)>0&&state->effective_enabled&&
            state->options.enabled&&!state->options.read_only&&!state->input->text().empty();
        if(auto frame=state->clear.set_frame({float(x),float(y),float(size),float(size)});!frame)return frame;
        return state->clear.set_visible(visible);
    }
    [[nodiscard]] static std::optional<ii::gui::Rect> clip(ii::gui::Rect r,Viewport bounds){
        const auto left=(std::max)(double(r.x),bounds.x),top=(std::max)(double(r.y),bounds.y);
        const auto right=(std::min)(double(r.x+r.width),bounds.x+bounds.width),bottom=(std::min)(double(r.y+r.height),bounds.y+bounds.height);
        if(right<=left||bottom<=top)return std::nullopt;
        return ii::gui::Rect{float(left),float(top),float(right-left),float(bottom-top)};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint(
            const std::shared_ptr<State>& state,const ii::gui::WidgetPaintState& view,bool include_background=true){
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Input field has retired");Activity activity(state);
        try {
            auto drawing=std::make_shared<ii::gui::WidgetDrawing>();const auto& options=state->options;
            const auto width=float(view.size.width),height=float(view.size.height);
            if(width<=0||height<=0)return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
            const bool material_surface=options.surface!=InputFieldSurface::flat;
            const auto radius=options.surface==InputFieldSurface::inline_?0:options.surface==InputFieldSurface::cylinder?Defaults::cylinder_radius:
                material_surface?Defaults::radius:options.corner_radius;
            const auto text_ink=material_surface?color(view.enabled?Defaults::text:Defaults::disabled_text):
                color(options.palette.text,view.enabled);
            if(include_background){
                const auto fill=color(state->hovered?options.palette.hover:options.palette.background,view.enabled);
                drawing->panels.push_back({{0,0,width,height},fill,fill,float((std::min)({radius,double(width)/2,double(height)/2}))});
            }
            if(view.focused){const auto focus=color(options.palette.focus,view.enabled);drawing->panels.push_back({{0,0,width,height},focus,focus,
                float((std::min)({radius,double(width)/2,double(height)/2})),float((std::min)({1.5,double(width)/2,double(height)/2}))});}
            const auto area=viewport(options,view.size,state->layout);
            if(area.width<=0||area.height<=0)return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
            if(view.focused&&state->input->selection().length){
                auto spans=state->layout.selection(state->input->selection());if(!spans)return std::unexpected(spans.error());
                const auto selected=color(material_surface?options.palette.focus:options.palette.selection,view.enabled);
                for(const auto& span:*spans){ii::gui::Rect rectangle{float(area.x+span.x-state->scroll_x),float(area.y+span.y-state->scroll_y),float(span.width),float(span.height)};
                    if(auto bounded=clip(rectangle,area))drawing->panels.push_back({*bounded,selected,selected,0});}
            }
            if(view.focused&&state->input->marked()){
                auto spans=state->layout.selection(*state->input->marked());if(!spans)return std::unexpected(spans.error());
                const auto marked=color(options.palette.focus,view.enabled);
                for(const auto& span:*spans){ii::gui::Rect rectangle{float(area.x+span.x-state->scroll_x),float(area.y+span.y+span.height-1-state->scroll_y),float(span.width),1};
                    if(auto bounded=clip(rectangle,area))drawing->panels.push_back({*bounded,marked,marked,0});}
            }
            if(view.focused&&!options.read_only&&(!state->input->marked()||!state->composition_cursor_hidden)){auto caret=state->layout.caret(state->input->caret(),state->affinity);if(!caret)return std::unexpected(caret.error());
                ii::gui::Rect rectangle{float(area.x+caret->x-state->scroll_x),float(area.y+caret->y-state->scroll_y),1,float(caret->height)};
                if(auto bounded=clip(rectangle,area)){const auto ink=color(material_surface?Defaults::description:options.palette.caret,view.enabled);drawing->panels.push_back({*bounded,ink,ink,0});}}
            if(state->input->text().empty()&&!options.placeholder.empty()){
                const auto line_height=state->layout.lines().front().height;
                ii::gui::TextRun run{{float(area.x),float(area.y),float(area.width),float((std::min)(area.height,line_height))},options.placeholder,
                    material_surface?text_ink:color(options.palette.placeholder,view.enabled),float(options.font_size),400,ii::gui::TextAlignment::left,options.font_family.empty()?state->inherited_font:options.font_family};
                run.line_height=float(options.line_height);run.clip=ii::gui::Rect{float(area.x),float(area.y),float(area.width),float(area.height)};
                drawing->texts.push_back(std::move(run));
            }else{
                const auto ink=text_ink;
                for(std::size_t i=0;i<state->layout.lines().size();++i){
                    const auto& line=state->layout.lines()[i];const auto y=area.y+line.y-state->scroll_y;
                    const auto top=(std::max)(y,area.y),bottom=(std::min)(y+line.height,area.y+area.height);
                    if(bottom<=top||state->layout.display_text(i).empty())continue;
                    auto bitmap=state->layout.render_line(i,state->scroll_x,area.width,ink,view.backing_scale);if(!bitmap)return std::unexpected(bitmap.error());
                    auto image=ii::gui::rgba_image(bitmap->width,bitmap->height,std::move(bitmap->rgba));if(!image)return std::unexpected(image.error());
                    ii::gui::ImageRun run;run.bounds={float(area.x),float(top),float(area.width),float(bottom-top)};run.image=std::move(*image);
                    run.source={0,float((top-y)/line.height),1,float((bottom-top)/line.height)};drawing->images.push_back(std::move(run));
                }
            }
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Input field paint failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<NativeTextHit> position(const State& state,double x,double y){
        const auto area=viewport(state.options,state.extent,state.layout);
        return state.layout.hit_test_position(x-area.x+state.scroll_x,y-area.y+state.scroll_y);
    }
    [[nodiscard]] static ii::core::Result<ii::gui::TextInputState> text_state(const State& state){
        const auto area=viewport(state.options,state.extent,state.layout);auto caret=state.layout.caret(state.input->caret(),state.affinity);
        if(!caret)return std::unexpected(caret.error());
        const auto selected=state.input->selection();const auto marked=state.input->marked();
        ii::gui::TextInputState value;value.length=state.input->length();value.selection={selected.location,selected.length};
#if defined(IIGUI_DIRECTIONAL_TEXT_SELECTION_API)
        value.selection_reversed=state.input->anchor()>state.input->caret();
#endif
        if(marked)value.marked=ii::gui::TextRange{marked->location,marked->length};
#if defined(IIGUI_TEXT_REVISION_API)
        value.revision=state.revision;
#endif
        const ii::gui::Rect rectangle{float(area.x+caret->x-state.scroll_x),float(area.y+caret->y-state.scroll_y),1,float(caret->height)};
        value.caret=clip(rectangle,area).value_or(ii::gui::Rect{float(area.x),float(area.y),0,0});
        value.read_only=state.options.read_only;value.multiline=state.options.multiline;value.password=state.options.password;return value;
    }
#if defined(IIGUI_TEXT_GEOMETRY_API)
    [[nodiscard]] static ii::core::Result<ii::gui::TextInputGeometry> text_geometry(const State& state,ii::gui::TextRange range){
        if(range.location>state.input->length()||range.length>state.input->length()-range.location)
            return ii::core::fail(ii::core::ErrorCode::out_of_range,"Input field geometry exceeds the document");
        const auto area=viewport(state.options,state.extent,state.layout);
        auto first=state.layout.caret(range.location),last=state.layout.caret(range.location+range.length);
        if(!first)return std::unexpected(first.error());if(!last)return std::unexpected(last.error());
        const auto rect=[&](NativeTextCaret value){
            return clip({float(area.x+value.x-state.scroll_x),float(area.y+value.y-state.scroll_y),1,float(value.height)},area)
                .value_or(ii::gui::Rect{float(area.x),float(area.y),0,0});
        };
        ii::gui::TextInputGeometry result{rect(*first),rect(*last),{}};
        if(range.length){
            auto spans=state.layout.selection({range.location,range.length});if(!spans)return std::unexpected(spans.error());
            for(const auto& span:*spans){
                const ii::gui::Rect value{float(area.x+span.x-state.scroll_x),float(area.y+span.y-state.scroll_y),float(span.width),float(span.height)};
                if(auto visible=clip(value,area))result.selection.push_back(*visible);
                if(result.selection.size()>4096)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Input geometry exceeds its visible span limit");
            }
        }
        return result;
    }
#endif
    [[nodiscard]] static ii::core::Result<void> reveal_caret(const std::shared_ptr<State>& state){
#if defined(IIGUI_REVEAL_LOCAL_RECT_API)
        if(!state->options.reveal_caret_in_ancestors)return {};
        auto owner=state->root.snapshot();if(!owner)return std::unexpected(owner.error());
        if(!owner->focused)return {};
        auto current=text_state(*state);if(!current)return std::unexpected(current.error());
        if(current->caret.width<=0||current->caret.height<=0)return {};
        return state->root.reveal_local_rect(current->caret);
#else
        (void)state;return {};
#endif
    }
    template<class F> [[nodiscard]] static ii::core::Result<void> mutate(const std::shared_ptr<State>& state,F&& operation,bool emit_edit,
            std::optional<NativeTextAffinity> requested_affinity=std::nullopt,bool system_initiated=false,
            std::optional<bool> composition_hidden=std::nullopt){
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Input field has retired");Activity activity(state);
        try {
            auto next=*state->input;auto result=operation(next);if(!result)return std::unexpected(input_error(result.error()));
            const bool text_changed=next.text()!=state->input->text();
            const bool committed_changed=emit_edit&&next.committed_text()!=state->input->committed_text();
            auto shaped=state->layout;auto fallback=state->wrapping_fallback;
            if(text_changed){auto layout=shape(next,state->options,state->extent,state->inherited_font);
                if(!layout)return std::unexpected(layout.error());shaped=std::move(layout->layout);fallback=layout->wrapping_fallback;}
            auto affinity=requested_affinity.value_or(text_changed||next.caret()!=state->input->caret()?NativeTextAffinity::downstream:state->affinity);
            if(!shaped.soft_boundary(next.caret()))affinity=NativeTextAffinity::downstream;
            const bool hidden=next.marked()&&composition_hidden.value_or(state->composition_cursor_hidden);
            const bool changed=text_changed||next.anchor()!=state->input->anchor()||next.caret()!=state->input->caret()||
                next.marked()!=state->input->marked()||affinity!=state->affinity||hidden!=state->composition_cursor_hidden;
            if(!changed&&!committed_changed)return {};
            auto scroll=visible_scroll(state->options,state->extent,next,shaped,{state->scroll_x,state->scroll_y},affinity);if(!scroll)return std::unexpected(scroll.error());
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Input field revision exhausted");
#if defined(IIGUI_TEXT_CHANGE_SCOPE_API)
            auto previous=state->root.snapshot();if(!previous)return std::unexpected(previous.error());
            auto started=state->root.begin_text_input_change(text_changed,system_initiated);if(!started)return std::unexpected(started.error());
            auto notice=std::move(*started);auto latest=state->root.snapshot();if(!latest)return std::unexpected(latest.error());
            if(previous->text_revision!=latest->text_revision)return ii::core::fail(ii::core::ErrorCode::busy,"Input changed during text notification");
#else
            (void)system_initiated;
#endif
            const auto revision=++state->revision;state->input=std::move(next);state->layout=std::move(shaped);
            state->scroll_x=scroll->x;state->scroll_y=scroll->y;state->affinity=affinity;state->wrapping_fallback=fallback;state->composition_cursor_hidden=hidden;state->layout_failure.reset();
            if(text_changed){state->press.reset();state->last_click.reset();state->word_drag.reset();}
            if(text_changed)if(auto updated=update_frames(state);!updated)return updated;
            if(auto published=state->root.notify_text_input_changed(text_changed||committed_changed);!published)return published;
            if(auto revealed=reveal_caret(state);!revealed)return revealed;
            if(state->revision==revision&&text_changed&&state->callbacks.displayed_changed){
                auto callback=state->callbacks.displayed_changed;callback(state->input->text());
                if(!state->root.snapshot())return {};
            }
            if(state->revision==revision&&committed_changed&&state->callbacks.edited){auto callback=state->callbacks.edited;callback(state->input->committed_text());}
            if(!state->root.snapshot())return {};
            return publish_content_height(state);
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Input field update failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<bool> input(const std::shared_ptr<State>& state,const ii::gui::WidgetInput& event){
        using Kind=ii::gui::WidgetInputKind;using Key=ii::gui::WidgetKey;
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Input field requires its owner thread");
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Input field has retired");Activity activity(state);
        try {
#if defined(IIGUI_TEXT_SERVICE_INPUT_API)
            const bool system=event.text_service;
#else
            constexpr bool system=false;
#endif
            if(event.kind==Kind::focus_lost
#if defined(IIGUI_POINTER_CANCEL_API)
               ||event.kind==Kind::pointer_cancel
#endif
            ){state->dragging=false;state->hovered=false;state->press.reset();state->last_click.reset();state->word_drag.reset();return false;}
            if(!state->options.enabled)return false;
            if(event.kind==Kind::scroll && state->options.multiline){
                if(!std::isfinite(event.scroll_x)||!std::isfinite(event.scroll_y))
                    return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Text scroll must be finite");
                const auto area=viewport(state->options,state->extent,state->layout);
                const auto direction=event.device==ii::gui::PointerDevice::touch?-1.:1.;
                const auto x=std::clamp(state->scroll_x+direction*event.scroll_x,0.,(std::max)(0.,state->layout.width()-area.width+1));
                const auto y=std::clamp(state->scroll_y+direction*event.scroll_y,0.,(std::max)(0.,state->layout.height()-area.height));
                if(x==state->scroll_x && y==state->scroll_y)return false;
                state->scroll_x=x;state->scroll_y=y;
                if(auto r=state->root.notify_text_input_changed(false);!r)return std::unexpected(r.error());
                if(auto r=state->root.request_repaint();!r)return std::unexpected(r.error());
                return true;
            }
#if defined(IIGUI_TEXT_SELECTION_API)
            if(event.kind==Kind::text_selection){
                if(!event.replacement_range||event.replacement_range->length>(std::numeric_limits<std::size_t>::max)()-event.replacement_range->location)
                    return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Text selection requires a valid UTF-16 range");
                const auto range=*event.replacement_range;
                auto anchor=range.location,caret=range.location+range.length;
#if defined(IIGUI_DIRECTIONAL_TEXT_SELECTION_API)
                if(event.selection_reversed)std::swap(anchor,caret);
#endif
                auto result=mutate(state,[&](TextInput& next){return next.set_selection(anchor,caret);},false,std::nullopt,system);
                if(!result)return std::unexpected(result.error());state->preferred_x.reset();return true;
            }
#endif
            if(event.kind==Kind::pointer_leave){state->hovered=false;if(auto r=state->root.request_repaint();!r)return std::unexpected(r.error());return false;}
            if(event.kind==Kind::pointer_move&&!state->dragging){state->hovered=true;if(auto r=state->root.request_repaint();!r)return std::unexpected(r.error());return false;}
            // The text handler is also an ancestor interceptor. Let presses on
            // the child clear action reach that action instead of selecting text.
            if(event.kind==Kind::pointer_down&&!state->clear_id.empty()&&
               (event.target==state->clear_id||event.target.ends_with("/"+state->clear_id)))return false;
            if(event.kind==Kind::pointer_down||(state->dragging&&(event.kind==Kind::pointer_move||event.kind==Kind::pointer_up))){
                const auto revision=state->revision;
                if(event.kind==Kind::pointer_down){if(auto focused=state->root.focus();!focused)return std::unexpected(focused.error());}
                if(state->revision!=revision)return true;
                if(!event.local_position)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Input field needs local pointer coordinates");
                auto where=position(*state,event.local_position->x,event.local_position->y);if(!where)return std::unexpected(where.error());
                const auto x=event.local_position->x,y=event.local_position->y;
                if(event.kind==Kind::pointer_down){
                    const bool timestamp=std::isfinite(event.timestamp)&&event.timestamp>0;
                    const auto& previous=state->last_click;
                    const bool same_device=previous&&previous->device==event.device&&
                        (event.device==ii::gui::PointerDevice::touch||previous->device_id==event.device_id);
                    const bool native=state->options.use_native_click_count&&event.native_click_count!=0;
                    const bool classified=native?event.native_click_count==2:
                        timestamp&&same_device&&event.timestamp>=previous->time&&
                        event.timestamp-previous->time<=state->options.double_click_interval&&
                        std::hypot(x-previous->x,y-previous->y)<=state->options.double_click_distance;
                    const bool double_click=same_device&&!event.shift&&state->options.double_click_interval>0&&classified;
                    state->last_click.reset();state->word_drag.reset();state->press.reset();
                    if(timestamp)state->press=typename State::Click{event.timestamp,x,y,event.device,event.device_id,false,native};
                    if(double_click){
                        if(state->options.password)state->word_drag=TextInputRange{0,state->input->length()};
                        else {
                            auto word=state->input->word_range(where->position);if(!word)return std::unexpected(input_error(word.error()));
                            state->word_drag=*word;
                        }
                    }
                }else if(state->press&&std::hypot(x-state->press->x,y-state->press->y)>state->options.double_click_distance)
                    state->press->moved=true;
                auto anchor=event.kind==Kind::pointer_down&&!event.shift?where->position:state->input->anchor();
                auto caret=where->position;
                if(state->word_drag){
                    const auto original=*state->word_drag;
                    anchor=original.location;caret=original.location+original.length;
                    if(where->position<original.location||where->position>original.location+original.length){
                        auto word=state->input->word_range(where->position);if(!word)return std::unexpected(input_error(word.error()));
                        if(where->position<original.location){anchor=original.location+original.length;caret=word->location;}
                        else caret=word->location+word->length;
                    }
                }
                state->dragging=event.kind!=Kind::pointer_up;state->hovered=true;state->preferred_x.reset();
                const auto affinity=caret==where->position?where->affinity:NativeTextAffinity::downstream;
                if(auto r=mutate(state,[&](TextInput& next){return next.set_selection(anchor,caret);},false,affinity);!r)return std::unexpected(r.error());
                if(event.kind==Kind::pointer_up){
                    const bool valid_release=state->press&&!state->press->moved&&!state->word_drag&&
                        std::isfinite(event.timestamp)&&event.timestamp>=state->press->time&&
                        (state->press->native_sequence||event.timestamp-state->press->time<=state->options.double_click_interval);
                    state->last_click=valid_release?state->press:std::nullopt;
                    state->press.reset();state->word_drag.reset();
                }
                return true;
            }
            state->last_click.reset();
#if defined(IIGUI_TEXT_COMPOSITION_REGION_API)
            if(event.kind==Kind::text_composition_region){
                if(state->options.read_only)return true;
                if(!event.replacement_range)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Composition region requires a UTF-16 range");
                const auto range=*event.replacement_range;
                auto r=mutate(state,[&](TextInput& next){return next.set_composition_region({range.location,range.length});},true,std::nullopt,system);
                if(!r)return std::unexpected(r.error());return true;
            }
#endif
#if defined(IIGUI_TEXT_SURROUNDING_DELETE_API)
            if(event.kind==Kind::text_surrounding_delete){
                if(state->options.read_only)return true;
                if(!event.replacement_range)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Surrounding deletion requires a UTF-16 range");
                const auto range=*event.replacement_range;
                auto result=mutate(state,[&](TextInput& next){return next.erase_surrounding({range.location,range.length});},true,std::nullopt,system);
                if(!result)return std::unexpected(result.error());state->preferred_x.reset();return true;
            }
#endif
            if(event.kind==Kind::composition){
                if(state->options.read_only)return true;
                const auto range=event.replacement_range?std::optional<TextInputRange>{{event.replacement_range->location,event.replacement_range->length}}:std::nullopt;
                const auto selection=event.composition_selection?std::optional<TextInputRange>{{event.composition_selection->location,event.composition_selection->length}}:std::nullopt;
#if defined(IIGUI_TEXT_COMPOSITION_PRESENTATION_API)
                const bool hidden=event.composition_cursor_hidden,reversed=event.selection_reversed;
#else
                constexpr bool hidden=false,reversed=false;
#endif
#if defined(IIGUI_TEXT_REPLACEMENT_CURSOR_API)
                if(event.text_cursor_offset){
                    auto r=mutate(state,[&](TextInput& next)->TextInputResult<bool>{
                        auto changed=next.set_composition(event.text,range,selection,reversed);if(!changed)return std::unexpected(changed.error());
                        const auto marked=*next.marked();
                        const auto caret=Policy::replacement_cursor(marked.location,marked.location+marked.length,next.length(),*event.text_cursor_offset);
                        auto moved=next.set_selection(caret,caret);if(!moved)return std::unexpected(moved.error());return *changed||*moved;
                    },false,std::nullopt,system,hidden);
                    if(!r)return std::unexpected(r.error());state->preferred_x.reset();return true;
                }
#endif
                auto r=mutate(state,[&](TextInput& next){return event.text.empty()?next.cancel_composition():next.set_composition(event.text,range,selection,reversed);},false,std::nullopt,system,hidden);
                if(!r)return std::unexpected(r.error());return true;
            }
            if(event.kind==Kind::commit_composition){
                auto r=mutate(state,[](TextInput& next){return next.commit_composition();},true,std::nullopt,system);
                if(!r)return std::unexpected(r.error());return true;
            }
            if(event.kind==Kind::text){
                if(state->options.read_only)return true;
                const auto range=event.replacement_range?std::optional<TextInputRange>{{event.replacement_range->location,event.replacement_range->length}}:std::nullopt;
                if(event.content&&event.content->text.empty())return true;
                const auto& text=event.content?event.content->text:event.text;
#if defined(IIGUI_TEXT_REPLACEMENT_SELECTION_API)
                if(event.text_selection_after){
                    auto r=mutate(state,[&](TextInput& next)->TextInputResult<bool>{
                        auto changed=next.insert(text,range);if(!changed)return std::unexpected(changed.error());
                        const auto selected=*event.text_selection_after;
                        const auto end=selected.location+selected.length;
                        auto moved=next.set_selection(event.selection_reversed?end:selected.location,
                                                      event.selection_reversed?selected.location:end);
                        if(!moved)return std::unexpected(moved.error());return *changed||*moved;
                    },true,std::nullopt,system);
                    if(!r)return std::unexpected(r.error());state->preferred_x.reset();return true;
                }
#endif
#if defined(IIGUI_TEXT_REPLACEMENT_CURSOR_API)
                if(event.text_cursor_offset){
                    auto r=mutate(state,[&](TextInput& next)->TextInputResult<bool>{
                        const auto target=range.value_or(next.marked().value_or(next.selection()));
                        auto changed=next.insert(text,range);if(!changed)return std::unexpected(changed.error());
                        const auto caret=Policy::replacement_cursor(target.location,next.caret(),next.length(),*event.text_cursor_offset);
                        auto moved=next.set_selection(caret,caret);if(!moved)return std::unexpected(moved.error());return *changed||*moved;
                    },true,std::nullopt,system);
                    if(!r)return std::unexpected(r.error());state->preferred_x.reset();return true;
                }
#endif
                auto r=mutate(state,[&](TextInput& next){return next.insert(text,range);},true,std::nullopt,system);
                if(!r)return std::unexpected(r.error());state->preferred_x.reset();return true;
            }
            if(event.kind!=Kind::key_down)return false;
            const bool shortcut=(event.control||event.meta)&&!event.alt&&event.key==Key::character;
            const bool undo=event.key==Key::undo||(shortcut&&(event.text=="z"||event.text=="Z")&&!event.shift);
            const bool redo=event.key==Key::redo||(shortcut&&(((event.text=="z"||event.text=="Z")&&event.shift)||
                                                               (event.text=="y"||event.text=="Y")));
            if(undo||redo){
                if(state->options.read_only)return true;
                state->preferred_x.reset();
                auto r=mutate(state,[&](TextInput& next){return undo?next.undo():next.redo();},true);
                if(!r)return std::unexpected(r.error());return true;
            }
            if(event.key==Key::copy||event.key==Key::cut){
                if(state->options.password||!state->input->selection().length)return true;
                auto selected=state->input->read(state->input->selection());if(!selected)return std::unexpected(input_error(selected.error()));
                const auto revision=state->revision;
                auto write=state->clipboard.write;
                auto copied=write?write(std::move(*selected)):
                    ii::gui::write_clipboard({.text=std::move(*selected)});
                if(!copied)return std::unexpected(copied.error());
                if(state->revision!=revision||event.key==Key::copy||state->options.read_only)return true;
                auto r=mutate(state,[](TextInput& next){return next.insert({});},true);if(!r)return std::unexpected(r.error());return true;
            }
            if(event.key==Key::enter&&(!state->options.multiline||event.control||event.meta)){
                if(state->callbacks.submitted){auto callback=state->callbacks.submitted;callback();}return true;
            }
            if(event.key==Key::select_all){state->preferred_x.reset();auto r=mutate(state,[](TextInput& next){return next.set_selection(0,next.length());},false);if(!r)return std::unexpected(r.error());return true;}
            if(event.key==Key::left||event.key==Key::right){
                state->preferred_x.reset();const bool right=event.key==Key::right;
                if(!event.shift&&!event.alt&&!event.control&&!event.meta&&!state->input->selection().length&&
                   state->layout.soft_boundary(state->input->caret())&&
                   ((right&&state->affinity==NativeTextAffinity::upstream)||(!right&&state->affinity==NativeTextAffinity::downstream))){
                    auto r=mutate(state,[](TextInput&){return TextInputResult<bool>{false};},false,
                        right?NativeTextAffinity::downstream:NativeTextAffinity::upstream);
                    if(!r)return std::unexpected(r.error());return true;
                }
                auto r=mutate(state,[&](TextInput& next){return next.move(right?1:-1,event.shift,event.alt||event.control);},false,
                    right?NativeTextAffinity::upstream:NativeTextAffinity::downstream);
                if(!r)return std::unexpected(r.error());return true;
            }
            if(event.key==Key::home||event.key==Key::end){
                state->preferred_x.reset();const auto caret=state->input->caret();const auto& lines=state->layout.lines();
                auto index=state->layout.line_at(caret,state->affinity);if(!index)return std::unexpected(index.error());
                const auto& line=lines[*index];
                const auto target=(event.control||event.meta)?(event.key==Key::home?std::size_t(0):state->input->length()):(event.key==Key::home?line.first:line.last);
                const auto affinity=event.key==Key::end?NativeTextAffinity::upstream:NativeTextAffinity::downstream;
                auto r=mutate(state,[&](TextInput& next){return next.set_selection(event.shift?next.anchor():target,target);},false,affinity);if(!r)return std::unexpected(r.error());return true;
            }
            if(event.key==Key::up||event.key==Key::down){
                auto caret=state->layout.caret(state->input->caret(),state->affinity);if(!caret)return std::unexpected(caret.error());
                if(!state->preferred_x)state->preferred_x=caret->x;
                auto target=state->layout.hit_test_position(*state->preferred_x,event.key==Key::up?caret->y-1:caret->y+caret->height+1);
                if(!target)return std::unexpected(target.error());
                auto r=mutate(state,[&](TextInput& next){return next.set_selection(event.shift?next.anchor():target->position,target->position);},false,target->affinity);if(!r)return std::unexpected(r.error());return true;
            }
            if(event.key==Key::backspace||event.key==Key::delete_forward||event.key==Key::enter){
                if(state->options.read_only)return true;state->preferred_x.reset();
                auto r=mutate(state,[&](TextInput& next)->TextInputResult<bool>{
                    if(event.key==Key::backspace||event.key==Key::delete_forward)
                        return next.erase(event.key==Key::backspace?-1:1,event.alt||event.control,true);
                    return next.insert("\n",next.marked()?std::optional<TextInputRange>{next.selection()}:std::nullopt);
                },true);if(!r)return std::unexpected(r.error());return true;
            }
            return false;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Input field event failed",std::current_exception());}
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeInputField> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,InputFieldConfiguration options={},NativeInputFieldCallbacks callbacks={},NativeInputFieldClipboard clipboard={}){
        if(!Policy::valid_configuration(options))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Input field configuration");
        try {
            auto appearance=builder.appearance();if(!appearance)return std::unexpected(appearance.error());
            const auto inherited_font=appearance->font_family;
            auto input=TextInput::create(Policy::template boundary<ii::gui::TextIndex>,options.text);if(!input)return std::unexpected(input_error(input.error()));
            if(auto limited=input->set_maximum_length(options.maximum_length,options.length_unit);!limited)
                return std::unexpected(input_error(limited.error()));
            if(options.read_only&&options.multiline){
                auto beginning=input->set_selection(0,0);if(!beginning)return std::unexpected(input_error(beginning.error()));
            }
            input->set_history_enabled(!options.password);
            auto layout=shape(*input,options,{options.width,options.height},inherited_font);if(!layout)return std::unexpected(layout.error());
            const auto fallback=layout->wrapping_fallback;
            auto state=std::make_shared<State>(std::move(options),std::move(*input),std::move(layout->layout),std::move(callbacks),std::move(clipboard),inherited_font);
            Activity activity(state);state->wrapping_fallback=fallback;
            auto root=builder.absolute_stack(parent,id,{state->options.width,state->options.height});if(!root)return std::unexpected(root.error());state->root=*root;
            if(auto r=root->set_input_handler([state](const auto& event){return BasicNativeInputField::input(state,event);});!r)return std::unexpected(r.error());
            auto provider=std::make_shared<ii::gui::TextInputProvider>();std::weak_ptr<State> weak=state;
            provider->state=[weak]()->ii::core::Result<ii::gui::TextInputState>{auto state=weak.lock();if(!state||state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Input field has retired");Activity activity(state);return text_state(*state);};
            provider->read=[weak](ii::gui::TextRange range)->ii::core::Result<ii::gui::TextContent>{auto state=weak.lock();if(!state||state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Input field has retired");Activity activity(state);
                auto text=state->input->read({range.location,range.length});if(!text)return std::unexpected(input_error(text.error()));return ii::gui::TextContent{range,std::move(*text)};};
            provider->position=[weak](double x,double y)->ii::core::Result<std::size_t>{auto state=weak.lock();if(!state||state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Input field has retired");Activity activity(state);
                auto hit=position(*state,x,y);if(!hit)return std::unexpected(hit.error());return hit->position;};
            provider->focus_changed=[weak](bool focused)->ii::core::Result<void>{
                auto state=weak.lock();if(!state||state->retired)return {};Activity activity(state);state->dragging=false;
                if(!focused&&state->input->marked()){
                    if(auto result=mutate(state,[](TextInput& next){return next.commit_composition();},true);!result)return result;
                    if(!state->root.snapshot())return {};
                }
                if(focused)if(auto revealed=reveal_caret(state);!revealed)return revealed;
                if(state->callbacks.focus_changed){auto callback=state->callbacks.focus_changed;callback(focused);}
                return {};
            };
            provider->set_text=[weak](std::string_view text)->ii::core::Result<void>{auto state=weak.lock();
                if(!state||state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Input field has retired");
                return BasicNativeInputField(std::move(state)).set_text(std::string(text));};
            if(auto r=root->set_text_input_provider(std::move(provider));!r)return std::unexpected(r.error());
#if defined(IIGUI_TEXT_GEOMETRY_API)
            if(auto r=root->set_text_geometry_provider([weak](ii::gui::TextRange range)->ii::core::Result<ii::gui::TextInputGeometry>{auto state=weak.lock();
                if(!state||state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Input field has retired");Activity activity(state);return text_geometry(*state,range);});!r)return std::unexpected(r.error());
#endif
            if(auto r=root->set_accessibility_name(state->options.accessibility_name);!r)return std::unexpected(r.error());
            if(auto r=root->set_enabled(state->options.enabled);!r)return std::unexpected(r.error());
            auto ink=builder.absolute_stack(*root,std::string(id)+".ink",{state->options.width,state->options.height});
            if(!ink)return std::unexpected(ink.error());state->ink=*ink;
            if(auto r=ink->set_paint_handler([state](const auto& value){return paint(state,value,false);},*root);!r)return std::unexpected(r.error());
            state->clear_id=std::string(id)+".clear";
            auto clear=builder.action_surface(*root,state->clear_id,
                {.accessibility_name=state->options.clear_accessibility_name,.focus_ring=false,
                 .width=Defaults::icon_size,.height=Defaults::icon_size,.pointer_focusable=false},
                [weak]{if(auto state=weak.lock()){
                    auto result=BasicNativeInputField(state).clear_as_user();
                    if(!result)throw std::runtime_error(result.error().message);
                    if(state->root.snapshot())if(auto focused=state->root.focus();!focused)throw std::runtime_error(focused.error().message);
                }});
            if(!clear)return std::unexpected(clear.error());state->clear=*clear;
            if(auto r=clear->set_paint_handler([weak](const ii::gui::WidgetPaintState& paint)
                    ->ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>{
                auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Input field has retired");
#if defined(__APPLE__)
                constexpr bool system_symbol=true;
#else
                constexpr bool system_symbol=false;
#endif
                return ii::gui::input_field_clear_drawing(paint,Defaults::description,Defaults::background,
                    Defaults::font_size,Defaults::clear_line_height,
                    state->options.font_family.empty()?state->inherited_font:state->options.font_family,system_symbol);
            });!r)return std::unexpected(r.error());
            if(auto r=root->set_interaction_handler([weak](const ii::gui::WidgetInteraction& interaction){
                if(auto state=weak.lock();state&&interaction.alive&&state->effective_enabled!=interaction.enabled){
                    state->effective_enabled=interaction.enabled;
                    if(auto updated=update_frames(state);!updated)state->layout_failure=updated.error();
                }
            });!r)return std::unexpected(r.error());
            if(auto r=root->set_layout_handler([weak](ii::gui::Rect rect){if(auto state=weak.lock();state&&!state->retired){
                Activity activity(state);const ii::gui::Size extent{rect.width,rect.height};
                if(state->options.multiline&&state->options.wrap_enabled&&extent.width!=state->extent.width){
                    auto shaped=shape(*state->input,state->options,extent,state->inherited_font);
                    if(shaped){state->layout=std::move(shaped->layout);state->wrapping_fallback=shaped->wrapping_fallback;state->layout_failure.reset();
                        if(!state->layout.soft_boundary(state->input->caret()))state->affinity=NativeTextAffinity::downstream;}
                    else state->layout_failure=shaped.error();
                }
                state->extent=extent;auto scroll=visible_scroll(state->options,state->extent,*state->input,state->layout,{state->scroll_x,state->scroll_y},state->affinity);
                if(scroll){state->scroll_x=scroll->x;state->scroll_y=scroll->y;}
                if(auto updated=update_frames(state);!updated)state->layout_failure=updated.error();
                if(state->root.snapshot())if(auto published=publish_content_height(state);!published)state->layout_failure=published.error();}});!r)return std::unexpected(r.error());
            if(auto r=root->set_paint_handler([state](const auto& value){
                if(state->retired)return ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>(ii::core::fail(ii::core::ErrorCode::closed,"Input field has retired"));
                Activity activity(state);if(state->options.surface!=InputFieldSurface::flat){
                    const bool inline_surface=state->options.surface==InputFieldSurface::inline_;
                    const auto radius=inline_surface?0:state->options.surface==InputFieldSurface::cylinder?
                        Defaults::cylinder_radius:Defaults::radius;
                    const auto variant=inline_surface?MaterialVariant::inline_:MaterialVariant::standard;
                    return state->material.paint(value,state->options.enabled,radius,variant);
                }
                return paint(state,value);
            });!r)return std::unexpected(r.error());
            if(auto retiring=builder.on_retire([weak]{if(auto state=weak.lock()){
                state->retired=true;if(state->revision!=(std::numeric_limits<std::uint64_t>::max)())++state->revision;
                state->callbacks={};state->clipboard={};state->layout_failure.reset();release_retired(*state);
            }});!retiring)return std::unexpected(retiring.error());
            if(auto r=update_frames(state);!r)return std::unexpected(r.error());
            auto scroll=visible_scroll(state->options,state->extent,*state->input,state->layout,{},state->affinity);if(!scroll)return std::unexpected(scroll.error());
            state->scroll_x=scroll->x;state->scroll_y=scroll->y;
            if(auto published=publish_content_height(state);!published)return std::unexpected(published.error());
            if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Input field retired during construction");
            return BasicNativeInputField(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Input field construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<NativeInputFieldSnapshot> snapshot()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Input field requires its owner thread");
        if(state_->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Input field has retired");Activity activity(state_);
        if(auto active=state_->root.snapshot();!active)return std::unexpected(active.error());
        if(state_->layout_failure)return std::unexpected(*state_->layout_failure);
        try{return NativeInputFieldSnapshot{state_->input->text(),state_->input->selection(),state_->input->marked(),state_->input->anchor(),state_->input->caret(),state_->scroll_x,state_->scroll_y,state_->affinity,state_->wrapping_fallback,state_->layout.height()};}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Input field snapshot failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<InputFieldConfiguration> options()const{
        auto current=snapshot();if(!current)return std::unexpected(current.error());
        try{auto value=state_->options;value.text=std::move(current->text);return value;}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Input field options failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> set_selection(std::size_t anchor,std::size_t caret)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Input field requires its owner thread");
        if(auto active=state_->root.snapshot();!active)return std::unexpected(active.error());
        auto result=mutate(state_,[&](TextInput& next){return next.set_selection(anchor,caret);},false);
        if(result)state_->last_click.reset();
        return result;
    }
private:
    [[nodiscard]] ii::core::Result<void> apply_options(InputFieldConfiguration options,bool force_text)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Input field requires its owner thread");
        if(!Policy::valid_configuration(options))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Input field configuration");
        if(state_->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Input field has retired");Activity activity(state_);
        try {
            if(auto active=state_->root.snapshot();!active)return std::unexpected(active.error());
            auto next=*state_->input;const auto incoming=options.text;
            // options() exposes the displayed draft, including IME text. A
            // caller changing another option with that snapshot must not turn
            // the draft into a new external value or commit the composition.
            const bool mirrors_current=!force_text&&state_->input->marked().has_value()&&
                incoming!=state_->last_input_text&&incoming==state_->input->text();
            // A binding may echo an edit to the committed text while a
            // separate marked candidate is still being composed.
            const bool mirrors_committed=!force_text&&state_->input->marked().has_value()&&
                incoming!=state_->last_input_text&&incoming==state_->input->committed_text();
            if(mirrors_current)options.text=state_->options.text;
            if(auto limited=next.set_maximum_length(options.maximum_length,options.length_unit);!limited)
                return std::unexpected(input_error(limited.error()));
            if(force_text||(incoming!=state_->last_input_text&&!mirrors_current&&!mirrors_committed)){
                auto changed=next.set_text(incoming);if(!changed)return std::unexpected(input_error(changed.error()));
                if(*changed&&options.read_only&&options.multiline){
                    auto beginning=next.set_selection(0,0);if(!beginning)return std::unexpected(input_error(beginning.error()));
                }
            }
            next.set_history_enabled(!options.password);
            // The parent may have assigned a different frame from the field's
            // requested extent. Shape against the last laid-out frame until
            // the host delivers a new frame through the layout handler.
            const ii::gui::Size requested_extent{options.width,options.height};const auto extent=state_->extent;
            auto shaped=shape(next,options,extent,state_->inherited_font);if(!shaped)return std::unexpected(shaped.error());
            auto affinity=next.text()==state_->input->text()?state_->affinity:NativeTextAffinity::downstream;
            if(!shaped->layout.soft_boundary(next.caret()))affinity=NativeTextAffinity::downstream;
            auto scroll=visible_scroll(options,extent,next,shaped->layout,{state_->scroll_x,state_->scroll_y},affinity);
            if(!scroll)return std::unexpected(scroll.error());
            if(state_->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Input field revision exhausted");
            const auto previous_options=state_->options;const auto previous_input=state_->input;const auto previous_layout=state_->layout;
            const auto previous_external=state_->last_input_text;const auto previous_extent=state_->extent;const auto previous_x=state_->scroll_x,previous_y=state_->scroll_y;
            const auto previous_affinity=state_->affinity;const auto previous_fallback=state_->wrapping_fallback;const auto previous_failure=state_->layout_failure;
            const bool changed_text=next.text()!=state_->input->text();
#if defined(IIGUI_TEXT_CHANGE_SCOPE_API)
            std::optional<ii::gui::TextInputChange> notice;
            const bool changed_selection=next.anchor()!=state_->input->anchor()||next.caret()!=state_->input->caret()||next.marked()!=state_->input->marked();
            if(changed_text||changed_selection){
                auto before=state_->root.snapshot();if(!before)return std::unexpected(before.error());
                auto started=state_->root.begin_text_input_change(changed_text);if(!started)return std::unexpected(started.error());
                notice.emplace(std::move(*started));auto after=state_->root.snapshot();if(!after)return std::unexpected(after.error());
                if(before->text_revision!=after->text_revision)return ii::core::fail(ii::core::ErrorCode::busy,"Input changed during text notification");
            }
#endif
            const auto revision=++state_->revision;
            state_->options=std::move(options);state_->input=std::move(next);state_->layout=std::move(shaped->layout);
            state_->last_input_text=mirrors_current?previous_external:incoming;
            state_->scroll_x=scroll->x;state_->scroll_y=scroll->y;state_->affinity=affinity;
            state_->wrapping_fallback=shaped->wrapping_fallback;state_->layout_failure.reset();
            const auto rollback=[&]{if(state_->revision==revision){state_->options=previous_options;state_->input=previous_input;state_->layout=previous_layout;
                state_->last_input_text=previous_external;state_->extent=previous_extent;state_->scroll_x=previous_x;state_->scroll_y=previous_y;
                state_->affinity=previous_affinity;state_->wrapping_fallback=previous_fallback;state_->layout_failure=previous_failure;}};
            if(auto r=state_->root.set_extent(requested_extent);!r){rollback();return r;}if(state_->revision!=revision)return {};
            const ii::gui::Size previous_requested_extent{previous_options.width,previous_options.height};
            if(auto r=state_->root.set_enabled(state_->options.enabled);!r){const bool current=state_->revision==revision;rollback();if(current)(void)state_->root.set_extent(previous_requested_extent);return r;}if(state_->revision!=revision)return {};
            if(auto r=state_->root.set_accessibility_name(state_->options.accessibility_name);!r){const bool current=state_->revision==revision;rollback();if(current){(void)state_->root.set_extent(previous_requested_extent);(void)state_->root.set_enabled(previous_options.enabled);}return r;}
            if(state_->revision!=revision)return {};
            if(auto r=state_->clear.set_accessibility_name(state_->options.clear_accessibility_name);!r){
                const bool current=state_->revision==revision;rollback();if(current){
                    (void)state_->root.set_extent(previous_requested_extent);(void)state_->root.set_enabled(previous_options.enabled);
                    (void)state_->root.set_accessibility_name(previous_options.accessibility_name);
                }return r;
            }
            if(state_->revision!=revision)return {};
            if(auto r=update_frames(state_);!r)return r;
            if(changed_text||!state_->options.enabled||state_->options.password!=previous_options.password||
               state_->options.double_click_interval!=previous_options.double_click_interval||
               state_->options.double_click_distance!=previous_options.double_click_distance){
                state_->dragging=false;state_->press.reset();state_->last_click.reset();state_->word_drag.reset();
            }
            if(auto notified=state_->root.notify_text_input_changed(changed_text);!notified)return notified;
            if(state_->revision==revision&&changed_text&&state_->callbacks.displayed_changed){
                auto callback=state_->callbacks.displayed_changed;callback(state_->input->text());
            }
            if(!state_->root.snapshot())return {};
            return publish_content_height(state_);
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Input field options failed",std::current_exception());}
    }
public:
    [[nodiscard]] ii::core::Result<void> set_options(InputFieldConfiguration options)const{return apply_options(std::move(options),false);}
    // Keep the last authored text while restyling, so an active composition or
    // local draft is not mistaken for a new external text value.
    [[nodiscard]] ii::core::Result<void> set_palette(InputFieldPalette palette)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Input field requires its owner thread");
        try{auto options=state_->options;options.palette=std::move(palette);return apply_options(std::move(options),false);}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Input field palette update failed",std::current_exception());}
    }
    // Rename the semantic element without replacing marked text or editing history.
    [[nodiscard]] ii::core::Result<void> set_accessibility_name(std::string name)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Input field requires its owner thread");
        if(!Policy::valid_text(name))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid input field accessibility name");
        try{
            if(auto active=state_->root.snapshot();!active)return std::unexpected(active.error());
            if(state_->options.accessibility_name==name)return {};
            if(state_->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Input field revision exhausted");
            auto previous=state_->options.accessibility_name;const auto revision=++state_->revision;
            state_->options.accessibility_name=std::move(name);
            try{
                auto changed=state_->root.set_accessibility_name(state_->options.accessibility_name);
                if(!changed&&state_->revision==revision)state_->options.accessibility_name=std::move(previous);
                return changed;
            }catch(...){if(state_->revision==revision)state_->options.accessibility_name=std::move(previous);throw;}
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Input field accessibility name update failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> set_text(std::string text)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Input field requires its owner thread");
        try{auto options=state_->options;options.text=std::move(text);return apply_options(std::move(options),true);}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Input field text update failed",std::current_exception());}
    }
    // An explicit clear affordance is an edit, not an external value reset:
    // it participates in local undo and resolves any marked text as one edit.
    [[nodiscard]] ii::core::Result<void> clear_as_user()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Input field requires its owner thread");
        if(auto active=state_->root.snapshot();!active)return std::unexpected(active.error());
        if(!state_->options.enabled||state_->options.read_only)return {};
        state_->preferred_x.reset();
        return mutate(state_,[](TextInput& next){return next.insert({},TextInputRange{0,next.length()});},true);
    }
};
} // namespace ii::gui
