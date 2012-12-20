#if defined(__gnu_linux__)
    #include <unistd.h>
    #include <signal.h>
#endif

#include <cstring>
#include <cassert>
#include "Thread.h"
#include "../Log/Logger.h"

IdCreator32<Thread> Thread::sm_uniqueIdCreator;


Thread::Thread(Runnable* runnable) :
m_runnable(runnable),
m_handle(INVALID_HANDLE_VALUE),
m_tid(0),
m_mutex(1)
{
    Entry::id = sm_uniqueIdCreator.CreateID();
    m_working = false;
}

#if defined(__gnu_linux__)
void*           Thread::ThreadFunc(void* arg) 
#else
uint32_t WINAPI Thread::ThreadFunc(void* arg)
#endif
{
    Thread* pThread       = (Thread* ) arg;
    pThread->m_working    = true;
    pThread->m_tid        = Thread::GetCurrentThreadId();
    pThread->m_mutex.Post();

#if defined(__gnu_linux__)
    sigset_t mask;
    ::sigfillset(&mask);
    ::pthread_sigmask(SIG_SETMASK, &mask, NULL);
#endif

    LOCK_SDK_LOG
    DEBUG_SDK << "Start thread id " << (Thread::GetCurrentThreadId() & 0xFFFF);
    UNLOCK_SDK_LOG
    while (pThread->m_working)
    {
        pThread->_Run();
        pThread->m_runnable.Reset();
        pThread->Suspend();
    }

    LOCK_SDK_LOG
    DEBUG_SDK << "Exit thread id " << (Thread::GetCurrentThreadId() & 0xFFFF);
    UNLOCK_SDK_LOG

#if defined(__gnu_linux__)
    ::pthread_exit(0);
#else
    ::_endthreadex(0);
#endif

    return 0;
}

void Thread::Sleep( unsigned int seconds)
{
#if defined(__gnu_linux__)
    ::sleep(seconds);
#else
    ::Sleep(seconds * 1000);
#endif
}

void Thread::MSleep( unsigned int mSeconds)
{
#if defined(__gnu_linux__)
    ::usleep(mSeconds * 1000U);
#else
    ::Sleep(mSeconds);
#endif
}

bool Thread::Start()
{
    m_mutex.Wait(); 
    if (!m_working)
    {
        if (!Thread::_LaunchThread(m_handle, ThreadFunc, this))
        {
            m_mutex.Post();
            return false;
        }
    }
    else
    {
        m_mutex.Post();
    }

    return true;
}

bool Thread::_LaunchThread(THREAD_HANDLE& handle, PTHREADFUNC func, void* arg)
{
#if defined(__gnu_linux__)
        return 0 == ::pthread_create(&handle, NULL, func, arg);
#else
        handle = (THREAD_HANDLE)_beginthreadex(0, 0, func, arg, 0, 0);
        return 0 != handle && INVALID_HANDLE_VALUE != handle;
#endif
}

void Thread::_Run()
{
    if (NULL != m_runnable)
    {
        m_runnable->Run();
    }
}

void Thread::StopAndWait()
{
    m_working = false;
    if (m_runnable)
    {
        m_runnable->Stop();
    }

    Resume();
    Join();
}

void Thread::Join()
{
    if ((THREAD_HANDLE)INVALID_HANDLE_VALUE == m_handle)
        return;
#if defined(__gnu_linux__)
    ::pthread_join(m_handle, NULL);
#else
    ::WaitForSingleObject(m_handle, INFINITE);
    ::CloseHandle(m_handle);
#endif
    m_handle = INVALID_HANDLE_VALUE;
}

void Thread::Suspend()
{    
    assert(m_tid == Thread::GetCurrentThreadId());
    m_sem.Wait();
}

void Thread::Resume()
{    
    assert(m_tid != Thread::GetCurrentThreadId());
    m_sem.Post();
}

THREAD_ID Thread::GetCurrentThreadId()
{
#if defined(__gnu_linux__)
    return ::pthread_self();
#else
    return ::GetCurrentThreadId();
#endif
}

void Thread::YieldCPU()
{
#if defined(__gnu_linux__)
    ::usleep(100);
#else
    if (!::SwitchToThread())
        ::Sleep(1);
#endif
}
