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

    if (!PPServer::_Bind(8888))
    {
        ERR(m_pLog) << "can not bind socket";
        return false;
    }

    return true;
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
