#include "../src/tcp_server.h"
#include "../src/log.h"
#include "../src/iomanager.h"
#include "../src/bytearray.h"
#include "../src/address.h"

static HPS::Logger::ptr g_logger = LOG_ROOT();

class EchoServer : public HPS::TcpServer
{
public:
    EchoServer(int type);
    void handleClient(HPS::Socket::ptr client);

private:
    int m_type = 0;
};

EchoServer::EchoServer(int type)
    : m_type(type)
{
}

void EchoServer::handleClient(HPS::Socket::ptr client)
{
    LOG_INFO(g_logger) << "handleClient " << *client;
    HPS::ByteArray::ptr ba(new HPS::ByteArray);
    while (true)
    {
        ba->clear();
        std::vector<iovec> iovs;
        ba->getWriteBuffers(iovs, 1024);

        int rt = client->recv(&iovs[0], iovs.size());
        if (rt == 0)
        {
            LOG_INFO(g_logger) << "client close: " << *client;
            break;
        }
        else if (rt < 0)
        {
            LOG_INFO(g_logger) << "client error rt=" << rt
                               << " errno=" << errno << " errstr=" << strerror(errno);
            break;
        }
        ba->setPosition(ba->getPosition() + rt);
        ba->setPosition(0);
        // LOG_INFO(g_logger) << "recv rt=" << rt << " data=" << std::string((char*)iovs[0].iov_base, rt);
        if (m_type == 1)
        {                                // text
            std::cout << ba->toString(); // << std::endl;
        }
        else
        {
            std::cout << ba->toHexString(); // << std::endl;
        }
        std::cout.flush();
    }
}

int type = 1;

void run()
{
    LOG_INFO(g_logger) << "server type=" << type;
    EchoServer::ptr es(new EchoServer(type));
    auto addr = HPS::Address::LookupAny("0.0.0.0:8020");
    while (!es->bind(addr))
    {
        sleep(2);
    }
    es->start();
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        LOG_INFO(g_logger) << "used as[" << argv[0] << " -t] or [" << argv[0] << " -b]";
        return 0;
    }

    if (!strcmp(argv[1], "-b"))
    {
        type = 2;
    }

    HPS::IOManager iom(2);
    iom.schedule(run);
    return 0;
}
