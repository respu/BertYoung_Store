#ifndef BERT_THREAD_H
#define BERT_THREAD_H

#include  <string>
#include  "Atomic.h"
#include  "../CommonDef.h"
#include  "../SmartPtr/UniquePtr.h"
#include  "../EntryManager.h"
#include  "IPC.h"

#if defined(__gnu_linux__)
    #include <pthread.h>
    typedef pthread_t   THREAD_HANDLE;
    typedef pthread_t   THREAD_ID;
    #define INVALID_HANDLE_VALUE   (-1)
#else
    #include  <WinBase.h>
    #include  <process.h>
    typedef HANDLE   THREAD_HANDLE;
    typedef DWORD    THREAD_ID;
#endif

class Runnable
{
protected:
    volatile bool  m_running;

public:
    Runnable() :  m_running(true)
    {
    }

    virtual  ~Runnable()  { }

    virtual void  Run() = 0;
    bool   IsAlive() const {  return m_running;   }
    void   Stop()          {  m_running = false;  }
};



class Thread : public Entry
{
    friend class ThreadPool;

     // Unique id for threads
    static IdCreator32<Thread> sm_uniqueIdCreator;

    UniquePtr<Runnable>  m_runnable;

    THREAD_HANDLE m_handle;
    THREAD_ID     m_tid;

    Semaphore     m_mutex;
    Semaphore     m_sem;

    volatile bool m_working;

#if defined(__gnu_linux__)
    typedef void * (*PTHREADFUNC)(void* ); 
    static  void * ThreadFunc(void* );
#else
    typedef uint32_t (WINAPI *PTHREADFUNC)(void* ); 
    static  uint32_t  WINAPI ThreadFunc(void* );
#endif

public:
    explicit Thread(Runnable* = NULL);

    void      SetRunnableEntity(Runnable* r)  { m_runnable.Reset(r);    }
    Runnable* GetRunnableEntity() const       { return m_runnable.Get(); }

    THREAD_ID     GetThreadID()     { return m_tid; }
    THREAD_HANDLE GetThreadHandle() { return m_handle; }
    
    static void   Sleep(unsigned int seconds);
    static void   MSleep(unsigned int mSeconds);
    
    bool   Start();
    bool   Idle() const   {  return NULL == m_runnable.Get(); }

private:
    void   _Run();
    static bool  _LaunchThread(THREAD_HANDLE& handle, PTHREADFUNC func, void* arg);

public:    
    void   StopMe();
    void   Join();
    
    void   Suspend();
    void   Resume();

    static THREAD_ID   GetCurrentThreadId();
    static void        YieldCPU();

private:
    Thread(const Thread& );

    Thread& operator= (const Thread& );
};


#endif
