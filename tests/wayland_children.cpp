#include "../src/Platform/WaylandChildren.h"
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
struct Child { int id; bool active=false; };
void mutation_during_iteration() {
    ii::gui::detail::WaylandChildren<Child> children;
    Child a{1}, b{2}, c{3};
    children.add(&a); children.add(&b); children.add(&c);
    std::vector<int> visited;
    children.for_each([&](Child& child) {
        visited.push_back(child.id);
        if (child.id==1) { children.remove(&b); children.add(&b); }
    });
    check(visited==std::vector<int>({1,3}), "A replaced generation was visited through a stale snapshot");
    visited.clear(); children.for_each([&](Child& child){ visited.push_back(child.id); });
    check(visited==std::vector<int>({1,3,2}), "Child insertion order or replacement was lost");
    children.remove(&a); children.remove(&b); children.remove(&c);
    check(children.empty(), "Removed children remained in the host");
}
void validation_and_activity() {
    ii::gui::detail::WaylandChildren<Child> children;
    Child a{1}, b{2}; children.add(&a); children.add(&b);
    bool duplicate=false, null=false;
    try { children.add(&a); } catch(const std::invalid_argument&) { duplicate=true; }
    try { children.add(nullptr); } catch(const std::invalid_argument&) { null=true; }
    check(duplicate && null, "Invalid children were accepted");
    check(!children.any([](const Child& child){ return child.active; }), "An inactive child activated the app");
    b.active=true;
    check(children.any([](const Child& child){ return child.active; }), "Active child state was lost");
    children.remove(&b);
    check(!children.any([](const Child& child){ return child.active; }), "A removed child kept the app active");
}
} // namespace
int main() {
    try { mutation_during_iteration(); validation_and_activity(); }
    catch(const std::exception& error){ std::fprintf(stderr,"%s\n",error.what()); return 1; }
}
