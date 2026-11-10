#include <iiGUI/NativeEditorCanvasView.h>
#include <iiGUI/WidgetState.h>
#include <cmath>
#include <stdexcept>
#include <vector>
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
int main(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,100}));std::optional<NativeEditorCanvas> canvas;
    int queries=0;bool fail=false;std::vector<EditorCanvasInput> input;
    EditorCanvasOptions options{.width=200,.height=100,.transform={.scale=2,.offset_x=10,.offset_y=20},.revision=1};
    NativeEditorCanvasCallbacks callbacks;
    callbacks.query=[&](const EditorCanvasQuery& query)->ii::core::Result<std::shared_ptr<const WidgetDrawing>>{
        ++queries;if(fail)return ii::core::fail(ii::core::ErrorCode::failed,"query failed");
        if(query.transform.rotation_radians==0)check(std::abs(query.document_bounds.x+5)<.001&&std::abs(query.document_bounds.width-100)<.001,"visible document query");
        else check(std::abs(query.document_bounds.x+40)<.001&&std::abs(query.document_bounds.y+95)<.001&&
            std::abs(query.document_bounds.width-50)<.001&&std::abs(query.document_bounds.height-100)<.001,"rotated document query omitted visible corners");
        auto drawing=std::make_shared<WidgetDrawing>();drawing->panels.push_back({{0,0,20,20},{1,0,0,1},{1,0,0,1},0});
        return std::shared_ptr<const WidgetDrawing>(drawing);
    };
    callbacks.input=[&](const EditorCanvasInput& event)->ii::core::Result<void>{input.push_back(event);return {};};
    good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{auto result=NativeEditorCanvas::create(b,{},"canvas",options,callbacks);
        if(!result)return std::unexpected(result.error());canvas=*result;return {};}));
    check(queries==1,"initial query count");(void)take(tree->drawing_snapshot({200,100}));(void)take(tree->drawing_snapshot({200,100}));
    check(queries==1,"paint performed a data query");good(canvas->set_options(options));check(queries==1,"unchanged state queried again");
    ++options.revision;good(canvas->set_options(options));check(queries==2,"revision did not query");
    fail=true;++options.revision;check(!canvas->set_options(options)&&take(canvas->options()).revision==2,"failed query replaced previous state");
    fail=false;good(canvas->set_options(options));
    options.transform.rotation_radians=1.5707963267948966;options.transform.flip_x=true;good(canvas->set_options(options));
    options.transform.rotation_radians=0;options.transform.flip_x=false;good(canvas->set_options(options));
    good(tree->input({.kind=WidgetInputKind::scroll,.x=30,.y=40,.scroll_y=-12,.control=true,.precise_scroll=true,.device=PointerDevice::touch}));
    check(!input.empty()&&input.back().phase==EditorCanvasPhase::scroll&&input.back().native.scroll_y==-12&&
          std::abs(input.back().document.x-10)<.001,"canvas zoom input lost its anchor or scroll data");input.clear();
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=30,.y=40,.device=PointerDevice::pen,.pressure=.6,.device_id=7}));
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=50,.y=60,.device=PointerDevice::pen,.pressure=.7,.device_id=7}));
    check(input.size()>=2&&input.front().phase==EditorCanvasPhase::begin&&std::abs(input.front().document.x-10)<.001&&
          std::abs(input.front().document.y-10)<.001&&input.front().native.device==PointerDevice::pen,"pen input lost document position");
    options.enabled=false;good(canvas->set_options(options));check(input.back().phase==EditorCanvasPhase::cancel,"disabling did not cancel gesture");
    options.enabled=true;good(canvas->set_options(options));good(tree->input({.kind=WidgetInputKind::pointer_down,.x=30,.y=40}));
    good(host.clear_content());check(input.back().phase==EditorCanvasPhase::cancel&&!canvas->set_options(options),"retirement did not cancel");
    input.clear();callbacks.input=[&](const EditorCanvasInput& event)->ii::core::Result<void>{input.push_back(event);
        if(event.phase==EditorCanvasPhase::begin)return host.clear_content();return {};};
    good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{auto result=NativeEditorCanvas::create(b,{},"canvas",options,callbacks);
        if(!result)return std::unexpected(result.error());canvas=*result;return {};}));
    (void)take(tree->drawing_snapshot({200,100}));good(tree->input({.kind=WidgetInputKind::pointer_down,.x=30,.y=40}));
    check(input.size()==2&&input.back().phase==EditorCanvasPhase::cancel&&!canvas->options(),"reentrant retirement lost cancellation");tree->close();
}
