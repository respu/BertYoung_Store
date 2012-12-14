
#if defined(__gnu_linux__)
    #include <errno.h>
    #include <sys/socket.h>
    #include <sys/epoll.h>
#endif

#include <cstdlib>
#include <cstring>
#include <cassert>
#include "Server.h"
#include "NetThreadPool.h"
#include "ListenSocket.h"
#include "Log/Logger.h"
#include "SmartPtr/SharePtr.h"

const int ListenSocket::LISTENQ = 1024;

ListenSocket::ListenSocket() 
{
    m_localPort  =  INVALID_PORT;
}

ListenSocket::~ListenSocket()
{
    LOCK_SDK_LOG
    DEBUG_SDK << __FUNCTION__ << " = close LISTEN socket " << m_localSock;
    UNLOCK_SDK_LOG
}

bool ListenSocket::Bind(int port)
{
    if (port < 0)
        return  false;

    if (m_localSock != INVALID_SOCKET)
        return false;

    m_localPort = port;

    m_localSock = CreateSocket();
    SetNonBlock(m_localSock);
    SetNodelay(m_localSock);
    SetReuseAddr(m_localSock);
    SetRecvBuffer(m_localSock);
    SetSendBuffer(m_localSock);

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof serv);
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = htonl(INADDR_ANY);
    serv.sin_port = htons(port);

    int ret = ::bind(m_localSock, (struct sockaddr*)&serv, sizeof serv);
    if (SOCKET_ERROR == ret)
    {
        CloseSocket(m_localSock);
        LOCK_SDK_LOG
        DEBUG_SDK << "cannot bind port";
        UNLOCK_SDK_LOG
        return false;
    }
    ret = ::listen(m_localSock, ListenSocket::LISTENQ);
    if (SOCKET_ERROR == ret)
    {
        CloseSocket(m_localSock);
        LOCK_SDK_LOG
        DEBUG_SDK << "cannot listen on port " << port;
        UNLOCK_SDK_LOG
        return false;
    }
#if defined(__gnu_linux__)
    if (!NetThreadPool::Instance().AddSocket(ShareMe(), IO_ACCEPT_REQ))
    {
        return false;
    }
#endif
    LOCK_SDK_LOG
    DEBUG_SDK << "CREATE LISTEN socket " << m_localSock << " on port " <<  m_localPort;
    UNLOCK_SDK_LOG
    return true;
}

SOCKET ListenSocket::_Accept()
{
    socklen_t   addrLength = sizeof m_addrClient;
    return ::accept(m_localSock, (struct sockaddr *)&m_addrClient, &addrLength);
}

bool ListenSocket::OnReadable()
{
    int connfd = _Accept();
    if (connfd >= 0)
    {
        Server::Instance()->NewConnection(connfd);
    }
    else
    {
        bool result = false;
#if defined(__gnu_linux__)
        switch (errno)
        {
        case EWOULDBLOCK:
        case ECONNABORTED:
        case EINTR:
            result = true;
            break;

        case EMFILE:
        case ENFILE:
            LOCK_SDK_LOG
            DEBUG_SDK << "Not enough file descriptor available!!!";
            UNLOCK_SDK_LOG
            result = true;
            // TODO FIXME
            break;

        case ENOBUFS:
            LOCK_SDK_LOG
            DEBUG_SDK << "Not enough memory";          
            UNLOCK_SDK_LOG
            result = true;

        default:
            break;
        }
#else
        DWORD why = WSAGetLastError();

        switch (why)
        {
        case WSAEWOULDBLOCK:
        case WSAECONNRESET:
            result = true;
            break;

        case WSAEMFILE:
            LOCK_SDK_LOG
            DEBUG_SDK << "Not enough socket handle available!!!";
            UNLOCK_SDK_LOG
            result = true;
            break;

        case WSAENOBUFS:
            LOCK_SDK_LOG
            DEBUG_SDK << "Not enough memory";
            UNLOCK_SDK_LOG
            result = true;

        default:
            break;
        }
#endif
        return result;
    }
    
    return true;

}

bool ListenSocket::OnWritable()
{
    return false;
}

void ListenSocket::OnError()
{
    Server::Instance()->Terminate();
}


#if !defined(__gnu_linux__)

bool ListenThread::AddSocket(SharedPtr<ListenSocket> pSock)
{
    LOCK_SDK_LOG
    DEBUG_SDK << "Add socket " << pSock->m_localSock;
    UNLOCK_SDK_LOG

    assert (pSock->m_localSock != INVALID_SOCKET);

    bool bSucc = m_sockets.insert(SOCKETS::value_type(pSock->m_localSock, pSock)).second;

    return bSucc;
}


void ListenThread::Run()
{
    if (m_sockets.empty())
        return;

    fd_set   listenSockets;
    FD_ZERO(&listenSockets);

    SOCKETS::const_iterator    iter(m_sockets.begin());
    for (int cnt = 0;
         iter != m_sockets.end() && cnt < FD_SETSIZE;
         ++ iter, ++ cnt)
    {
        FD_SET(iter->first, &listenSockets);
    }

    if (iter != m_sockets.end())
    {
        LOCK_SDK_LOG
        DEBUG_SDK << "WARNING! Too many listen sockets " << m_sockets.size();
        UNLOCK_SDK_LOG
    }

    struct timeval  timeout;  
    timeout.tv_sec  = 1;  //s  
    timeout.tv_usec = 0;

    while (IsAlive())
    {
        fd_set  readFd = listenSockets;
        assert (readFd.fd_count > 0);

        int nReady = ::select(0, &readFd, NULL, NULL, &timeout);

        if (0 == nReady)
        {
            continue;
        }

        if (SOCKET_ERROR == nReady )
        {
            if (::WSAGetLastError() == WSAENOTSOCK)
            {
                Server::Instance()->Terminate();
            }

            continue;
        }

        for (unsigned int i = 0; i < readFd.fd_count; ++ i)
        {
            const int fd    =   readFd.fd_array[i];
            SharedPtr<ListenSocket>   pSock = m_sockets[fd];

            assert(pSock.Get() && "Error, NULL socket");
  
            if (!pSock->OnReadable())       
            {
                LOCK_SDK_LOG
                DEBUG_SDK << "Accept error!!!";
                UNLOCK_SDK_LOG
                
                pSock->OnError();
            }
        }
    }
} 

#endif
