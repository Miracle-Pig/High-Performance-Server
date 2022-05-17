#include "../src/http/http_server.h"
#include "../src/log.h"

static HPS::Logger::ptr g_logger = LOG_ROOT();

#define XX(...) #__VA_ARGS__

HPS::IOManager::ptr worker;
void run()
{
    // g_logger->setLevel(HPS::LogLevel::INFO);
    // HPS::http::HttpServer::ptr server(new HPS::http::HttpServer(true, worker.get(), HPS::IOManager::GetThis()));
    //! 创建HTTP服务器
    HPS::http::HttpServer::ptr server(new HPS::http::HttpServer(true));
    //! 返回本地主机套接字
    HPS::Address::ptr addr = HPS::Address::LookupAnyIPAddress("0.0.0.0:8020");
    //! 绑定
    while (!server->bind(addr))
    {
        sleep(2);
    }
    //! 取得服务器Servlet分发器
    auto sd = server->getServletDispatch();
    //! 设定0.0.0.0:8020/HPS/xx此路径的消息体
    sd->addServlet("/HPS/xx", [](HPS::http::HttpRequest::ptr req, HPS::http::HttpResponse::ptr rsp, HPS::http::HttpSession::ptr session)
                   {
                rsp->setBody(req->toString());
                return 0; });
    //! 设定0.0.0.0:8020/HPS/此路径的消息体
    sd->addGlobServlet("/HPS/*", [](HPS::http::HttpRequest::ptr req, HPS::http::HttpResponse::ptr rsp, HPS::http::HttpSession::ptr session)
                       {
                rsp->setBody("Glob:\r\n" + req->toString());
                return 0; });

    sd->addGlobServlet("/sylarx/*", [](HPS::http::HttpRequest::ptr req, HPS::http::HttpResponse::ptr rsp, HPS::http::HttpSession::ptr session)
                       {
                rsp->setBody(XX(<html>
    <head><title>404 Not Found</title></head>
    <body>
    <center><h1>404 Not Found</h1></center>
    <hr><center>nginx/1.16.0</center>
    </body>
    </html>
    <!-- a padding to disable MSIE and Chrome friendly error page -->
    <!-- a padding to disable MSIE and Chrome friendly error page -->
    <!-- a padding to disable MSIE and Chrome friendly error page -->
    <!-- a padding to disable MSIE and Chrome friendly error page -->
    <!-- a padding to disable MSIE and Chrome friendly error page -->
    <!-- a padding to disable MSIE and Chrome friendly error page -->
    ));
                return 0; });
    //! 启动HTTP服务器
    server->start();
}

int main(int argc, char **argv)
{
    HPS::IOManager iom(1);
    // worker.reset(new HPS::IOManager(3, false, "worker"));
    iom.schedule(run);
    return 0;
}
