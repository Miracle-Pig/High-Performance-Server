#include "../include/HPS.h"

static HPS::Logger::ptr g_logger = LOG_NAME("test");

static uint32_t EncodeZigzag32(const int32_t &v)
{
    if (v < 0)
    {
        return ((uint32_t)(-v)) * 2 - 1;
    }
    else
    {
        return v * 2;
    }
}

HPS::http::HttpMethod HPS::http::StringToHttpMethod(const std::string &m)
{
#define XX(num, name, string)            \
    if (strcmp(#string, m.c_str()) == 0) \
    {                                    \
        return HttpMethod::name;         \
    }
    HTTP_METHOD_MAP(XX);
#undef XX
    return HPS::http::HttpMethod::INVALID_METHOD;
}

int main()
{
    // int a = 1, b = 2;
    // if (LIKELY(a == b))
    // {
    //     LOG_DEBUG(g_logger) << "111";
    // }

    LOG_DEBUG(g_logger) << EncodeZigzag32(-4);
    LOG_DEBUG(g_logger) << HPS::http::HttpMethodToString(HPS::http::StringToHttpMethod(""));
    return 0;
}

