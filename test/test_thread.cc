#include "../include/HPS.h"
#include <unistd.h>

HPS::Logger::ptr g_logger = LOG_ROOT();

int count = 0;
// HPS::RWMutex s_mutex;
HPS::Mutex s_mutex;

void fun1()
{
    LOG_INFO(g_logger) << "name: " << HPS::Thread::GetName()
                       << " this.name: " << HPS::Thread::GetThis()->getName()
                       << " id: " << HPS::GetThreadId()
                       << " this.id: " << HPS::Thread::GetThis()->getId();

    for (int i = 0; i < 100000; ++i)
    {
        // HPS::RWMutex::WriteLock lock(s_mutex);
        HPS::Mutex::Lock lock(s_mutex); 
        ++count;
    }
}

void fun2()
{
    while (true)
    {
        LOG_INFO(g_logger) << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
    }
}

void fun3()
{
    while (true)
    {
        LOG_INFO(g_logger) << "========================================";
    }
}

int main(int argc, char **argv)
{
    LOG_INFO(g_logger) << "thread test begin";
    YAML::Node root = YAML::LoadFile("/home/miracle/Zone/workspace/HPS/bin/conf/log.yml");
    HPS::Config::LoadFromYaml(root);

    std::vector<HPS::Thread::ptr> thrs;
    for (int i = 0; i < 10; ++i)
    {
        // HPS::Thread::ptr thr1(new HPS::Thread(&fun2, "name_" + std::to_string(i * 2)));
        // HPS::Thread::ptr thr2(new HPS::Thread(&fun3, "name_" + std::to_string(i * 2 + 1)));
        // thrs.push_back(thr1);
        // thrs.push_back(thr2);

        HPS::Thread::ptr thr3(new HPS::Thread(&fun1, "name_" + std::to_string(i * 2 + 1)));
        thrs.push_back(thr3);
    }

    for (size_t i = 0; i < thrs.size(); ++i)
    {
        thrs[i]->join();
    }
    LOG_INFO(g_logger) << "thread test end";
    LOG_INFO(g_logger) << "count=" << count;

    return 0;
}
