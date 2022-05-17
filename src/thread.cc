#include "thread.h"
#include "log.h"
#include "util.h"

namespace HPS
{
    //! 当前线程指针
    static thread_local Thread *t_thread = nullptr;
    //! 当前线程名称
    static thread_local std::string t_thread_name = "UNKNOW";

    static HPS::Logger::ptr g_logger = LOG_NAME("system");

    Thread *Thread::GetThis()
    {
        return t_thread;
    }

    const std::string &Thread::GetName()
    {
        return t_thread_name;
    }

    void Thread::SetName(const std::string &name)
    {
        if (name.empty())
        {
            return;
        }
        if (t_thread)
        {
            t_thread->m_name = name;
        }
        t_thread_name = name;
    }
    //# 1) 创建线程
    Thread::Thread(std::function<void()> cb, const std::string &name)
        : m_cb(cb), m_name(name)
    {
        if (name.empty())
        {
            m_name = "UNKNOW";
        }
        int rt = pthread_create(&m_thread, nullptr, &Thread::run, this);
        if (rt)
        {
            LOG_ERROR(g_logger) << "pthread_create thread fail, rt=" << rt
                                << " name=" << name;
            throw std::logic_error("pthread_create error");
        }
        //! 必须等待线程函数执行完毕后，线程才能创建完成
        m_semaphore.wait();
    }
    //# 2) 线程执行函数
    void *Thread::run(void *arg)
    {
        Thread *thread = (Thread *)arg;
        t_thread = thread;
        t_thread_name = thread->m_name;
        thread->m_id = HPS::GetThreadId();
        pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());

        std::function<void()> cb;
        cb.swap(thread->m_cb);
        //! 必须在线程所有属性都初始化完毕后才执行线程函数
        thread->m_semaphore.notify();

        cb();
        return 0;
    }
    //# 3) 回收线程
    void Thread::join()
    {
        if (m_thread)
        {
            int rt = pthread_join(m_thread, nullptr);
            if (rt)
            {
                LOG_ERROR(g_logger) << "pthread_join thread fail, rt=" << rt
                                    << " name=" << m_name;
                throw std::logic_error("pthread_join error");
            }
            m_thread = 0;
        }
    }

    Thread::~Thread()
    {
        if (m_thread)
        {
            pthread_detach(m_thread);
        }
    }

}
