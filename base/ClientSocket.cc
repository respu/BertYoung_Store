
#if defined(__gnu_linux__)
    #include <errno.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/epoll.h>
#endif

#include <cstdlib>
#include <cstring>
#include <cassert>
#include "Server.h"
#include "ClientSocket.h"
#include "Log/Logger.h"
#include "NetThreadPool.h"

#if !defined(__gnu_linux__)
ClientSocket::ClientSocket()
{
    m_ovConnect.m_event = IO_CONNECT;
    m_ovConnect.m_pSocket = this;
}
#endif

ClientSocket::~ClientSocket()
{
    LOCK_SDK_LOG;
    if (m_localSock == INVALID_SOCKET)
        DEBUG_SDK << "close invalid client socket key" << this;
    else
        DEBUG_SDK << __FUNCTION__ << " close Client socket " <<  m_localSock;
    UNLOCK_SDK_LOG;
}

bool ClientSocket::Connect(const char* ip, int port)
{
    if (!ip || port < 0)
        return false;

    if (INVALID_SOCKET != m_localSock)
        return false;

    m_peerAddr.Init(ip, port);

    m_localSock = CreateSocket();
    SetNonBlock(m_localSock);
    SetNodelay(m_localSock);
    SetRecvBuffer(m_localSock);
    SetSendBuffer(m_localSock);

#if !defined(__gnu_linux__)
    if (!NetThreadPool::Instance().AddSocket(ShareMe(), IO_CONNECT_REQ))
    {
        LOCK_SDK_LOG
        DEBUG_SDK << "ERROR when AddSocket client socket " << m_localSock 
                  << ", connected to " << ip << ":" << m_peerAddr.GetPort();
        UNLOCK_SDK_LOG
        return false;
    }
    
#else
    int  result = ::connect(m_localSock, (sockaddr*)&m_peerAddr.GetAddr(), sizeof(sockaddr_in));

    if (0 == result)
    {
        LOCK_SDK_LOG
        DEBUG_SDK << "CLIENT socket " << m_localSock << ", immediately connected to port " << m_peerAddr.GetPort();
        UNLOCK_SDK_LOG
       
        Server::Instance()->NewConnection(m_localSock);
        m_localSock = INVALID_SOCKET;
        return  true;
    }
    else
    {
        if (EINPROGRESS == errno)
        {
            LOCK_SDK_LOG
            DEBUG_SDK << "EINPROGRESS : client socket " << m_localSock <<", connected to " << ip << ":" << m_peerAddr.GetPort();
            UNLOCK_SDK_LOG
            
            if (!NetThreadPool::Instance().AddSocket(ShareMe(), IO_CONNECT_REQ))
            {
                LOCK_SDK_LOG
                DEBUG_SDK << "ERROR AddSocket when client socket " << m_localSock
                          << ", connected to " << ip
                          << " : " << m_peerAddr.GetPort();
                UNLOCK_SDK_LOG

                return false;
            }
         
            return true;
        }

        LOCK_SDK_LOG
        DEBUG_SDK << "Error client socket " << m_localSock << ", connected to " << m_peerAddr.GetPort();
        UNLOCK_SDK_LOG
        
        return false;
    }

#endif

    
    return true;
}

bool ClientSocket::OnWritable()
{
    int         error  = 0;
    socklen_t   len    = sizeof error;

    bool bSucc = (::getsockopt(m_localSock, SOL_SOCKET, SO_ERROR, (char*)&error, &len) >= 0 && 0 == error);
    if (!bSucc)    
    {
        errno = error;
        LOCK_SDK_LOG
        DEBUG_SDK << "FAILED client socket " << m_localSock 
                  << ", connect to " << m_peerAddr.GetPort()
                  << ", error " << error;
        UNLOCK_SDK_LOG
        return false;
    }

    LOCK_SDK_LOG
    DEBUG_SDK << "Create client socket " << m_localSock << ", connect to " << m_peerAddr.GetPort();
    UNLOCK_SDK_LOG

#if defined(__gnu_linux__)
    SharedPtr<ClientSocket>  pSocket(ShareMe());
    m_pNetThread->RemoveSocket(pSocket);
#else  
    if (!Socket::UpdateContext(m_localSock))
        return  false;
#endif

    Server::Instance()->NewConnection(m_localSock);
    m_localSock = INVALID_SOCKET; 

    return true;
}

void ClientSocket::OnError()
{
    Socket::OnError();
    Server::Instance()->Terminate();
}


#if !defined(__gnu_linux__)
bool    ClientSocket::PostConnect()
{
    // bind dummy local address
    SocketAddr    local(htonl(INADDR_ANY), 0);
    const sockaddr_in& localAddr = local.GetAddr();
    int  result = ::bind(m_localSock, (sockaddr *)(&localAddr), sizeof localAddr); 
    if (SOCKET_ERROR == result)
    {
        LOCK_SDK_LOG
        DEBUG_SDK << "Can not bind local addr, error = " << ::WSAGetLastError();
        UNLOCK_SDK_LOG
        return  false;
    }

    // get connect pointer
    LPFN_CONNECTEX ConnectEx = Socket::GetConnect(m_localSock);
    if (NULL == ConnectEx)
    {
        LOCK_SDK_LOG
        DEBUG_SDK << "Can not get connectEx, error = " << ::WSAGetLastError();
        UNLOCK_SDK_LOG
        return  false;
    }

    const sockaddr_in&  peerAddr = m_peerAddr.GetAddr();
    BOOL  bSucc = ConnectEx(m_localSock,
                            (const struct sockaddr* )(&peerAddr),
                            sizeof peerAddr,
                            NULL,
                            0,
                            NULL,
                            (LPOVERLAPPED)&m_ovConnect);

    if (bSucc == FALSE &&
        ::WSAGetLastError() != ERROR_IO_PENDING)
    {
        return false;
    }
    
    return  true;
}
#endif
