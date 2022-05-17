#include "timer.h"
#include "util.h"

//# 实用方法
namespace HPS
{
    //! 按定时器执行时间排序
    bool Timer::Comparator::operator()(const Timer::ptr &lhs, const Timer::ptr &rhs) const
    {
        if (!lhs && !rhs)
        {
            return false;
        }
        if (!lhs)
        {
            return true;
        }
        if (!rhs)
        {
            return false;
        }
        if (lhs->m_next < rhs->m_next)
        {
            return true;
        }
        if (rhs->m_next < lhs->m_next)
        {
            return false;
        }
        return lhs.get() < rhs.get();
    }

    bool Timer::cancel()
    {
        TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
        if (m_cb)
        {
            m_cb = nullptr;
            auto it = m_manager->m_timers.find(shared_from_this());
            m_manager->m_timers.erase(it);
            return true;
        }
        return false;
    }

    bool Timer::refresh()
    {
        TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
        if (!m_cb)
        {
            return false;
        }
        auto it = m_manager->m_timers.find(shared_from_this());
        if (it == m_manager->m_timers.end())
        {
            return false;
        }
        m_manager->m_timers.erase(it);
        m_next = HPS::GetCurrentMS() + m_ms;
        m_manager->m_timers.insert(shared_from_this());
        return true;
    }

    bool Timer::reset(uint64_t ms, bool from_now)
    {
        if (ms == m_ms && !from_now)
        {
            return true;
        }
        TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
        if (!m_cb)
        {
            return false;
        }
        auto it = m_manager->m_timers.find(shared_from_this());
        if (it == m_manager->m_timers.end())
        {
            return false;
        }
        m_manager->m_timers.erase(it);
        uint64_t start = 0;
        if (from_now)
        {
            start = HPS::GetCurrentMS();
        }
        else
        {
            start = m_next - m_ms;
        }
        m_ms = ms;
        m_next = start + m_ms;
        m_manager->addTimer(shared_from_this(), lock);
        return true;
    }
    
    static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void()> cb)
    {
        //! 如果当前 weak_ptr 已经过期，则该函数会返回一个空的 shared_ptr 指针；
        //! 反之，该函数返回一个和当前 weak_ptr 指向相同的 shared_ptr 指针。
        std::shared_ptr<void> tmp = weak_cond.lock();
        if (tmp)
        {
            cb();
        }
    }

    Timer::ptr TimerManager::addConditionTimer(uint64_t ms, std::function<void()> cb, std::weak_ptr<void> weak_cond, bool recurring)
    {
        return addTimer(ms, std::bind(&OnTimer, weak_cond, cb), recurring);
    }

    uint64_t TimerManager::getNextTimer()
    {
        RWMutexType::ReadLock lock(m_mutex);
        m_tickled = false;
        if (m_timers.empty())
        {
            return ~0ull;
        }

        const Timer::ptr &next = *m_timers.begin();
        uint64_t now_ms = HPS::GetCurrentMS();
        if (now_ms >= next->m_next)
        {
            return 0;
        }
        else
        {
            return next->m_next - now_ms;
        }
    }

    bool TimerManager::hasTimer()
    {
        RWMutexType::ReadLock lock(m_mutex);
        return !m_timers.empty();
    }

}

//# 整体逻辑
namespace HPS
{
    //# 1)创建定时器
    Timer::Timer(uint64_t ms, std::function<void()> cb,
                 bool recurring, TimerManager *manager)
        : m_recurring(recurring), m_ms(ms), m_cb(cb), m_manager(manager)
    {
        m_next = HPS::GetCurrentMS() + m_ms;
    }
    Timer::Timer(uint64_t next)
        : m_next(next)
    {
    }

    //# 2) 创建定时器管理器
    TimerManager::TimerManager()
    {
        m_previouseTime = HPS::GetCurrentMS();
    }

    //# 3) 添加定时器
    Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> cb, bool recurring)
    {
        Timer::ptr timer(new Timer(ms, cb, recurring, this));
        RWMutexType::WriteLock lock(m_mutex);
        addTimer(timer, lock);
        return timer;
    }
    void TimerManager::addTimer(Timer::ptr val, RWMutexType::WriteLock &lock)
    {
        auto it = m_timers.insert(val).first;
        //! 若插入的定时器为集合头部（即为最早执行的定时器），且没有触发onTimerInsertedAtFront
        bool at_front = (it == m_timers.begin()) && !m_tickled;
        if (at_front)
        {
            m_tickled = true;
        }
        lock.unlock();

        if (at_front)
        {
            onTimerInsertedAtFront();
        }
    }

    //# 4) 获取需要执行的定时器的回调函数列表
    void TimerManager::listExpiredCb(std::vector<std::function<void()>> &cbs)
    {
        uint64_t now_ms = HPS::GetCurrentMS();
        //! 在定时器集合中提取已经失效的定时器（过期的）
        std::vector<Timer::ptr> expired;
        {
            RWMutexType::ReadLock lock(m_mutex);
            if (m_timers.empty())
            {
                return;
            }
        }
        RWMutexType::WriteLock lock(m_mutex);
        if (m_timers.empty())
        {
            return;
        }
        //! 检测管理器执行时间是否被调后了,并重置管理器执行时间
        bool rollover = detectClockRollover(now_ms);
        //! 若管理器执行时间没有被调后且最近要执行的定时器时间晚于当前时间
        if (!rollover && ((*m_timers.begin())->m_next > now_ms))
        {
            //! 不存在过期的定时器
            return;
        }

        Timer::ptr now_timer(new Timer(now_ms));
        //! 若管理器执行时间已调后，则所有定时器全部过期（需要执行）
        //! 否则将所有精确执行时间为当前时间的定时器放入过期定时器数组
        auto it = rollover ? m_timers.end() : m_timers.lower_bound(now_timer);
        while (it != m_timers.end() && (*it)->m_next == now_ms)
        {
            ++it;
        }
        expired.insert(expired.begin(), m_timers.begin(), it);
        m_timers.erase(m_timers.begin(), it);
        cbs.reserve(expired.size());

        for (auto &timer : expired)
        {
            cbs.push_back(timer->m_cb);
            if (timer->m_recurring)
            {
                //! 若为循环定时器，则将定时器重新加入到定时器集合
                timer->m_next = now_ms + timer->m_ms;
                m_timers.insert(timer);
            }
            else
            {
                timer->m_cb = nullptr;
            }
        }
    }
    bool TimerManager::detectClockRollover(uint64_t now_ms)
    {
        bool rollover = false;
        //! 若当前时间早于上次定时器管理器执行时间前一个小时，
        if (now_ms < m_previouseTime &&
            now_ms < (m_previouseTime - 60 * 60 * 1000))
        {
            rollover = true;
        }
        m_previouseTime = now_ms;
        return rollover;
    }

    //# 6)销毁定时器管理器
    TimerManager::~TimerManager()
    {
    }
}