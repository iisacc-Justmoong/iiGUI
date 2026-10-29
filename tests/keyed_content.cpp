#include "../src/iiGUI/WidgetState.h"
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace ii;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void good(core::Result<void> result) {
    if (!result) { if (result.error().cause) std::rethrow_exception(result.error().cause); throw std::runtime_error(result.error().message); }
}
template<class T> T take(core::Result<T> result) {
    if (!result) { if (result.error().cause) std::rethrow_exception(result.error().cause); throw std::runtime_error(result.error().message); }
    return std::move(*result);
}
struct Counters final { int created=0,retired=0,destroyed=0,actions=0; };
struct Owner final {
    Counters& counts;
    explicit Owner(Counters& value) : counts(value) { ++counts.created; }
    ~Owner() { ++counts.destroyed; }
};
gui::KeyedContentItem row(std::string key, Counters& counts, bool fail=false) {
    return {std::move(key), [&counts,fail](gui::WidgetBuilder& builder) -> core::Result<void> {
        auto owner=std::make_shared<Owner>(counts);
        if(auto r=builder.retain(owner);!r)return r;
        if(auto r=builder.on_retire([&counts]{++counts.retired;});!r)return r;
        auto root=builder.vstack({},"row",{.spacing=0,.minimum_height=48});
        if(!root)return std::unexpected(root.error());
        auto text=builder.input_field(*root,"input",{.text="Initial"});
        if(!text)return std::unexpected(text.error());
        auto action=builder.button(*root,"action",{.text="Action"},[&counts]{++counts.actions;});
        if(!action)return std::unexpected(action.error());
        if(fail)return core::fail(core::ErrorCode::failed,"Requested row failure");
        return {};
    }};
}
void identity() {
    Counters counts;
    auto state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    auto host=take(state->content_host({},"list",{}));
    good(host.set_keyed_content({row("A",counts),row("B",counts),row("C",counts)}, {.layout={.spacing=2}}));
    auto b=take(host.keyed_content_item("B","input"));
    const auto b_id=take(b.snapshot()).id;
    good(b.set_text("Edited")); good(b.focus());
    const auto epoch=take(host.snapshot()).content_generation;
    good(host.set_keyed_content({row("A",counts),row("B",counts),row("C",counts)}, {.layout={.spacing=2}}));
    check(counts.created==3 && take(host.snapshot()).content_generation==epoch,"Unchanged keys rebuilt their collection");
    good(host.set_keyed_content({row("C",counts),row("B",counts),row("D",counts)}, {.layout={.spacing=2}}));
    auto kept=take(host.keyed_content_item("B","input"));
    check(counts.created==4 && counts.retired==1 && counts.destroyed==1,"Key reconciliation recreated retained rows or kept removed rows");
    check(take(kept.snapshot()).id==b_id && take(kept.snapshot()).text=="Edited" && take(b.interaction()).focused,
          "Reordering lost widget identity, local editing or focus");
    good(state->input({.kind=gui::WidgetInputKind::text,.text="Reordered",.replacement_range=gui::TextRange{0,6}}));
    check(take(b.snapshot()).text=="Reordered","Keyboard routing did not follow the retained focused row");
    check(take(host.measure(320)).height>=148,"Reparenting a retained row left a zero-height layout cache");
    check(take(state->find(b_id)).snapshot().has_value(),"A retained qualified widget id no longer resolved after reparenting");
    check(take(host.keyed_content_keys())==std::vector<std::string>{"C","B","D"},"Key order differs from presentation order");
    good(take(host.keyed_content_item("B","action")).activate());
    check(counts.actions==1,"Retained action was disconnected");
    good(host.clear_content());
    check(!b.snapshot() && counts.retired==4 && counts.destroyed==4 && !host.keyed_content_item("B","input"),"Clearing left retained row state alive");
    good(host.set_keyed_content({row("B",counts)}));
    check(take(take(host.keyed_content_item("B","input")).snapshot()).id!=b_id&&!state->find(b_id),
          "Reactivating a removed key rebound an expired qualified id to another generation");
    state->close();
    check(counts.created==5&&counts.destroyed==5,"Reactivated key ownership was not released");
}
void rollback() {
    Counters counts;
    auto state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    auto host=take(state->content_host({},"list",{}));
    good(host.set_keyed_content({row("A",counts),row("B",counts)}));
    auto a=take(host.keyed_content_item("A","input"));
    good(a.set_text("Preserved"));
    const auto epoch=take(host.snapshot()).content_generation;
    auto failed=host.set_keyed_content({row("B",counts),row("new",counts,true)});
    check(!failed && counts.created==3 && counts.retired==1 && counts.destroyed==1,"A failed candidate was not retired exactly once");
    check(take(host.snapshot()).content_generation==epoch && take(a.snapshot()).text=="Preserved" &&
          take(host.keyed_content_keys())==std::vector<std::string>{"A","B"},"A failed row factory changed the working collection");
    check(!host.set_keyed_content({row("A",counts),row("A",counts)}),"Duplicate keys were accepted");
    check(!host.set_keyed_content({row("",counts)}),"Empty keys were accepted");
    check(counts.created==3,"Invalid keys invoked a row factory");
    bool foreign_rejected=false;
    std::thread foreign([&]{foreign_rejected=!host.set_keyed_content({});}); foreign.join();
    check(foreign_rejected,"Foreign-thread reconciliation was admitted");
    good(host.set_keyed_content({row("B",counts),row("new",counts)}));
    check(counts.created==4 && !a.snapshot(),"A valid candidate did not recover after a factory failure");
    state->close();
    check(counts.created==counts.retired && counts.retired==counts.destroyed,"Collection teardown leaked an owner");
}
void bounded() {
    Counters counts;
    auto state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    auto host=take(state->content_host({},"list",{}));
    for(int n=0;n<300;++n) {
        good(host.set_keyed_content({row("kept",counts),row(std::to_string(n),counts)}));
        check(take(state->snapshots()).size()==10,"Retired wrapper nodes accumulated in the active domain");
    }
    check(counts.created==301 && counts.destroyed==299,"Churn did not preserve the stable row");
    state->close();
    check(counts.created==counts.destroyed,"Churn leaked row activations");
}
void capacity() {
    auto state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    auto host=take(state->content_host({},"list",{}));
    int created=0;
    auto rows=[&](int count,bool reverse){
        std::vector<gui::KeyedContentItem> result;
        for(int n=0;n<count;++n){
            const auto key=std::to_string(reverse?count-1-n:n);
            result.push_back({key,[&](gui::WidgetBuilder& b)->core::Result<void>{
                ++created;auto label=b.label({},"value",{.text="Row"});
                if(!label)return std::unexpected(label.error());return {};
            }});
        }
        return result;
    };
    good(host.set_keyed_content(rows(2047,false)));
    check(created==2047&&take(state->snapshots()).size()==4096,"An exactly bounded collection was rejected");
    const auto first=take(host.keyed_content_item("0","value"));
    good(host.set_keyed_content(rows(2047,true)));
    check(created==2047&&first.snapshot().has_value(),"Reordering at capacity duplicated retained row nodes");
    const auto epoch=take(host.snapshot()).content_generation;
    check(!host.set_keyed_content(rows(2048,false)),"A keyed transfer bypassed the domain node limit");
    check(first.snapshot().has_value()&&take(host.snapshot()).content_generation==epoch&&take(state->snapshots()).size()==4096,
          "A rejected capacity update damaged the current generation");
    good(host.clear_content());
    check(!first.snapshot(),"Clearing a capacity-sized collection retained a row");
    good(host.set_keyed_content(rows(2047,false)));
    check(take(state->snapshots()).size()==4096,"Reactivating at capacity leaked per-row shared observers");
    state->close();
}
void retirement(){
    Counters counts;
    auto state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    auto host=take(state->content_host({},"list",{}));
    good(host.set_keyed_content({row("a",counts)}));
    auto previous=take(host.keyed_content_item("a","input"));
    bool reentry_rejected=false;
    auto added=row("b",counts);
    auto build=std::move(added.factory);
    added.factory=[&](gui::WidgetBuilder& builder)->core::Result<void>{
        good(build(builder));
        auto reentry=host.set_keyed_content({});
        reentry_rejected=!reentry&&reentry.error().code==core::ErrorCode::busy;
        state->close();return {};
    };
    auto result=host.set_keyed_content({row("a",counts),std::move(added)});
    check(!result&&reentry_rejected&&!previous.snapshot()&&counts.created==2&&counts.retired==2&&counts.destroyed==2,
          "Owner retirement or reentrant construction leaked a candidate or admitted another publication");
}
void positioned(){
    Counters counts;
    auto state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    auto host=take(state->content_host({},"list",{320,160}));
    auto items=[&](float y){auto a=row("A",counts),b=row("B",counts);a.frame=gui::Rect{0,y,150,80};b.frame=gui::Rect{160,y,150,80};return std::vector{std::move(a),std::move(b)};};
    good(host.set_keyed_content(items(-15),{.viewport=gui::Size{320,160}}));
    auto a=take(host.keyed_content_item("A","input"));const auto identity=take(a.snapshot()).id;
    good(a.set_text("Preserved"));good(a.focus());
    const auto epoch=take(host.snapshot()).content_generation;
    good(host.set_keyed_content(items(-16),{.viewport=gui::Size{320,160}}));
    check(counts.created==2&&take(host.snapshot()).content_generation==epoch&&take(a.snapshot()).id==identity&&
          take(a.snapshot()).text=="Preserved"&&take(a.interaction()).focused,"Pixel scrolling rebuilt positioned row generations");
    const auto bounds=take(take(host.keyed_content_item("A","row")).snapshot()).bounds;
    check(bounds.y==-16&&bounds.width==150&&bounds.height==80,"Signed cell geometry did not reach the native row");
    auto invalid=items(-16);invalid[1].frame->width=-1;
    check(!host.set_keyed_content(std::move(invalid),{.viewport=gui::Size{320,160}})&&take(host.snapshot()).content_generation==epoch,
          "Invalid positioned geometry partially changed the working generation");
    check(!host.set_keyed_content(items(0)),"Positioned cells silently discarded frames in a linear layout");
    auto a_root=take(host.keyed_content_item("A","row")),b_root=take(host.keyed_content_item("B","row"));int moves=0,ups=0,foreign_moves=0;
    good(a_root.set_input_handler([&](const gui::WidgetInput& e)->core::Result<bool>{if(e.kind==gui::WidgetInputKind::pointer_move)++moves;if(e.kind==gui::WidgetInputKind::pointer_up)++ups;return e.kind==gui::WidgetInputKind::pointer_down||e.kind==gui::WidgetInputKind::pointer_move||e.kind==gui::WidgetInputKind::pointer_up;}));
    good(b_root.set_input_handler([&](const gui::WidgetInput& e)->core::Result<bool>{if(e.kind==gui::WidgetInputKind::pointer_move)++foreign_moves;return false;}));
    const auto a_bounds=take(a_root.snapshot()).bounds,b_bounds=take(b_root.snapshot()).bounds;
    good(state->input({.kind=gui::WidgetInputKind::pointer_down,.x=a_bounds.x+10,.y=a_bounds.y+50,.buttons=1}));
    good(state->input({.kind=gui::WidgetInputKind::pointer_move,.x=b_bounds.x+10,.y=b_bounds.y+50,.buttons=1}));
    good(state->input({.kind=gui::WidgetInputKind::pointer_up,.x=b_bounds.x+10,.y=b_bounds.y+50}));
    check(moves==1&&ups==1&&foreign_moves==0,"A later sibling stole an earlier row's captured gesture");
    good(state->input({.kind=gui::WidgetInputKind::pointer_move,.x=b_bounds.x+10,.y=b_bounds.y+50}));check(foreign_moves==1,"Pointer release retained the previous row's capture");
    good(host.set_keyed_content({row("B",counts),row("A",counts)}));
    check(take(a.snapshot()).id==identity&&counts.created==2,"Changing layout mode cloned keyed row owners");
    state->close();check(counts.created==counts.destroyed,"Positioned content retained retired owners");
}
void virtual_scroll(){
    auto state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    auto host=take(state->content_host({},"root",{640,480}));
    good(host.set_content([](gui::WidgetBuilder& builder)->core::Result<void>{
        auto root=take(builder.vstack({},"body",{.spacing=0}));
        auto scroll=take(builder.scroll_view(root,"scroll",{.width=320,.height=160}));
        take(builder.content_host(scroll,"content",{320,160}));return {};
    }));
    auto scroll=take(host.content_item("scroll"));
    int delivered=0;
    good(scroll.set_scroll_handler([&](const gui::ScrollViewport& view){
        ++delivered;
        good(scroll.set_virtual_scroll_content({320,40000},0,view.offset));
    }));
    auto snapshot=take(scroll.snapshot());
    check(snapshot.content_height==40000&&delivered<=2,"Virtual extent publication from a viewport callback did not settle");
    good(scroll.set_scroll_offset(210));
    check(take(scroll.snapshot()).scroll_offset==210,"Native virtual scroll position did not move");
    good(scroll.set_extent({480,240}));
    check(take(scroll.snapshot()).bounds.width==480&&take(scroll.snapshot()).content_height==40000,"Resizing a scroll viewport discarded its logical content extent");
    check(!scroll.set_virtual_scroll_content({-1,1},0,0),"Invalid logical extent was accepted");
    state->close();
}
void popup(){
    auto state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    auto host=take(state->content_host({},"list",{640,480}));
    int created=0;
    auto entry=[&](std::string key)->gui::KeyedContentItem{return {std::move(key),[&](gui::WidgetBuilder& builder)->core::Result<void>{
        ++created;
        auto root=take(builder.vstack({},"row"));
        auto menu=take(builder.menu(root,"menu",{}));
        take(builder.menu_item(menu,"item",{.label="Menu item"}));return {};
    }};};
    good(host.set_keyed_content({entry("a"),entry("b")}));
    auto menu=take(host.keyed_content_item("a","menu"));
    good(menu.open_at(10,10));
    check(take(menu.snapshot()).opened,"A row popup did not open");
    good(host.set_keyed_content({entry("b"),entry("a")}));
    check(created==2&&take(menu.snapshot()).opened&&take(host.keyed_content_item("a","menu")).snapshot()->id==take(menu.snapshot()).id,
          "Moving a row closed or recreated its open popup");
    good(menu.close_popup());
    state->close();
}
void measurement(){
    auto state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    auto host=take(state->content_host({},"root",{320,160}));
    gui::KeyedContentItem cell{"a",[](gui::WidgetBuilder& builder)->core::Result<void>{
        auto root=take(builder.vstack({},"body",{.spacing=0,.alignment=gui::StackAlignment::stretch}));
        take(builder.label(root,"text",{.text="Short",.font_size=12,.line_height=12}));return {};
    },gui::Rect{0,0,120,80}};
    good(host.set_keyed_content({cell},{.viewport=gui::Size{320,160}}));
    auto body=take(host.keyed_content_item("a","body")),text=take(host.keyed_content_item("a","text"));
    std::vector<gui::WidgetMeasurement> sizes;
    good(body.set_measure_handler([&](const auto& size){sizes.push_back(size);}));
    const auto bounds=take(body.snapshot()).bounds;
    check(sizes.size()==1&&sizes.back().available_width==120&&sizes.back().natural.height==12,"A row did not report its natural size under the allocated width");
    good(text.set_text(std::string(120,'W')));
    check(take(body.snapshot()).bounds==bounds&&sizes.size()==2&&sizes.back().natural.height>12,"Content changed inside fixed bounds without a natural-size notification");
    (void)take(body.snapshot());check(sizes.size()==2,"Unchanged natural metrics emitted another callback");
    good(body.set_measure_handler([&](const auto& size){sizes.push_back(size);}));
    (void)take(body.snapshot());check(sizes.size()==2,"Refreshing a measurement callback forced another layout notification");
    good(body.set_measure_handler({}));good(text.set_text("Short"));(void)take(body.snapshot());
    check(sizes.size()==2,"Removing a measurement callback left it subscribed");
    good(body.set_measure_handler([&](const auto&){state->close();}));
    check(!body.snapshot(),"Closing an owner during measurement did not retire the callback safely");
}
}
int main(int argc,char** argv) {
    try {
        const std::string name=argc>1?argv[1]:"identity";
        if(name=="identity")identity();else if(name=="rollback")rollback();else if(name=="bounded")bounded();else if(name=="capacity")capacity();
        else if(name=="retirement")retirement();else if(name=="popup")popup();else if(name=="positioned")positioned();else if(name=="virtual_scroll")virtual_scroll();else if(name=="measurement")measurement();
        else throw std::runtime_error("Unknown keyed content test");
        std::cout<<"Native keyed content "<<name<<" passed without a window\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
