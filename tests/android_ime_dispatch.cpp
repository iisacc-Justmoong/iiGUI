#include "../src/Platform/AndroidImeDispatch.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace ii;
using namespace ii::gui::detail;
using namespace std::chrono_literals;

namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Queue final {
    std::mutex mutex;std::condition_variable ready;std::vector<runtime::Task> tasks;
    bool post(runtime::Task task){std::lock_guard lock(mutex);tasks.push_back(std::move(task));ready.notify_one();return true;}
    void run(){runtime::Task task;{std::unique_lock lock(mutex);check(ready.wait_for(lock,2s,[&]{return !tasks.empty();}),"Queued request was not posted");task=std::move(tasks.front());tasks.erase(tasks.begin());}task();}
};
}
int main(){
    Queue queue;AndroidImeDispatcher dispatch([&](runtime::Task task){return queue.post(std::move(task));},200ms);
    auto owner=dispatch.call<int>([]{return core::Result<int>{7};});check(owner&&*owner==7,"Owner-thread request did not run inline");
    core::Result<int> result=core::fail(core::ErrorCode::failed,"not run");int edits=0;
    std::thread worker([&]{result=dispatch.call<int>([&]()->core::Result<int>{++edits;return 42;});});
    queue.run();worker.join();check(result&&*result==42&&edits==1,"Foreign request did not run once on its owner");
    AndroidImeDispatcher timeout([&](runtime::Task task){return queue.post(std::move(task));},20ms);
    std::thread late([&]{result=timeout.call<int>([&]()->core::Result<int>{++edits;return 1;});});late.join();
    check(!result&&result.error().code==core::ErrorCode::cancelled,"Queued request did not time out");queue.run();
    check(edits==1,"Timed-out request was applied later");
    AndroidImeDispatcher closing([&](runtime::Task task){return queue.post(std::move(task));},2s);
    std::thread waiting([&]{result=closing.call<int>([&]()->core::Result<int>{++edits;return 1;});});
    {std::unique_lock lock(queue.mutex);check(queue.ready.wait_for(lock,2s,[&]{return !queue.tasks.empty();}),"Closing request was not queued");}
    closing.close();waiting.join();queue.run();check(!result&&result.error().code==core::ErrorCode::closed&&edits==1,"A closed request was applied");
    AndroidImeDispatcher rejecting([](runtime::Task){return false;},20ms);
    std::thread rejected([&]{result=rejecting.call<int>([&]()->core::Result<int>{++edits;return 1;});});rejected.join();
    check(!result&&edits==1,"Rejected work was reported as applied");
    AndroidImeDispatcher wrong([](runtime::Task task){task();return true;},20ms);
    std::thread incorrect([&]{result=wrong.call<int>([&]()->core::Result<int>{++edits;return 1;});});incorrect.join();
    check(!result&&result.error().code==core::ErrorCode::wrong_executor&&edits==1,"Wrong-thread scheduler applied editor work");
}
