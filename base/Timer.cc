#include <cstdlib>
#include <cassert>
#include <cstdio>
#include "Timer.h"


Time     g_now;

Time::Time() : m_ms(0), m_valid(false)
{
    this->Now();
}

Time::Time(const Time & other) : m_ms(other.m_ms)
{
    m_valid = false;
}

void Time::Now() const
{
#if defined(__gnu_linux__)
    struct timeval now;
    ::gettimeofday(&now, 0);
    m_ms = (unsigned long long)now.tv_sec * 1000UL + now.tv_usec/1000UL;
#else
    struct _timeb now;
    ::_ftime64_s(&now);
    m_ms = now.time * 1000UL + now.millitm;
#endif
    m_valid = false;
}

const char * Time::FormatTime(char* buf, unsigned int maxSize) const
{
    if (NULL != buf && maxSize > 0)
    {
        _SET_TM_;
#if defined(__gnu_linux__)
        ;//strftime(buf, maxSize, "%F[%T]", &m_tm);
#else
        ;//strftime(buf, maxSize, "%Y-%m-%d[%H_%M_%S]", &m_tm);
#endif
        snprintf(buf, maxSize, "%04d-%02d-%02d[%02d_%02d_%02d_%03ld]",
                m_tm.tm_year+1900, m_tm.tm_mon+1, m_tm.tm_mday,
                m_tm.tm_hour, m_tm.tm_min, m_tm.tm_sec, static_cast<long>(m_ms % 1000));

        return buf;
    }
    else
    {
        return NULL;
    }
}

void Time::AddDelay(unsigned long delay)
{
    m_ms   += delay;
    m_valid = false;
}

Time& Time::operator= (const Time & other)
{
    if (this != &other)
    {
        m_ms    = other.m_ms;
        m_valid = false;
    }
    return *this;
}




Timer::Timer(uint32_t interval, int32_t count) :
m_interval(interval),
m_count(count)
{
    m_next = m_prev = NULL;
    m_nextTriggerTime.AddDelay(interval);
}

bool Timer::OnTimer()
{
    if (m_count < 0 || -- m_count >= 0)
    {
        m_nextTriggerTime.AddDelay(m_interval);
        return  _OnTimer();
    }

    return false;        
};



TimerManager::~TimerManager()
{
    for (int i = 0; i < LIST1_SIZE; ++ i)
    {
        Timer*   pTimer;
        while (NULL != (pTimer = m_list1[i].m_next))
        {
            KillTimer(pTimer);
        }
    }

    for (int i = 0; i < LIST_SIZE; ++ i)
    {
        Timer*   pTimer;
        while (NULL != (pTimer = m_list2[i].m_next))
        {
            KillTimer(pTimer);
        }

        while (NULL != (pTimer = m_list3[i].m_next))
        {
            KillTimer(pTimer);
        }

        while (NULL != (pTimer = m_list4[i].m_next))
        {
            KillTimer(pTimer);
        }

        while (NULL != (pTimer = m_list5[i].m_next))
        {
            KillTimer(pTimer);
        }
    }
}

void TimerManager::UpdateTimers()
{
    Time  now;
    while (m_thisCheckTime <= now)
    {
        int index = m_thisCheckTime & (LIST1_SIZE - 1);
        if (0 == index &&
            !_Cacsade(m_list2, _Index(0)) &&
            !_Cacsade(m_list3, _Index(1)) &&
            !_Cacsade(m_list4, _Index(2)))
        {
            _Cacsade(m_list5, _Index(3));
        }

        m_thisCheckTime.AddDelay(1);

        Timer*   pTimer;
        while (NULL != (pTimer = m_list1[index].m_next))
        {
            KillTimer(pTimer);
            if (pTimer->OnTimer())
                AddTimer(pTimer);
        }
    }        
}


void TimerManager::AddTimer(Timer* pTimer)
{
    uint32_t diff      =  static_cast<uint32_t>(pTimer->m_nextTriggerTime - m_thisCheckTime);
    Timer*   pListHead =  NULL;
    uint64_t trigTime  =  pTimer->m_nextTriggerTime.MilliSeconds();

    if (static_cast<int32_t>(diff) < 0)
    {
        pListHead = &m_list1[m_thisCheckTime.MilliSeconds() & (LIST1_SIZE - 1)];
    }
    else if (diff < static_cast<uint32_t>(LIST1_SIZE))
    {
        pListHead = &m_list1[trigTime & (LIST1_SIZE - 1)];
    }
    else if (diff < 1 << (LIST1_BITS + LIST_BITS))
    {
        pListHead = &m_list2[(trigTime >> LIST1_BITS) & (LIST_SIZE - 1)];
    }
    else if (diff < 1 << (LIST1_BITS + 2 * LIST_BITS))
    {
        pListHead = &m_list3[(trigTime >> (LIST1_BITS + LIST_BITS)) & (LIST_SIZE - 1)];
    }
    else if (diff < 1 << (LIST1_BITS + 3 * LIST_BITS))
    {
        pListHead = &m_list4[(trigTime >> (LIST1_BITS + 2 * LIST_BITS)) & (LIST_SIZE - 1)];
    }
    else
    {
        pListHead = &m_list5[(trigTime >> (LIST1_BITS + 3 * LIST_BITS)) & (LIST_SIZE - 1)];
    }

    // push front
    assert(NULL == pListHead->m_prev);
    pTimer->m_prev = pListHead;
    pTimer->m_next = pListHead->m_next;
    if (NULL != pListHead->m_next)
        pListHead->m_next->m_prev = pTimer;
    pListHead->m_next = pTimer;
}


void TimerManager::KillTimer(Timer* pTimer)
{
    if (NULL != pTimer && NULL != pTimer->m_prev)
    {
        pTimer->m_prev->m_next = pTimer->m_next;

        if (NULL != pTimer->m_next)
            pTimer->m_next->m_prev = pTimer->m_prev;

        pTimer->m_prev = pTimer->m_next = NULL;
    }
}

bool TimerManager::_Cacsade(Timer pList[], int index)
{
    if (NULL == pList || index < 0 || index > LIST_SIZE || NULL == pList[index].m_next)
        return false;

    Timer*   pTimer;
    while (NULL != (pTimer = pList[index].m_next))
    {
        KillTimer(pTimer);
        AddTimer(pTimer);
    }

    return true;
}
