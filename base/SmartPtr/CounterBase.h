#ifndef BERT_COUNTERBASE_H
#define BERT_COUNTERBASE_H

#ifdef SMART_PTR
#include <iostream>
#endif
#include "../Threads/Atomic.h"

class CounterBase
{
public:
    CounterBase() : m_shareCnt(1), m_weakCnt(1)
    {
#ifdef SMART_PTR
        std::cout << __FUNCTION__ << std::endl;
#endif
    }

    virtual ~CounterBase()
    {
    }

    virtual void Dispose() = 0;

    void AddShareCnt()
    {
        AtomicChange(&m_shareCnt, 1);
    }

    void DecShareCnt()
    {
        if (1 == AtomicChange(&m_shareCnt, -1))
        {
            Dispose();
            DecWeakCnt();
        }
    }

    void AddWeakCnt()
    {
        AtomicChange(&m_weakCnt, 1);
    }

    void DecWeakCnt()
    {
        if (1 == AtomicChange(&m_weakCnt, -1))
            Destroy();
    }

    bool  AddShareCopy()
    {
        if (0 == m_shareCnt)
            return false;

        AddShareCnt();
        return true;
    }

    void Destroy()
    {        
#ifdef SMART_PTR
        std::cout << __FUNCTION__ << std::endl;
#endif
        delete this;
    }

    int UseCount() const 
    {
        return m_shareCnt;
    }

    int WeakCount() const 
    {
        return m_weakCnt;
    }

private:
    volatile int m_shareCnt;
    volatile int m_weakCnt;

    CounterBase(const CounterBase& );
    CounterBase& operator= (const CounterBase& );
};

#endif

