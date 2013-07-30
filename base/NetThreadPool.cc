
#include "NetThreadPool.h"
#include "StreamSocket.h"
#include "ClientSocket.h"
#include "Log/Logger.h"
#include "SmartPtr/SharePtr.h"
#include <algorithm>

#if defined(__gnu_linux__)
    #include <sys/epoll.h>
    #include <errno.h>
#else
    #include "Server.h"
    HANDLE NetThread::sm_iocp = NULL;
#endif

NetThread::NetThread() : m_newCnt(0)
{
    LOCK_SDK_LOG
#if defined(__gnu_linux__)
    m_epfd = ::epoll_create(max_task_size);
    m_events.resize(max_task_size);
    DEBUG_SDK << "creat epollfd " << m_epfd;

#else
    DEBUG_SDK << "Create netThread";

#endif
   UNLOCK_SDK_LOG
}

NetThread::~NetThread()
{
    _OnExit();
#if defined(__gnu_linux__)
    LOCK_SDK_LOG
    DEBUG_SDK << "Try close epollfd " << m_epfd;
    UNLOCK_SDK_LOG
    TEMP_FAILURE_RETRY(::close(m_epfd));
#endif
}

void NetThread::AddSocket(SharedPtr<Socket> task, enum IORequestType type)
{
    ScopeMutex    guard(m_mutex);
    m_newTasks.push_back(std::make_pair(task, type));
    ++ m_newCnt;

    LOCK_SDK_LOG
    DEBUG_SDK << "Now thread " << (Thread::GetCurrentThreadId() & 0xffff)
              << " has new task " << m_newCnt << ", with request type " << (int)type;
    UNLOCK_SDK_LOG
}

void NetThread::_AddSocket(SharedPtr<Socket> task, enum IORequestType type)
{
    LOCK_SDK_LOG
    DEBUG_SDK << "add socket: fd " << task->m_localSock;
    UNLOCK_SDK_LOG
#if defined(__gnu_linux__)
    struct epoll_event event;
    event.events  = 0;
    event.data.fd = task->m_localSock;
    event.data.ptr= task.Get();

    if (type & (IO_READ_REQ | IO_ACCEPT_REQ))
    {
        event.events |= EPOLLIN;
    }

    if (type & (IO_WRITE_REQ | IO_CONNECT_REQ))
    {
        event.events |= EPOLLOUT;
    }

    if ((type & IO_ALL_REQ) == 0)
    {
        LOCK_SDK_LOG
        DEBUG_SDK << "FUCK: why no events? type = " << (int)type;
        UNLOCK_SDK_LOG

        event.events |= EPOLLIN;
    }

    if (0 != epoll_ctl(m_epfd, EPOLL_CTL_ADD, task->m_localSock, &event))
        assert(!"epoll ctl add error");

    task->SetNetThread(this);
#else
    HANDLE    iocp = ::CreateIoCompletionPort((HANDLE)task->m_localSock, sm_iocp, ULONG_PTR(0), 0);
    if (iocp != sm_iocp)
    {
        LOCK_SDK_LOG
        DEBUG_SDK << "Fatal ERRor, IOCP Add socket " << task->m_localSock
                  << ", return = " << (long)iocp
                  << ", error = "  << ::WSAGetLastError()
                  << ", s_iocp = " << (long)sm_iocp;
        UNLOCK_SDK_LOG
    }
    LOCK_SDK_LOG
    DEBUG_SDK << "IOCP Add socket " << task->m_localSock;
    UNLOCK_SDK_LOG

    if (type & IO_READ_REQ)
    {
        StreamSocket* pTCPTask = static_cast<StreamSocket* >(task.Get());
        if (!pTCPTask->BeginRecv() || SOCKET_ERROR == pTCPTask->Recv())
        {
            LOCK_SDK_LOG
            DEBUG_SDK << "First recv failed! socket = " << task->m_localSock
                      << ", error = " << WSAGetLastError();
            UNLOCK_SDK_LOG
            pTCPTask->OnError();
            pTCPTask->EndRecv();
            return;
        }
    }

    if (type & IO_CONNECT_REQ)
    {
        ClientSocket* pTask = static_cast<ClientSocket* >(task.Get());
        if (!pTask->PostConnect())
        {
            LOCK_SDK_LOG
            DEBUG_SDK << "First recv failed! socket = " << task->m_localSock
                      << ", error = " << WSAGetLastError();
            UNLOCK_SDK_LOG
            
            pTask->OnError();
            return;
        }
    }

    if (type & IO_ACCEPT_REQ)
    {
        assert (!!!"Windows use listen thread");
    }

    if (type & IO_WRITE_REQ)
    {
        assert (!!!"Windows never use write req");
    }
    
#endif
    m_tasks.push_back(task);

#if defined(__gnu_linux__)
    if (m_tasks.size() > m_events.size())
        m_events.resize(m_tasks.size() + 128);
#endif
}

#if defined(__gnu_linux__)
void NetThread::ModSocket(SharedPtr<Socket> task,  enum IORequestType type)
{
    struct epoll_event event;
    event.events  = 0;
    event.data.fd = task->m_localSock;
    event.data.ptr= task.Get();
   
    LOCK_SDK_LOG
    DEBUG_SDK << "mod : fd " << task->m_localSock;

    if (type & (IO_READ_REQ | IO_ACCEPT_REQ))
    {
        DEBUG_SDK << "new event EPOLLIN";
        event.events |= EPOLLIN;
    }

    if (type & (IO_WRITE_REQ | IO_CONNECT_REQ))
    {
        DEBUG_SDK << "new event EPOLLOUT";
        event.events |= EPOLLOUT;
    }
  
    UNLOCK_SDK_LOG

    if (0 != epoll_ctl(m_epfd, EPOLL_CTL_MOD, task->m_localSock, &event ) )
        assert(!"epoll ctl modify error");
}
#endif

void NetThread::RemoveSocket(std::list<SharedPtr<Socket> >::iterator & iter)
{
#if defined(__gnu_linux__)
    struct epoll_event dummy;
    if (0 != epoll_ctl(m_epfd, EPOLL_CTL_DEL, (*iter)->m_localSock, &dummy))
        assert(!"epoll ctl del error");
#endif

    iter = m_tasks.erase(iter);
}


void NetThread::RemoveSocket(SharedPtr<Socket> pSock)
{
    std::list<SharedPtr<Socket> >::iterator it = std::find(m_tasks.begin(), m_tasks.end(), pSock);
    assert(it != m_tasks.end());
    RemoveSocket(it);
}


void  NetThread::_OnExit()
{
    LOCK_SDK_LOG
        DEBUG_SDK << __FUNCTION__;
    UNLOCK_SDK_LOG
    m_mutex.Lock();
    NEW_TASKS::const_iterator    iter(m_newTasks.begin());
    for (; iter != m_newTasks.end(); ++ iter)
    {
        LOCK_SDK_LOG
        DEBUG_SDK << "Remove from buffer task, fd = " << iter->first->m_localSock;
        UNLOCK_SDK_LOG
        iter->first->m_invalid = true;
    }
    m_newTasks.clear();
    m_mutex.Unlock();

    for (std::list<SharedPtr<Socket> >::iterator it = m_tasks.begin(); it != m_tasks.end(); )
    {
        SharedPtr<Socket>    pSocket  = *it;
        StreamSocket*       pTask    = NULL;

        if (pSocket->GetSocketType() == Socket::STREAMSOCKET)
        {
            pTask = static_cast<StreamSocket*>(pSocket.Get());
        }

#if !defined(__gnu_linux__)
        if (pTask && pTask->IsIOPending())
        {
            pTask->_FinalSync();
            Socket::CloseSocket(pTask->m_localSock);
            while (pTask->IsIOPending() && NULL != sm_iocp)
            {
                Thread::YieldCPU();
            }

            pTask->m_invalid = true;
        }
#endif
        RemoveSocket(it);
    }
}


void NetThread::Run( )
{
    std::list<SharedPtr<Socket> >::iterator    it;
    NEW_TASKS::const_iterator    iter, end;
   
    while (IsAlive())
    {
        if (m_newCnt > 0 &&
            m_mutex.TryLock())
        {
            NEW_TASKS    tmp_tasks;
            m_newTasks.swap(tmp_tasks);
            m_newCnt = 0;
            m_mutex.Unlock();

			iter = tmp_tasks.begin();    
			end  = tmp_tasks.end();

			for (; iter != end; ++ iter)                
				_AddSocket(iter->first, iter->second);
        }

        for (it = m_tasks.begin(); it != m_tasks.end(); )
        {
            SharedPtr<Socket> pSocket = *it;
            StreamSocket*    pTask   = NULL;

            if (Socket::STREAMSOCKET == pSocket->GetSocketType())
            {
                pTask = static_cast<StreamSocket* >(pSocket.Get());

                if (!pSocket->Invalid() && !pTask->Send())
                    pTask->OnError();
            }

            if (pSocket->Invalid())
            {
#if defined(__gnu_linux__)
                LOCK_SDK_LOG
                DEBUG_SDK << "!!!Recycle fd " << pSocket->m_localSock;
                UNLOCK_SDK_LOG
                RemoveSocket(it);
#else
                bool bPending = false;
                if (pTask)
                {
                    bPending = pTask->IsIOPending();
                }

                if (bPending)
                {                
                    pTask->_FinalSync();
                    Socket::CloseSocket(pSocket->m_localSock);
                    ++ it;
                }
                else
                {
                    LOCK_SDK_LOG
                    DEBUG_SDK << "!!!Recycle fd " << pSocket->m_localSock;
                    UNLOCK_SDK_LOG
                    RemoveSocket(it);
                }
#endif
            }
            else
            {
                ++it;
            }
        }

#if defined(__gnu_linux__)
        if  (m_tasks.empty())
        {
            Thread::MSleep(10);
            continue;
        }

        int ret = ::epoll_wait(m_epfd, &m_events[0], m_tasks.size(), 5);
        for (int i=0; i<ret; ++i)
        {
            Socket*    pSocket = (Socket* )m_events[i].data.ptr;
            
            LOCK_SDK_LOG
            if ((m_events[i].events & EPOLLHUP) && !(m_events[i].events & EPOLLIN))
            {
                DEBUG_SDK << "hup fd = " << pSocket->m_localSock << ", reason " << errno;
                pSocket->OnError();
            }

            if (m_events[i].events & EPOLLERR)
            {
                DEBUG_SDK << "error fd = " << pSocket->m_localSock << ", reason " << errno;
                pSocket->OnError();
            }

            if (m_events[i].events & (EPOLLIN | EPOLLPRI))
            {
                if (!pSocket->OnReadable())
                {
                    DEBUG_SDK << "error4 read. task fd = " << pSocket->m_localSock << ", errno " << errno;
                    pSocket->OnError();
                }
            }
            
            if (m_events[i].events & EPOLLOUT)
            {
                if (!pSocket->OnWritable())
                {
                    DEBUG_SDK << "error send. task fd = " << pSocket->m_localSock << ", errno " << errno;
                    pSocket->OnError();
                }
            }
            UNLOCK_SDK_LOG
        }
#else
        DWORD  bytes = 0;

        OverlappedStruct*   pOvdata = NULL;
        ULONG_PTR           pDummy  = NULL;

        BOOL bOK = ::GetQueuedCompletionStatus(sm_iocp, &bytes, &pDummy, (LPOVERLAPPED*)&pOvdata, 5);

        if (FALSE == bOK)
        {
            int error = ::WSAGetLastError();

            if (NULL != pOvdata)
            {
                StreamSocket* pSocket = static_cast<StreamSocket* >(pOvdata->m_pSocket);

                LOCK_SDK_LOG
                switch (error)
                {
                case ERROR_OPERATION_ABORTED:
                    DEBUG_SDK << "Closed socket or cancel IO, but still has pending io";                    
                    break;

                case ERROR_NETNAME_DELETED:
                    DEBUG_SDK << "Peer socket force close, my socket is " << pSocket->m_localSock;
                    break;

                default:
                    DEBUG_SDK << "Not null overlapped data, error is " << error;
                    break;
                }
                UNLOCK_SDK_LOG

                pSocket->OnError();
                if (IO_RECV == pOvdata->m_event ||
                    IO_RECV_ZERO == pOvdata->m_event)
                {
                    pSocket->EndRecv();
                    LOCK_SDK_LOG
                    DEBUG_SDK << "EndRecv socket " << pSocket->m_localSock
                              << ", now recv overlappe is complete ? "
                              << HasOverlappedIoCompleted(&pOvdata->m_overlap);
                    UNLOCK_SDK_LOG
                }
                else if (IO_SEND == pOvdata->m_event)
                {
                    pSocket->EndSend();
                }
                else if (IO_CONNECT == pOvdata->m_event)
                {
                    LOCK_SDK_LOG
                    DEBUG_SDK << "Error client socket, error = " << ::WSAGetLastError();
                    UNLOCK_SDK_LOG
                }
                else
                {
                    assert (false);
                }
            }
            else
            {
                if (WAIT_TIMEOUT != error)
                {
                    LOCK_SDK_LOG
                    DEBUG_SDK << "Fatal IOCP error : " << error;
                    UNLOCK_SDK_LOG
                    break;
                }
            }
        }
        else
        {
            if (END_THREAD == bytes)
            {
                LOCK_SDK_LOG
                DEBUG_SDK << "Request exit!";
                UNLOCK_SDK_LOG
                break;
            }

            StreamSocket* pSocket = static_cast<StreamSocket* >(pOvdata->m_pSocket);

            switch (pOvdata->m_event)
            {
            case IO_RECV:
                LOCK_SDK_LOG
                DEBUG_SDK << "Worker thread [" << (Thread::GetCurrentThreadId()&0xffff) << "], OK! recv bytes = " << bytes << " socket = " << pSocket->m_localSock;
                UNLOCK_SDK_LOG

                pSocket->m_ovRecv.m_bytes = bytes;
                if (!pSocket->OnReadable())
                {
                    pSocket->OnError();        
                    pSocket->EndRecv();
                    DEBUG_SDK << "2EndRecv socket " << pSocket->m_localSock
                        << ", now recv overlappe is complete ? "
                        << HasOverlappedIoCompleted(&pOvdata->m_overlap);
                    UNLOCK_SDK_LOG
                }

                break;

            case IO_RECV_ZERO:
                assert(0 == bytes);
                pSocket->m_ovRecv.m_bytes = bytes;
                if (SOCKET_ERROR == pSocket->Recv())
                {
                    pSocket->OnError();
                    pSocket->EndRecv();
                    LOCK_SDK_LOG
                    DEBUG_SDK << "3EndRecv socket " << pSocket->m_localSock
                        << ", now recv overlappe is complete ? "
                        << HasOverlappedIoCompleted(&pOvdata->m_overlap);
                    UNLOCK_SDK_LOG
                }
                
                break;
            
            case IO_SEND:
                pSocket->m_ovSend.m_bytes = bytes;
                LOCK_SDK_LOG
                DEBUG_SDK << "Worker thread [" << (Thread::GetCurrentThreadId()&0xffff) << "], OK! send bytes = " << bytes << " socket = " << pSocket->m_localSock;
                UNLOCK_SDK_LOG
                if (!pSocket->OnWritable())
                    pSocket->OnError();
                pSocket->EndSend();
                
                break;

            case IO_CONNECT:
                if (!pOvdata->m_pSocket->OnWritable())
                     pOvdata->m_pSocket->OnError();
                break;

            default:
                assert (!!!"Error event type");
                break;
            }
        }
#endif
    }

    _OnExit();
}

Thread* NetThreadPool::WorkerThreadPool::GetThread()
{
    NetThread*    pEntity = NULL;
    
    ScopeMutex        guard(m_threadsLock);
    ThreadIterator    it(m_threads.begin());
    for (; it != m_threads.end(); ++ it)
    {
        if ((*it)->Idle())
            continue;

        pEntity = static_cast<NetThread *>((*it)->GetRunnableEntity());

        if (pEntity != NULL && pEntity->TaskSize() < pEntity->MaxTaskSize())
        {
            return  *it;
        }
    }

    return NULL;
}

Thread* NetThreadPool::WorkerThreadPool::GetThreadOptimal()
{
    NetThread*    pEntity = NULL;
    Thread*       pThread = NULL;

    ScopeMutex        guard(m_threadsLock);
    ThreadIterator    it(m_threads.begin());
    while (it != m_threads.end())
    {
        if (!(*it)->Idle())
        {
            pEntity = static_cast<NetThread *>((*it)->GetRunnableEntity());
            if (pEntity != NULL && pEntity->TaskSize() < pEntity->MaxTaskSize())
            {
                if (NULL == pThread)
                {
                    pThread = *it;
                }
                else 
                {
                    NetThread* current = static_cast<NetThread *>(pThread->GetRunnableEntity());
                    if (current->TaskSize() > pEntity->TaskSize())
                        pThread = *it;
                }
            }
        }
        
        ++ it;
    }

    return pThread;
}

#if !defined(__gnu_linux__)
bool NetThread::CreateIOCP()
{
    if (!sm_iocp)
    {
        sm_iocp = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    }

    return sm_iocp != 0;
}

bool NetThread::Shutdown()
{
    BOOL ret = FALSE;
    if (NULL != sm_iocp)
    {
        const int nThread = NetThreadPool::Instance().Size();
        for (int i = 0; i < nThread; ++ i)
        {
            ::PostQueuedCompletionStatus(sm_iocp, END_THREAD, 0, 0);
        }
        ret = ::CloseHandle(sm_iocp);
        sm_iocp = 0;

        LOCK_SDK_LOG
        DEBUG_SDK << "shutdown IOCP";
        UNLOCK_SDK_LOG
    }

    return TRUE == ret;
}

#endif

NetThreadPool::NetThreadPool()
{
#if !defined(__gnu_linux__)
    if (!NetThread::CreateIOCP())
    {
        Server::Instance()->Terminate();
    }

#endif
}

bool NetThreadPool::AddSocket(SharedPtr<Socket> sock, enum IORequestType type)
{
    Thread* pThread = m_netThreads.GetThread();
    if (pThread != NULL)
    {
        NetThread* pNetThread = static_cast<NetThread* >(pThread->GetRunnableEntity());
        pNetThread->AddSocket(sock, type);
        return true;
    }
    
    LOCK_SDK_LOG
    DEBUG_SDK << "my god, cannot find net thread";
    UNLOCK_SDK_LOG
   
    return false;
}

bool NetThreadPool::PrepareThreads(int num)
{
    for (int i = 0; i < num; ++ i)
    {
        if (!m_netThreads.PrepareThread(new NetThread()))
            return false;
    }
    return true;
}

bool NetThreadPool::StartAllThreads()
{
    return m_netThreads.StartAllThreads();
}


void NetThreadPool::Shutdown()
{    
#if !defined(__gnu_linux__)
    NetThread::Shutdown();

#endif

    m_netThreads.StopAllThreads();
}
