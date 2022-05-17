#include "../include/HPS.h"

HPS::Logger::ptr g_logger = LOG_ROOT();

void run_in_fiber() {
    LOG_INFO(g_logger) << "run_in_fiber begin";
    HPS::Fiber::YieldToHold();
    LOG_INFO(g_logger) << "run_in_fiber end";
    HPS::Fiber::YieldToHold();
}

void test_fiber() {
    LOG_INFO(g_logger) << "main begin -1";
    {
        HPS::Fiber::GetThis();
        LOG_INFO(g_logger) << "main begin";
        HPS::Fiber::ptr fiber(new HPS::Fiber(run_in_fiber));
        fiber->call();
        LOG_INFO(g_logger) << "main after call";
        fiber->call();
        LOG_INFO(g_logger) << "main after end";
        fiber->call();
    }
    LOG_INFO(g_logger) << "main after end2";
}

int main(int argc, char** argv) {
    HPS::Thread::SetName("main");

    std::vector<HPS::Thread::ptr> thrs;
    for(int i = 0; i < 1; ++i) {
        thrs.push_back(HPS::Thread::ptr(
                    new HPS::Thread(&test_fiber, "name_" + std::to_string(i))));
    }
    for(auto i : thrs) {
        i->join();
    }
    return 0;
}
