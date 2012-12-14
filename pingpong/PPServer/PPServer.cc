#include "PPServer.h"
#include "../../base/Log/Logger.h"
#include "../../base/NetThreadPool.h"
#include "../../base/Timer.h"
#include "ClientTask.h"



PPServer::PPServer()
{
    m_pLog = NULL;
}

PPServer::~PPServer()
{
}

SharedPtr<StreamSocket>   PPServer::_OnNewConnection(SOCKET connfd)
{
    DBG(m_pLog) << "New tcp task " << connfd;

    SocketAddr localAddr, peerAddr;
    Socket::GetLocalAddr(connfd, localAddr);
    Socket::GetPeerAddr(connfd,  peerAddr);

    SharedPtr<ClientTask>    pNewTask(new ClientTask());
    if (!pNewTask->Init(connfd, localAddr.GetPort(), peerAddr.GetPort()))
        pNewTask.Reset();

    return  pNewTask;
}

bool PPServer::_Init()
{
    m_pLog = LogManager::Instance().CreateLog(Logger::logALL, Logger::logALL);

    WRN(m_pLog) << "First PPServer log";

#if defined(__gnu_linux__)
    if (!NetThreadPool::Instance().PrepareThreads(1))
#else
    if (!NetThreadPool::Instance().PrepareThreads(NetThread::GetNumOfCPU()))
#endif
    {
        ERR(m_pLog) << "can not create threads";
        return false;
    }

    if (!PPServer::_Bind(8888))
    {
        ERR(m_pLog) << "can not bind socket";
        return false;
    }

    NetThreadPool::Instance().StartAllThreads();

#if !defined(__gnu_linux__)
    if (!Server::_StartListen())
    {
        return false;
    }

#endif

    return  LogManager::Instance().StartLog();
}


bool PPServer::_RunLogic()
{
    g_now.Now();

    if (!Server::_RunLogic())   
        Thread::MSleep(1);

    return  true;
}


int main()
{
    UniquePtr<PPServer> pSvr(new PPServer);
    pSvr->MainLoop();

    return 0;
}
