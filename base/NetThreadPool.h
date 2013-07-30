#ifndef BERT_NETTHREADPOOL_H
#define BERT_NETTHREADPOOL_H

#include <list>
#include <string>
#include <vector>
#include "./Threads/Thread.h"
#include "./Threads/ThreadPool.h"

class NetThreadPool;
class Socket;

//////////////////////////////////////////////////////////////////
class NetThread : public Runnable
{
private:
    Mutex      m_mutex;
    typedef std::vector<std::pair<SharedPtr<Socket>, enum IORequestType> > NEW_TASKS;
    NEW_TASKS  m_newTasks;
    volatile int m_newCnt;

    std::list<SharedPtr<Socket> > m_tasks;

    void    _AddSocket(SharedPtr<Socket> , enum IORequestType type = IO_READ_REQ);
    void    _OnExit();

#if defined(__gnu_linux__)
    // Epoll the sockets
    int m_epfd;

    // Epoll events
    std::vector<struct epoll_event> m_events;    
#else
    static HANDLE sm_iocp;
#endif

public:
    // Put a task to queue
    void AddSocket(SharedPtr<Socket>, enum IORequestType type = IO_READ_REQ);

#if defined(__gnu_linux__)
    void ModSocket(SharedPtr<Socket>, enum IORequestType type = IO_READ_REQ);
#endif

    //  Remove a task from task list
    void RemoveSocket(std::list<SharedPtr<Socket> >::iterator & iter);
    void RemoveSocket(SharedPtr<Socket>);

public:
    // Tasks per thread dealing
    static const size_t max_task_size = 512;

    // Constructor
    NetThread();
    
    // Destructor
    ~NetThread();
    
    // All tasks
    size_t TaskSize() const
    {
        return this->m_tasks.size() + this->m_newTasks.size();
    }

    // Max capacity per thread
    size_t MaxTaskSize() const { return max_task_size; }

    void Run();

public:
#if !defined(__gnu_linux__)
    enum
    {
        END_THREAD  = -1,
        END_RECV    = -2,
        END_SEND    = -3,
    };
    static bool CreateIOCP();
    static bool Shutdown();
#endif
};

///////////////////////////////////////////////
///////////////////////////////////////////////
class NetThreadPool
{
    class WorkerThreadPool : public ThreadPool
    {
    public:
        virtual Thread * GetThread();
        virtual Thread * GetThreadOptimal();
    } ;
    WorkerThreadPool m_netThreads;

    NetThreadPool();

public:
    bool AddSocket(SharedPtr<Socket> , enum IORequestType type = IO_READ_REQ);
    bool PrepareThreads(int num = 1);
    bool StartAllThreads();
    void Shutdown();
    int  Size()   { return  m_netThreads.Size();    }
    static NetThreadPool& Instance()
    {
        static  NetThreadPool pool;
        return  pool;
    }
};

#endif

