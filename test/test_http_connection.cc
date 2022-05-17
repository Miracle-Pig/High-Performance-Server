#include <iostream>
#include "../src/http/http_connection.h"
#include "../src/log.h"
#include "../src/iomanager.h"
#include "../src/http/http_parser.h"
// #include "../src/streams/zlib_stream.h"
#include <fstream>

static HPS::Logger::ptr g_logger = LOG_ROOT();

void test_pool() {
    HPS::http::HttpConnectionPool::ptr pool(new HPS::http::HttpConnectionPool(
                "www.HPS.top", "", 80, false, 10, 1000 * 30, 5));

    HPS::IOManager::GetThis()->addTimer(1000, [pool](){
            auto r = pool->doGet("/", 300);
            LOG_INFO(g_logger) << r->toString();
    }, true);
}

void run() {
    HPS::Address::ptr addr = HPS::Address::LookupAnyIPAddress("www.HPS.top:80");
    if(!addr) {
        LOG_INFO(g_logger) << "get addr error";
        return;
    }

    HPS::Socket::ptr sock = HPS::Socket::CreateTCP(addr);
    bool rt = sock->connect(addr);
    if(!rt) {
        LOG_INFO(g_logger) << "connect " << *addr << " failed";
        return;
    }

    HPS::http::HttpConnection::ptr conn(new HPS::http::HttpConnection(sock));
    HPS::http::HttpRequest::ptr req(new HPS::http::HttpRequest);
    req->setPath("/blog/");
    req->setHeader("host", "www.HPS.top");
    LOG_INFO(g_logger) << "req:" << std::endl
        << *req;

    conn->sendRequest(req);
    auto rsp = conn->recvResponse();

    if(!rsp) {
        LOG_INFO(g_logger) << "recv response error";
        return;
    }
    LOG_INFO(g_logger) << "rsp:" << std::endl
        << *rsp;

    std::ofstream ofs("rsp.dat");
    ofs << *rsp;

    LOG_INFO(g_logger) << "=========================";

    auto r = HPS::http::HttpConnection::DoGet("http://www.HPS.top/blog/", 300);
    LOG_INFO(g_logger) << "result=" << r->result
        << " error=" << r->error
        << " rsp=" << (r->response ? r->response->toString() : "");

    LOG_INFO(g_logger) << "=========================";
    test_pool();
}



int main(int argc, char** argv) {
    HPS::IOManager iom(1);
    iom.schedule(run);
    // iom.schedule(test_https);
    return 0;
}
