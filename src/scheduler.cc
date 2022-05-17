#include "scheduler.h"
#include "log.h"
#include "macro.h"
#include "hook.h"

//# 实用方法
namespace HPS
{
    static HPS::Logger::ptr g_logger = LOG_NAME("system");
    static thread_local Scheduler *t_scheduler = nullptr;
    static thread_local Fiber *t_scheduler_fiber = nullptr;

    Scheduler *Scheduler::GetThis()
    {
        return t_scheduler;
    }

    Fiber *Scheduler::GetMainFiber()
    {
        return t_scheduler_fiber;
    }

    void Scheduler::setThis()
    {
        t_scheduler = this;
    }

    void Scheduler::tickle()
    {
        LOG_INFO(g_logger) << "tickle";
    }

    bool Scheduler::stopping()
    {
        MutexType::Lock lock(m_mutex);
        //! 调度器自动（正常）停止且处于正在停止状态且没有任务和活跃线程，调度器才能停止
        return m_autoStop && m_stopping && m_tasks.empty() && m_activeThreadCount == 0;
    }
    //! 空闲方法，
    void Scheduler::idle()
    {
        LOG_INFO(g_logger) << "idle";
        while (!stopping())
        {
            //! 空闲协程和调度协程在此切入切出
            HPS::Fiber::YieldToHold();
        }
    }

    void Scheduler::switchTo(int thread)
    {
        ASSERT(Scheduler::GetThis() != nullptr);
        if (Scheduler::GetThis() == this)
        {
            if (thread == -1 || thread == HPS::GetThreadId())
            {
                return;
            }
        }
        schedule(Fiber::GetThis(), thread);
        Fiber::YieldToHold();
    }

    std::ostream &Scheduler::dump(std::ostream &os)
    {
        os << "[Scheduler name=" << m_name
           << " size=" << m_threadCount
           << " active_count=" << m_activeThreadCount
           << " idle_count=" << m_idleThreadCount
           << " stopping=" << m_stopping
           << " ]" << std::endl
           << "    ";
        for (size_t i = 0; i < m_threadIds.size(); ++i)
        {
            if (i)
            {
                os << ", ";
            }
            os << m_threadIds[i];
        }
        return os;
    }

    // SchedulerSwitcher::SchedulerSwitcher(Scheduler *target)
    // {
    //     m_caller = Scheduler::GetThis();
    //     if (target)
    //     {
    //         target->switchTo();
    //     }
    // }

    // SchedulerSwitcher::~SchedulerSwitcher()
    // {
    //     if (m_caller)
    //     {
    //         m_caller->switchTo();
    //     }
    // }
}

//# 整体逻辑
namespace HPS
{
    //# 1)主线程创建协程调度器
    Scheduler::Scheduler(size_t threads, bool use_mainThread, const std::string &name)
        : m_name(name)
    {
        ASSERT(threads > 0);
        //! 若需要主线程（创建该调度器的线程，非线程池的线程）执行任务
        if (use_mainThread)
        {
            //! 创建线程主协程
            HPS::Fiber::GetThis().get();
            --threads;

            ASSERT(GetThis() == nullptr);
            t_scheduler = this;
            //! 回调函数往往通过函数指针来实现，而类的成员函数，多了一个隐含的参数this，
            //! 所以直接赋值给函数指针会引起编译报错。通过bind可以解决此问题
            m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, false));
            HPS::Thread::SetName(m_name);
            //! 线程主协程作为调度协程
            t_scheduler_fiber = m_rootFiber.get();
            m_rootThread = HPS::GetThreadId();
            m_threadIds.push_back(m_rootThread);
        }
        else
        {
            m_rootThread = -1;
        }
        m_threadCount = threads;
    }

    //# 2) 调度器启动
    void Scheduler::start()
    {
        MutexType::Lock lock(m_mutex);
        //! 若调度器正处于停止状态，则无法启动
        if (!m_stopping)
        {
            return;
        }
        m_stopping = false;
        ASSERT(m_threads.empty());
        //! 创建线程池
        m_threads.resize(m_threadCount);
        for (size_t i = 0; i < m_threadCount; ++i)
        {
            m_threads[i].reset(new Thread(std::bind(&Scheduler::run, this), m_name + "_" + std::to_string(i)));
            m_threadIds.push_back(m_threads[i]->getId());
        }
        lock.unlock();
    }

    //# 3)调度器执行
    void Scheduler::run()
    {
        LOG_DEBUG(g_logger) << m_name << " run";
        set_hook_enable(true);
        //! 设置当前线程的调度器（作用???）
        setThis();
        //! 如果当前线程不是主线程
        if (HPS::GetThreadId() != m_rootThread)
        {
            //! 在该线程上创建调度协程
            t_scheduler_fiber = Fiber::GetThis().get();
        }
        //! 运行空闲方法的协程
        Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this), 0, true));
        //! 执行任务的协程
        Fiber::ptr task_fiber;

        Task task;
        while (true)
        {
            task.reset();
            //! 作用（???）
            bool tickle_me = false;
            bool is_active = false;
            {
                MutexType::Lock lock(m_mutex);
                auto it = m_tasks.begin();
                //! 遍历任务队列，
                while (it != m_tasks.end())
                {
                    //! 若该任务不能被任意线程执行，或者不是能执行该任务相应的线程
                    if (it->thread != -1 && it->thread != HPS::GetThreadId())
                    {
                        ++it;
                        tickle_me = true;
                        continue;
                    }

                    ASSERT(it->fiber || it->cb);
                    //! 若该任务已经在执行状态
                    if (it->fiber && it->fiber->getState() == Fiber::EXEC)
                    {
                        ++it;
                        continue;
                    }
                    //! 线程取得任务
                    task = *it;
                    m_tasks.erase(it++);
                    ++m_activeThreadCount;
                    is_active = true;
                    break;
                }
                tickle_me |= it != m_tasks.end();
            }
            //! ???
            if (tickle_me)
            {
                tickle();
            }
            //! 取到任务时执行
            if (task.fiber && (task.fiber->getState() != Fiber::TERM && task.fiber->getState() != Fiber::EXCEPT))
            {
                task.fiber->setUseCaller(true);
                task.fiber->call();
                --m_activeThreadCount;

                if (task.fiber->getState() == Fiber::READY)
                {
                    schedule(task.fiber);
                }
                else if (task.fiber->getState() != Fiber::TERM && task.fiber->getState() != Fiber::EXCEPT)
                {
                    task.fiber->m_state = Fiber::HOLD;
                }
                task.reset();
            }
            else if (task.cb)
            {
                if (task_fiber)
                {
                    task_fiber->reset(task.cb, true);
                }
                else
                {
                    task_fiber.reset(new Fiber(task.cb, 0, true));
                }
                task.reset();
                task_fiber->call();
                --m_activeThreadCount;
                if (task_fiber->getState() == Fiber::READY)
                {
                    schedule(task_fiber);
                    task_fiber.reset();
                }
                else if (task_fiber->getState() == Fiber::EXCEPT || task_fiber->getState() == Fiber::TERM)
                {
                    task_fiber->reset(nullptr);
                }
                else
                { // if(task_fiber->getState() != Fiber::TERM) {
                    task_fiber->m_state = Fiber::HOLD;
                    task_fiber.reset();
                }
            }
            //! 没有取到时，陷入空闲方法
            else
            {
                if (is_active)
                {
                    --m_activeThreadCount;
                    continue;
                }
                if (idle_fiber->getState() == Fiber::TERM)
                {
                    LOG_INFO(g_logger) << "idle fiber term";
                    break;
                }

                ++m_idleThreadCount;
                idle_fiber->call();
                --m_idleThreadCount;
                if (idle_fiber->getState() != Fiber::TERM && idle_fiber->getState() != Fiber::EXCEPT)
                {
                    idle_fiber->m_state = Fiber::HOLD;
                }
            }
        }
    }
    //# 4) 主线程将调度器停止
    void Scheduler::stop()
    {
        m_autoStop = true;
        //! 当只有主线程执行任务时
        if (m_rootFiber && m_threadCount == 0 && (m_rootFiber->getState() == Fiber::TERM || m_rootFiber->getState() == Fiber::INIT))
        {
            LOG_INFO(g_logger) << this << " stopped";
            m_stopping = true;

            if (stopping())
            {
                return;
            }
        }

        // bool exit_on_this_fiber = false;
        //! 主线程参与执行任务
        if (m_rootThread != -1)
        {
            ASSERT(GetThis() == this);
        }
        //! 主线程不参与执行任务
        else
        {
            //! 其他线程不得停止调度器
            ASSERT(GetThis() != this);
        }

        m_stopping = true;
        for (size_t i = 0; i < m_threadCount; ++i)
        {
            tickle();
        }

        if (m_rootFiber)
        {
            tickle();
        }

        if (m_rootFiber)
        {
            // while(!stopping()) {
            //     if(m_rootFiber->getState() == Fiber::TERM
            //             || m_rootFiber->getState() == Fiber::EXCEPT) {
            //         m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true));
            //         LOG_INFO(g_logger) << " root fiber is term, reset";
            //         t_fiber = m_rootFiber.get();
            //     }
            //     m_rootFiber->call();
            // }
            if (!stopping())
            {
                //! 主线程执行任务
                m_rootFiber->call();
            }
        }

        std::vector<Thread::ptr> thrs;
        {
            MutexType::Lock lock(m_mutex);
            thrs.swap(m_threads);
        }

        for (auto &i : thrs)
        {
            i->join();
        }
        // if(exit_on_this_fiber) {
        // }
    }

    //# 5) 调度器销毁
    Scheduler::~Scheduler()
    {
        ASSERT(m_stopping);
        if (GetThis() == this)
        {
            t_scheduler = nullptr;
        }
    }
}
