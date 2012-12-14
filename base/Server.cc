#include <signal.h>
#include <ctime>
#include <cstdlib>
#include "Server.h"
#include "StreamSocket.h"
#include "ListenSocket.h"
#include "ClientSocket.h"
#include "Log/Logger.h"
#include "NetThreadPool.h"

static void int_handler(int signum)
{
    if (Server::Instance() != NULL)
        Server::Instance()->Terminate();

#if !defined(__gnu_linux__)
    ::signal(signum, int_handler);
#endif
}


Server* Server::sm_instance = NULL;

#if !defined(__gnu_linux__)
bool Server::_StartListen()
{
    return m_thread.Start();
}

void Server::_StopListen()
{
    m_thread.StopMe();
    m_thread.Resume();
    m_thread.Join();
}
#endif

Server::Server() : m_bTerminate(false)
{
    assert(NULL == sm_instance);
        
    sm_instance = this;
    
#if !defined(__gnu_linux__)
    m_thread.SetRunnableEntity(new ListenThread);
#endif

#ifdef  DEBUG_BERT_SDK
    g_sdkLog = LogManager::Instance().CreateLog(Logger::logDEBUG, Logger::logFILE, "./sdk_debug_log");
    LOCK_SDK_LOG;
    DEBUG_SDK << "Create g_sdk Log!";
    UNLOCK_SDK_LOG;
#endif
}

Server::~Server()
{
    sm_instance = NULL;
}

void Server::_Recycle()
{    
#if !defined(__gnu_linux__)
    Server::_StopListen();

#endif
    NetThreadPool::Instance().Shutdown();
    m_tasks.Clear();
    LogManager::Instance().StopLog();
}

bool Server::_Bind(int port)
{
    SharedPtr<ListenSocket> pServerSocket(new ListenSocket);
    if (!pServerSocket.Get() ||
        !pServerSocket->Bind(port))
    {
        return false;
    }

#if !defined(__gnu_linux__)
    ListenThread* pThread = static_cast<ListenThread*>(m_thread.GetRunnableEntity());
    
    return  pThread != NULL && pThread->AddSocket(pServerSocket);
#else
    return  true;

#endif
}

bool Server::_Connect(const char* ip, int port)
{
    SharedPtr<ClientSocket> pClient(new ClientSocket);

    return  pClient.Get() && pClient->Connect(ip, port);
}

void Server::MainLoop()
{
#if defined(__gnu_linux__)
    // handle signals
    struct sigaction sig;
    ::memset(&sig, 0, sizeof sig);
    sig.sa_handler = int_handler;
    ::sigaction(SIGINT, &sig, NULL);
    ::sigaction(SIGQUIT, &sig, NULL);
    ::sigaction(SIGABRT, &sig, NULL);
    ::sigaction(SIGTERM, &sig, NULL);
    ::sigaction(SIGHUP, &sig, NULL);

    sig.sa_handler = SIG_IGN;
    ::sigaction(SIGPIPE, &sig, NULL);

#else
    ::signal(SIGINT,  int_handler);
    ::signal(SIGBREAK,int_handler);
    ::signal(SIGABRT, int_handler);

    WSADATA data;
    if (0 != ::WSAStartup(MAKEWORD(2, 2), &data))
        return;

    if (LOBYTE(data.wVersion) != 2 ||
        HIBYTE(data.wVersion) != 2 ) 
    {
        ::WSACleanup();
        return; 
    }

#endif

    ::srand(static_cast<unsigned int>(time(NULL)));

    if (_Init())
    {
        while (!m_bTerminate)
        {
            _RunLogic();
        }
    }
    _Recycle();

#if !defined(__gnu_linux__)
    ::WSACleanup();
#endif
}


void  Server::NewConnection(SOCKET connfd)
{
    if (INVALID_SOCKET == connfd)
        return;

    SharedPtr<StreamSocket>  pNewTask = _OnNewConnection(connfd);
    
    if (pNewTask.Get() == NULL)
        return;
    
    if (NetThreadPool::Instance().AddSocket(pNewTask))
        m_tasks.AddTask(pNewTask);
}
