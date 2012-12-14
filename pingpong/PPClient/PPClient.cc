#include "PPClient.h"
#include "../../base/Log/Logger.h"
#include "../../base/NetThreadPool.h"
#include "ServerTask.h"
#include "../../base/Timer.h"


PPClient::PPClient()
{
    m_pLog = NULL;
}

PPClient::~PPClient()
{
}

SharedPtr<StreamSocket>   PPClient::_OnNewConnection(SOCKET connfd)
{
    DBG(m_pLog) << "New tcp task " << connfd;

    SocketAddr  localAddr,  peerAddr;
    Socket::GetLocalAddr(connfd, localAddr);
    Socket::GetPeerAddr(connfd,  peerAddr);

    SharedPtr<ServerTask>    pNewTask(new ServerTask());
    if (!pNewTask->Init(connfd, localAddr.GetPort(), peerAddr.GetPort()))
    {
        pNewTask.Reset();
    }
    else
    {
        pNewTask->SendMsg(); // FOR PING_PONG TEST
    }

    return  pNewTask;
}

bool PPClient::_Init()
{
    m_pLog = LogManager::Instance().CreateLog(Logger::logALL, Logger::logALL);

#if defined(__gnu_linux__)
    if (!NetThreadPool::Instance().PrepareThreads(1))
#else
    if (!NetThreadPool::Instance().PrepareThreads(NetThread::GetNumOfCPU()))
#endif
    {
        ERR(m_pLog) << "can not create threads";
        return false;
    }

    if (!Server::_Connect("127.0.0.1", 8888))
    {
        ERR(m_pLog) << "can not connect server 8888";
        return false;
    }

    if (!NetThreadPool::Instance().StartAllThreads())
    {       
        ERR(m_pLog) << "can not start threads";
        return false;
    }

#if !defined(__gnu_linux__)
    if (!Server::_StartListen())
    {
        return false;
    }

#endif

    return  LogManager::Instance().StartLog();
}

bool PPClient::_RunLogic()
{
    g_now.Now();

    if (!Server::_RunLogic())
        Thread::MSleep(1);

    return  true;
}


int main()
{
    UniquePtr<PPClient>     pSvr(new PPClient);
    pSvr->MainLoop();

    return 0;
}
