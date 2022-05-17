#include "../src/http/http.h"
#include "../src/log.h"

void test_request()
{
    HPS::http::HttpRequest::ptr req(new HPS::http::HttpRequest);
    req->setHeader("host", "www.baidu.com");
    req->setBody("hello HPS");
    req->dump(std::cout) << std::endl;
}

void test_response()
{
    HPS::http::HttpResponse::ptr rsp(new HPS::http::HttpResponse);
    rsp->setHeader("X-X", "HPS");
    rsp->setBody("hello HPS");
    rsp->setStatus((HPS::http::HttpStatus)400);
    rsp->setClose(false);

    rsp->dump(std::cout) << std::endl;
}

int main(int argc, char **argv)
{
    test_request();
    test_response();
    return 0;
}
