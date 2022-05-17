#include "../include/HPS.h"

static HPS::Logger::ptr g_logger = LOG_ROOT();
static int s_count = 5;

void test_fiber() {

    
    
    LOG_INFO(g_logger) << "test in fiber s_count=" << s_count;

    
    if(--s_count >= 0) {
        HPS::Scheduler::GetThis()->schedule(&test_fiber, HPS::GetThreadId());
    }
    HPS::set_hook_enable(false);
    sleep(1);
}

int main(int argc, char** argv) {
    LOG_INFO(g_logger) << "main";
    HPS::Scheduler sc(1, false, "test");
    sc.start();
    sleep(2);
    LOG_INFO(g_logger) << "schedule";
    sc.schedule(&test_fiber);
    sc.stop();
    LOG_INFO(g_logger) << "over";
    return 0;
}
