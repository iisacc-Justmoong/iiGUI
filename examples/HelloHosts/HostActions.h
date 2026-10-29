#pragma once
#include <functional>
struct HostActions final {std::function<void()> on_close;void close(){if(on_close)on_close();}};
