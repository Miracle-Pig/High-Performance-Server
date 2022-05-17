#include "hook.h"
#include "config.h"
#include "log.h"
#include "fiber.h"
#include "iomanager.h"
#include "fd_manager.h"
#include "macro.h"

#include <dlfcn.h>
#include <stdarg.h>

HPS::Logger::ptr g_logger = LOG_NAME("system");
namespace HPS
{

    static HPS::ConfigVar<int>::ptr g_tcp_connect_timeout =
        HPS::Config::Lookup("tcp.connect.timeout", 5000, "tcp connect timeout");

    static thread_local bool t_hook_enable = false;

    static uint64_t s_connect_timeout = -1;

    bool is_hook_enable()
    {
        return t_hook_enable;
    }

    void set_hook_enable(bool flag)
    {
        t_hook_enable = flag;
    }

}

//# 整体逻辑
namespace HPS
{
    //# 2) 宏定义批量“钩”C库函数
#define HOOK_FUN(XX) \
    XX(sleep)        \
    XX(usleep)       \
    XX(nanosleep)    \
    XX(socket)       \
    XX(connect)      \
    XX(accept)       \
    XX(read)         \
    XX(readv)        \
    XX(recv)         \
    XX(recvfrom)     \
    XX(recvmsg)      \
    XX(write)        \
    XX(writev)       \
    XX(send)         \
    XX(sendto)       \
    XX(sendmsg)      \
    XX(close)        \
    XX(fcntl)        \
    XX(ioctl)        \
    XX(getsockopt)   \
    XX(setsockopt)
    //# 3) 初始化HOOK
    void hook_init()
    {
        static bool is_inited = false;
        if (is_inited)
        {
            return;
        }
        //! dlsym根据动态链接库操作句柄与符号，返回符号对应的地址，
        //! 不但可以获取函数地址，可以获取变量地址。
        //! 钩住!
#define XX(name) name##_f = (name##_fun)dlsym(RTLD_NEXT, #name);
        HOOK_FUN(XX);
#undef XX
    }

    //# 4) HOOK初始化结构体
    struct _HookIniter
    {
        _HookIniter()
        {
            hook_init();
            s_connect_timeout = g_tcp_connect_timeout->getValue();

            g_tcp_connect_timeout->addListener([](const int &old_value, const int &new_value)
                                               {
                LOG_INFO(g_logger) << "tcp connect timeout changed from "
                                         << old_value << " to " << new_value;
                s_connect_timeout = new_value; });
        }
    };

    //# 5) 自动创建HOOK初始化结构体默认构造函数
    static _HookIniter s_hook_initer;
}

struct timer_info
{
    int cancelled = 0;
};

//# 6) 自定义IO(!!!)
template <typename OriginFun, typename... Args>
static ssize_t do_io(int fd, OriginFun fun, const char *hook_fun_name,
                     uint32_t event, int timeout_so, Args &&...args)
{
    //! 若“不钩”则直接调用内部C库函数
    if (!HPS::t_hook_enable)
    {
        return fun(fd, std::forward<Args>(args)...);
    }
    //! 向句柄管理器添加并获取文件句柄管理对象
    HPS::FdCtx::ptr ctx = HPS::FdMgr::GetInstance()->get(fd);
    if (!ctx)
    {
        return fun(fd, std::forward<Args>(args)...);
    }
    //! 文件若关闭
    if (ctx->isClose())
    {
        errno = EBADF;
        return -1;
    }
    //! 不是socket句柄或用户主动设为阻塞
    if (!ctx->isSocket() || ctx->getUserNonblock())
    {
        return fun(fd, std::forward<Args>(args)...);
    }
    //! 获取socket接收或发送的超时时间
    uint64_t to = ctx->getTimeout(timeout_so);
    std::shared_ptr<timer_info> tinfo(new timer_info);

retry:
    //! foward完美转发参数
    ssize_t n = fun(fd, std::forward<Args>(args)...);
    //! 读写时发生中断
    while (n == -1 && errno == EINTR)
    {
        //! 重新调用，读完为止
        n = fun(fd, std::forward<Args>(args)...);
    }
    //! 连续做read操作而没有数据可读
    if (n == -1 && errno == EAGAIN)
    {
        HPS::IOManager *iom = HPS::IOManager::GetThis();
        HPS::Timer::ptr timer;
        std::weak_ptr<timer_info> winfo(tinfo);
        //! 句柄存在超时时间
        if (to != (uint64_t)-1)
        {
            timer = iom->addConditionTimer(
                to, [winfo, fd, iom, event]()
                {
                auto t = winfo.lock();
                if(!t || t->cancelled) {
                    return;
                }
                t->cancelled = ETIMEDOUT;
                //! 执行并取消事件
                iom->cancelEvent(fd, (HPS::IOManager::Event)(event)); },
                winfo);
        }
        //! 添加事件的回调为空,默认当前协程为回调
        int rt = iom->addEvent(fd, (HPS::IOManager::Event)(event));
        //! 添加事件失败，rt!=0
        if (UNLIKELY(rt))
        {
            LOG_ERROR(g_logger) << hook_fun_name << " addEvent("
                                << fd << ", " << event << ")";
            if (timer)
            {
                timer->cancel();
            }
            return -1;
        }
        else
        {
            //! 添加事件成功
            //! 让出协程执行权限???
            HPS::Fiber::YieldToHold();
            //! 当条件定时器超时时,会唤醒协程,当数据回来时,也会唤醒协程???
            if (timer)
            {
                timer->cancel();
            }
            //! 通过定时任务唤醒
            if (tinfo->cancelled)
            {
                errno = tinfo->cancelled;
                return -1;
            }
            //! 有IO事件,需要重新读
            goto retry;
        }
    }

    return n;
}

extern "C"
{
#define XX(name) name##_fun name##_f = nullptr;
    HOOK_FUN(XX);
#undef XX

    unsigned int sleep(unsigned int seconds)
    {
        if (!HPS::t_hook_enable)
        {
            return sleep_f(seconds);
        }

        HPS::Fiber::ptr fiber = HPS::Fiber::GetThis();
        HPS::IOManager *iom = HPS::IOManager::GetThis();
        iom->addTimer(seconds * 1000, std::bind((void(HPS::Scheduler::*)(HPS::Fiber::ptr, int thread)) & HPS::IOManager::schedule, iom, fiber, -1));
        HPS::Fiber::YieldToHold();
        return 0;
    }

    int usleep(useconds_t usec)
    {
        if (!HPS::t_hook_enable)
        {
            return usleep_f(usec);
        }
        HPS::Fiber::ptr fiber = HPS::Fiber::GetThis();
        HPS::IOManager *iom = HPS::IOManager::GetThis();
        iom->addTimer(usec / 1000, std::bind((void(HPS::Scheduler::*)(HPS::Fiber::ptr, int thread)) & HPS::IOManager::schedule, iom, fiber, -1));
        HPS::Fiber::YieldToHold();
        return 0;
    }

    int nanosleep(const struct timespec *req, struct timespec *rem)
    {
        if (!HPS::t_hook_enable)
        {
            return nanosleep_f(req, rem);
        }

        int timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000 / 1000;
        HPS::Fiber::ptr fiber = HPS::Fiber::GetThis();
        HPS::IOManager *iom = HPS::IOManager::GetThis();
        iom->addTimer(timeout_ms, std::bind((void(HPS::Scheduler::*)(HPS::Fiber::ptr, int thread)) & HPS::IOManager::schedule, iom, fiber, -1));
        HPS::Fiber::YieldToHold();
        return 0;
    }

    int socket(int domain, int type, int protocol)
    {
        if (!HPS::t_hook_enable)
        {
            return socket_f(domain, type, protocol);
        }
        int fd = socket_f(domain, type, protocol);
        if (fd == -1)
        {
            return fd;
        }
        HPS::FdMgr::GetInstance()->get(fd, true);
        return fd;
    }

    int connect_with_timeout(int fd, const struct sockaddr *addr, socklen_t addrlen, uint64_t timeout_ms)
    {
        if (!HPS::t_hook_enable)
        {
            return connect_f(fd, addr, addrlen);
        }
        HPS::FdCtx::ptr ctx = HPS::FdMgr::GetInstance()->get(fd);
        if (!ctx || ctx->isClose())
        {
            errno = EBADF;
            return -1;
        }

        if (!ctx->isSocket())
        {
            return connect_f(fd, addr, addrlen);
        }

        if (ctx->getUserNonblock())
        {
            return connect_f(fd, addr, addrlen);
        }

        int n = connect_f(fd, addr, addrlen);
        if (n == 0)
        {
            return 0;
        }
        else if (n != -1 || errno != EINPROGRESS)
        {
            return n;
        }

        HPS::IOManager *iom = HPS::IOManager::GetThis();
        HPS::Timer::ptr timer;
        std::shared_ptr<timer_info> tinfo(new timer_info);
        std::weak_ptr<timer_info> winfo(tinfo);

        if (timeout_ms != (uint64_t)-1)
        {
            timer = iom->addConditionTimer(
                timeout_ms, [winfo, fd, iom]()
                {
                auto t = winfo.lock();
                if(!t || t->cancelled) {
                    return;
                }
                t->cancelled = ETIMEDOUT;
                iom->cancelEvent(fd, HPS::IOManager::WRITE); },
                winfo);
        }

        int rt = iom->addEvent(fd, HPS::IOManager::WRITE);
        if (rt == 0)
        {
            HPS::Fiber::YieldToHold();
            if (timer)
            {
                timer->cancel();
            }
            if (tinfo->cancelled)
            {
                errno = tinfo->cancelled;
                return -1;
            }
        }
        else
        {
            if (timer)
            {
                timer->cancel();
            }
            LOG_ERROR(g_logger) << "connect addEvent(" << fd << ", WRITE) error";
        }

        int error = 0;
        socklen_t len = sizeof(int);
        if (-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len))
        {
            return -1;
        }
        if (!error)
        {
            return 0;
        }
        else
        {
            errno = error;
            return -1;
        }
    }

    int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
    {
        return connect_with_timeout(sockfd, addr, addrlen, HPS::s_connect_timeout);
    }

    int accept(int s, struct sockaddr *addr, socklen_t *addrlen)
    {
        int fd = do_io(s, accept_f, "accept", HPS::IOManager::READ, SO_RCVTIMEO, addr, addrlen);
        if (fd >= 0)
        {
            HPS::FdMgr::GetInstance()->get(fd, true);
        }
        return fd;
    }

    //# 7) 调用自定义钩子函数
    ssize_t read(int fd, void *buf, size_t count)
    {
        return do_io(fd, read_f, "read", HPS::IOManager::READ, SO_RCVTIMEO, buf, count);
    }

    ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
    {
        return do_io(fd, readv_f, "readv", HPS::IOManager::READ, SO_RCVTIMEO, iov, iovcnt);
    }

    ssize_t recv(int sockfd, void *buf, size_t len, int flags)
    {
        return do_io(sockfd, recv_f, "recv", HPS::IOManager::READ, SO_RCVTIMEO, buf, len, flags);
    }

    ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
    {
        return do_io(sockfd, recvfrom_f, "recvfrom", HPS::IOManager::READ, SO_RCVTIMEO, buf, len, flags, src_addr, addrlen);
    }

    ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags)
    {
        return do_io(sockfd, recvmsg_f, "recvmsg", HPS::IOManager::READ, SO_RCVTIMEO, msg, flags);
    }

    ssize_t write(int fd, const void *buf, size_t count)
    {
        return do_io(fd, write_f, "write", HPS::IOManager::WRITE, SO_SNDTIMEO, buf, count);
    }

    ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
    {
        return do_io(fd, writev_f, "writev", HPS::IOManager::WRITE, SO_SNDTIMEO, iov, iovcnt);
    }

    ssize_t send(int s, const void *msg, size_t len, int flags)
    {
        return do_io(s, send_f, "send", HPS::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags);
    }

    ssize_t sendto(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen)
    {
        return do_io(s, sendto_f, "sendto", HPS::IOManager::WRITE, SO_SNDTIMEO, msg, len, flags, to, tolen);
    }

    ssize_t sendmsg(int s, const struct msghdr *msg, int flags)
    {
        return do_io(s, sendmsg_f, "sendmsg", HPS::IOManager::WRITE, SO_SNDTIMEO, msg, flags);
    }

    int close(int fd)
    {
        if (!HPS::t_hook_enable)
        {
            return close_f(fd);
        }

        HPS::FdCtx::ptr ctx = HPS::FdMgr::GetInstance()->get(fd);
        if (ctx)
        {
            auto iom = HPS::IOManager::GetThis();
            if (iom)
            {
                iom->cancelAll(fd);
            }
            HPS::FdMgr::GetInstance()->del(fd);
        }
        return close_f(fd);
    }

    int fcntl(int fd, int cmd, ... /* arg */)
    {
        va_list va;
        va_start(va, cmd);
        switch (cmd)
        {
        case F_SETFL:
        {
            int arg = va_arg(va, int);
            va_end(va);
            HPS::FdCtx::ptr ctx = HPS::FdMgr::GetInstance()->get(fd);
            if (!ctx || ctx->isClose() || !ctx->isSocket())
            {
                return fcntl_f(fd, cmd, arg);
            }
            ctx->setUserNonblock(arg & O_NONBLOCK);
            if (ctx->getSysNonblock())
            {
                arg |= O_NONBLOCK;
            }
            else
            {
                arg &= ~O_NONBLOCK;
            }
            return fcntl_f(fd, cmd, arg);
        }
        break;
        case F_GETFL:
        {
            va_end(va);
            int arg = fcntl_f(fd, cmd);
            HPS::FdCtx::ptr ctx = HPS::FdMgr::GetInstance()->get(fd);
            if (!ctx || ctx->isClose() || !ctx->isSocket())
            {
                return arg;
            }
            if (ctx->getUserNonblock())
            {
                return arg | O_NONBLOCK;
            }
            else
            {
                return arg & ~O_NONBLOCK;
            }
        }
        break;
        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
        case F_SETFD:
        case F_SETOWN:
        case F_SETSIG:
        case F_SETLEASE:
        case F_NOTIFY:
#ifdef F_SETPIPE_SZ
        case F_SETPIPE_SZ:
#endif
        {
            int arg = va_arg(va, int);
            va_end(va);
            return fcntl_f(fd, cmd, arg);
        }
        break;
        case F_GETFD:
        case F_GETOWN:
        case F_GETSIG:
        case F_GETLEASE:
#ifdef F_GETPIPE_SZ
        case F_GETPIPE_SZ:
#endif
        {
            va_end(va);
            return fcntl_f(fd, cmd);
        }
        break;
        case F_SETLK:
        case F_SETLKW:
        case F_GETLK:
        {
            struct flock *arg = va_arg(va, struct flock *);
            va_end(va);
            return fcntl_f(fd, cmd, arg);
        }
        break;
        case F_GETOWN_EX:
        case F_SETOWN_EX:
        {
            struct f_owner_exlock *arg = va_arg(va, struct f_owner_exlock *);
            va_end(va);
            return fcntl_f(fd, cmd, arg);
        }
        break;
        default:
            va_end(va);
            return fcntl_f(fd, cmd);
        }
    }

    int ioctl(int d, unsigned long int request, ...)
    {
        va_list va;
        va_start(va, request);
        void *arg = va_arg(va, void *);
        va_end(va);

        if (FIONBIO == request)
        {
            bool user_nonblock = !!*(int *)arg;
            HPS::FdCtx::ptr ctx = HPS::FdMgr::GetInstance()->get(d);
            if (!ctx || ctx->isClose() || !ctx->isSocket())
            {
                return ioctl_f(d, request, arg);
            }
            ctx->setUserNonblock(user_nonblock);
        }
        return ioctl_f(d, request, arg);
    }

    int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen)
    {
        return getsockopt_f(sockfd, level, optname, optval, optlen);
    }

    int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
    {
        if (!HPS::t_hook_enable)
        {
            return setsockopt_f(sockfd, level, optname, optval, optlen);
        }
        if (level == SOL_SOCKET)
        {
            if (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO)
            {
                HPS::FdCtx::ptr ctx = HPS::FdMgr::GetInstance()->get(sockfd);
                if (ctx)
                {
                    const timeval *v = (const timeval *)optval;
                    ctx->setTimeout(optname, v->tv_sec * 1000 + v->tv_usec / 1000);
                }
            }
        }
        return setsockopt_f(sockfd, level, optname, optval, optlen);
    }
}
