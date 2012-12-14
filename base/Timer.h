
#ifndef BERT_TIMER_H
#define BERT_TIMER_H

#include <ctime>
#include "CommonDef.h"
#include "./Threads/Atomic.h"

#if defined(__gnu_linux__)
    #include <sys/time.h>
#else
    #include <sys/timeb.h>
#endif

class Time
{
    // milliseconds from 1970
    mutable uint64_t m_ms; 

    // for print time and date
    mutable struct tm m_tm;

    mutable bool m_valid;

#if defined(__gnu_linux__)
    #define _SET_TM_  \
    do { \
        if (!m_valid) \
        { \
            m_valid = true; \
            const time_t _now(m_ms / 1000UL); \
            ::localtime_r(&_now, &m_tm);\
        } \
    } while(0)

#else
    #define _SET_TM_  \
    do { \
        if (!m_valid) \
        { \
            m_valid = true; \
            const time_t _now(m_ms / 1000UL); \
            ::localtime_s(&m_tm, &_now);\
        } \
    } while(0)
#endif

public:
    Time();
    Time(const Time& other);

    void Now() const;
    uint64_t MilliSeconds() const { return m_ms; }

    int GetMonth()  const { _SET_TM_; return m_tm.tm_mon;  } 
    int GetDay()    const { _SET_TM_; return m_tm.tm_mday; }
    int GetHour()   const { _SET_TM_; return m_tm.tm_hour; }
    int GetMinute() const { _SET_TM_; return m_tm.tm_min;  }
    int GetSecond() const { _SET_TM_; return m_tm.tm_sec;  }

    const char* FormatTime(char* buf, unsigned int maxSize) const;

    void AddDelay(unsigned long delay);

    Time& operator= (const Time& );

    operator uint64_t() const { return m_ms; }
    
    friend bool operator< (const Time& l, const Time& r) {    return l.m_ms < r.m_ms;   }
    friend bool operator== ( const Time& l, const Time& r)   {    return l.m_ms == r.m_ms;  }
    friend bool operator<= (const Time& l, const Time& r)    {     return l < r || l == r;   }
};


extern  Time     g_now;


class Timer
{
    friend class TimerManager;
public:
    explicit Timer(uint32_t interval = uint32_t(-1), int32_t count = -1);
    virtual ~Timer() {}
    bool     OnTimer();

private:
    Timer*   m_next;
    Timer*   m_prev;
    Time     m_nextTriggerTime;
    uint32_t m_interval;
    int32_t  m_count;
    virtual bool _OnTimer() { return false; }
};


class TimerManager
{
private:
    TimerManager() {}

public:

    static  TimerManager* Instance()
    {
        static  TimerManager   mgr;
        return  &mgr;
    }

    ~TimerManager();
    void UpdateTimers();
    void AddTimer(Timer* pTimer);
    void KillTimer(Timer* pTimer);

private:

    // false if noop
    bool _Cacsade(Timer pList[], int index);
    int  _Index(int level);

    static const int LIST1_BITS = 8;
    static const int LIST_BITS  = 6;
    static const int LIST1_SIZE = 1 << LIST1_BITS;
    static const int LIST_SIZE  = 1 << LIST_BITS;

    Time  m_thisCheckTime;

    Timer m_list1[LIST1_SIZE]; // 256 ms * ACCURACY
    Timer m_list2[LIST_SIZE];  // 64 * 256ms = 16秒
    Timer m_list3[LIST_SIZE];  // 64 * 64 * 256ms = 17分钟
    Timer m_list4[LIST_SIZE];  // 64 * 64 * 64 * 256ms = 18 小时
    Timer m_list5[LIST_SIZE];  // 64 * 64 * 64 * 64 * 256ms = 49 天
};

inline int TimerManager::_Index(int level)
{
    uint64_t current = m_thisCheckTime.MilliSeconds();
    current >>= (LIST1_BITS + level * LIST_BITS);
    return  current & (LIST_SIZE - 1);
}

#endif
