#include "../src/address.h"
#include "../src/log.h"

HPS::Logger::ptr g_logger = LOG_ROOT();

void test()
{
    std::vector<HPS::Address::ptr> addrs;

    LOG_INFO(g_logger) << "begin";
    bool v = HPS::Address::Lookup(addrs, "[localhost]:80", AF_INET6);
    // bool v = HPS::Address::Lookup(addrs, "www.baidu.com", AF_INET);
    // bool v = HPS::Address::Lookup(addrs, "www.HPS.top", AF_INET);
    LOG_INFO(g_logger) << "end";
    if (!v)
    {
        LOG_ERROR(g_logger) << "lookup fail";
        return;
    }

    for (size_t i = 0; i < addrs.size(); ++i)
    {
        LOG_INFO(g_logger) << i << " - " << addrs[i]->toString();
    }

    auto addr = HPS::Address::LookupAny("[localhost]:80", AF_INET6);
    if (addr)
    {
        LOG_INFO(g_logger) << *addr;
    }
    else
    {
        LOG_ERROR(g_logger) << "error";
    }
}

void test_iface()
{
    std::multimap<std::string, std::pair<HPS::Address::ptr, uint32_t>> results;

    bool v = HPS::Address::GetInterfaceAddresses(results);
    if (!v)
    {
        LOG_ERROR(g_logger) << "GetInterfaceAddresses fail";
        return;
    }

    for (auto &i : results)
    {
        LOG_INFO(g_logger) << i.first << " - " << i.second.first->toString() << " - "
                           << i.second.second;
    }
}

void test_ipv4()
{
    // auto addr = HPS::IPAddress::Create("www.HPS.top");
    auto addr = HPS::IPAddress::Create("127.0.0.8");
    if (addr)
    {
        LOG_INFO(g_logger) << addr->toString();
    }
}

int main(int argc, char **argv)
{
    // test();
    test_iface();
    // test_ipv4();
    return 0;
}
