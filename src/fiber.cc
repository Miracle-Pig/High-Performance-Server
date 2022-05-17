#include "fiber.h"
#include "config.h"
#include "macro.h"
#include "log.h"
#include "scheduler.h"

#include <atomic>

//# 实用函数
namespace HPS
{

    static Logger::ptr g_logger = LOG_NAME("system");

    static std::atomic<uint64_t> s_fiber_id{0};
    static std::atomic<uint64_t> s_fiber_count{0};

    static thread_local Fiber *t_fiber = nullptr;
    static thread_local Fiber::ptr t_threadFiber = nullptr;

    static ConfigVar<uint32_t>::ptr g_fiber_stack_size =
        Config::Lookup<uint32_t>("fiber.stack_size", 128 * 1024, "fiber stack size");

    class MallocStackAllocator
    {
    public:
        static void *Alloc(size_t size)
        {
            return malloc(size);
        }

        static void Dealloc(void *vp, size_t size)
        {
            return free(vp);
        }
    };

    using StackAllocator = MallocStackAllocator;

    uint64_t Fiber::GetFiberId()
    {
        if (t_fiber)
        {
            return t_fiber->getId();
        }
        return 0;
    }

    //重置协程函数，并重置状态
    // INIT，TERM, EXCEPT
    void Fiber::reset(std::function<void()> cb, bool join_schedule)
    {
        ASSERT(m_stack);
        ASSERT(m_state == TERM || m_state == EXCEPT || m_state == INIT);
        m_cb = cb;
        if (getcontext(&m_ctx))
        {
            ASSERT2(false, "getcontext");
        }

        m_ctx.uc_link = nullptr;
        m_ctx.uc_stack.ss_sp = m_stack;
        m_ctx.uc_stack.ss_size = m_stacksize;
        m_joinSchedule = join_schedule;

        makecontext(&m_ctx, &Fiber::MainFunc, 0);
        m_state = INIT;
    }

    //设置当前协程
    void Fiber::SetThis(Fiber *f)
    {
        t_fiber = f;
    }

    //协程切换到后台，并且设置为Ready状态
    void Fiber::YieldToReady()
    {
        Fiber::ptr cur = GetThis();
        ASSERT(cur->m_state == EXEC);
        cur->m_state = READY;
        cur->back();
    }

    //协程切换到后台，并且设置为Hold状态
    void Fiber::YieldToHold()
    {
        Fiber::ptr cur = GetThis();
        ASSERT(cur->m_state == EXEC);
        // cur->m_state = HOLD;
        cur->back();
    }

    //总协程数
    uint64_t Fiber::TotalFibers()
    {
        return s_fiber_count;
    }

    void Fiber::setUseCaller(bool join_schedule)
    {
        this->m_joinSchedule = true;
    }

}

//# 整体逻辑
namespace HPS
{
    //# 1) 创建协程
    Fiber::Fiber(std::function<void()> cb, size_t stacksize, bool join_schedule)
        : m_id(++s_fiber_id), m_cb(cb), m_joinSchedule(join_schedule)
    {
        //! 分配协程栈指针和大小
        ++s_fiber_count;
        m_stacksize = stacksize ? stacksize : g_fiber_stack_size->getValue();
        m_stack = StackAllocator::Alloc(m_stacksize);
        //! 初始化协程上下文
        if (getcontext(&m_ctx))
        {
            ASSERT2(false, "getcontext");
        }
        //! 将协程栈指针和大小赋值给协程上下文
        m_ctx.uc_link = nullptr;
        m_ctx.uc_stack.ss_sp = m_stack;
        m_ctx.uc_stack.ss_size = m_stacksize;
        //! 将协程执行函数分配给协程上下文
        makecontext(&m_ctx, &Fiber::MainFunc, 0);

        LOG_DEBUG(g_logger) << "Fiber::Fiber id=" << m_id;
    }
    //! 无参构造创建主协程，所以必须为私有方法，外部不能随意创建
    Fiber::Fiber()
    {
        m_state = EXEC;
        //! 创建时将本协程设为当前协程
        SetThis(this);
        //! 初始化协程上下文
        if (getcontext(&m_ctx))
        {
            ASSERT2(false, "getcontext");
        }

        ++s_fiber_count;

        LOG_DEBUG(g_logger) << "Fiber::Fiber main";
    }

    //# 2) 返回当前协程，若当前线程不存在任何协程，则创建主协程并返回
    Fiber::ptr Fiber::GetThis()
    {
        if (t_fiber)
        {
            return t_fiber->shared_from_this();
        }
        Fiber::ptr main_fiber(new Fiber);
        ASSERT(t_fiber == main_fiber.get());
        t_threadFiber = main_fiber;
        return t_fiber->shared_from_this();
    }

    //# 3) 主协程切换至运行协程
    void Fiber::call()
    {
        SetThis(this);
        m_state = EXEC;
        if (m_joinSchedule)
        {
            if (swapcontext(&Scheduler::GetMainFiber()->m_ctx, &m_ctx))
            {
                ASSERT2(false, "swapcontext");
            }
        }
        else
        {
            if (swapcontext(&t_threadFiber->m_ctx, &m_ctx))
            {
                ASSERT2(false, "swapcontext");
            }
        }
        //! 切换协程上下文
    }

    //# 4) 协程执行
    void Fiber::MainFunc()
    {
        Fiber::ptr cur = GetThis();
        ASSERT(cur);
        try
        {
            cur->m_cb();
            cur->m_cb = nullptr;
            //! 运行完毕后，置协程状态为终止
            cur->m_state = TERM;
        }
        catch (std::exception &ex)
        {
            //! 若捕捉到异常，则置协程状态为异常
            cur->m_state = EXCEPT;
            LOG_ERROR(g_logger) << "Fiber Except: " << ex.what()
                                << " fiber_id=" << cur->getId()
                                << std::endl
                                << HPS::BacktraceToString();
        }
        catch (...)
        {
            cur->m_state = EXCEPT;
            LOG_ERROR(g_logger) << "Fiber Except"
                                << " fiber_id=" << cur->getId()
                                << std::endl
                                << HPS::BacktraceToString();
        }

        auto raw_ptr = cur.get();
        //! 首先将cur与原始指针分离
        cur.reset();
        //! 然后切换回主协程
        raw_ptr->back();
        ASSERT2(false, "never reach fiber_id=" + std::to_string(raw_ptr->getId()));
    }

    //# 5) 协程运行完毕后切换值主协程
    void Fiber::back()
    {

        if (m_joinSchedule)
        {
            SetThis(Scheduler::GetMainFiber());
            if (swapcontext(&m_ctx, &Scheduler::GetMainFiber()->m_ctx))
            {
                ASSERT2(false, "swapcontext");
            }
        }
        else
        {
            SetThis(t_threadFiber.get());
            if (swapcontext(&m_ctx, &t_threadFiber->m_ctx))
            {
                ASSERT2(false, "swapcontext");
            }
        }
    }

    //# 6) 销毁协程
    Fiber::~Fiber()
    {
        LOG_DEBUG(g_logger) << "Fiber::~Fiber id=" << m_id
                            << " total=" << s_fiber_count;
        --s_fiber_count;
        //! 有协程栈时，说明为其他协程，释放协程栈空间
        if (m_stack)
        {
            ASSERT(m_state == TERM || m_state == EXCEPT || m_state == INIT);

            StackAllocator::Dealloc(m_stack, m_stacksize);
        }
        //! 没有协程栈时，说明为主协程，将主协程置空
        else
        {
            //! 主协程没有运行函数且状态一直为执行
            ASSERT(!m_cb);
            ASSERT(m_state == EXEC);

            Fiber *cur = t_fiber;
            if (cur == this)
            {
                SetThis(nullptr);
            }
        }
    }
}
