#include "../src/tcp_server.h"
#include "../src/iomanager.h"
#include "../src/log.h"

HPS::Logger::ptr g_logger = LOG_ROOT();

void run() {
    //! 返回本机任意网络地址，形成套接字
    auto addr = HPS::Address::LookupAny("0.0.0.0:8033");
    //auto addr2 = HPS::UnixAddress::ptr(new HPS::UnixAddress("/tmp/unix_addr"));
    std::vector<HPS::Address::ptr> addrs;
    addrs.push_back(addr);
    //addrs.push_back(addr2);
    //! 创建TCP服务器
    HPS::TcpServer::ptr tcp_server(new HPS::TcpServer);
    std::vector<HPS::Address::ptr> fails;
    //! 与本机网络地址绑定
    while(!tcp_server->bind(addrs, fails)) {
        sleep(2);
    }
    //! 启动服务器
    tcp_server->start();
}
int main(int argc, char** argv) {
    //! 启动IO调度器
    HPS::IOManager iom(2);
    iom.schedule(run);
    return 0;
}
