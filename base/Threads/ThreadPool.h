
#ifndef BERT_THREADPOOL_H
#define BERT_THREADPOOL_H

#include <set>
#include "IPC.h"

class Thread;
class Runnable;

class ThreadPool
{
protected:
    Mutex               m_threadsLock; 
    std::set<Thread* >  m_threads;

    typedef std::set<Thread* >::iterator ThreadIterator;
public:
    virtual ~ThreadPool() {  }

    virtual Thread* GetThread() { return NULL; }
    virtual Thread* GetThreadOptimal() { return NULL; }

    bool PrepareThread(Runnable* toRun);
    bool StartAllThreads();
    bool ExecuteTask(Runnable* toRun);
    void StopAllThreads();
    int  Size();
};

#endif

