
#include <cstring>
#include <cassert>
#include "Socket.h"
#include "Log/Logger.h"

#if defined(__gnu_linux__)
    #include <errno.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/socket.h>
    #include <netinet/tcp.h>
#endif

#if !defined(__gnu_linux__)
LPFN_CONNECTEX       Socket::m_connectEx = NULL;

#endif

IdCreator32<Socket>  Socket::sm_idCreator;

Socket::Socket() : m_localSock(INVALID_SOCKET)
{
    Entry::id   = sm_idCreator.CreateID();
#if defined(__gnu_linux__)
    m_pNetThread= NULL;
#endif
    m_invalid   = false;
}

Socket::~Socket()
{
    CloseSocket(m_localSock);
}


void Socket::OnError()
{
    LOCK_SDK_LOG
    DEBUG_SDK << "Socket is set invalid " << m_localSock;
    UNLOCK_SDK_LOG
    m_invalid = true;
}

SOCKET Socket::CreateSocket()
{
    SOCKET sock;
#if defined(__gnu_linux__)
    sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
    sock = ::WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
#endif
    return sock;
}

void Socket::CloseSocket(SOCKET& sock)
{
    if (sock != INVALID_SOCKET)
    {
#if defined(__gnu_linux__)
        TEMP_FAILURE_RETRY(::shutdown(sock, SHUT_RDWR));
        TEMP_FAILURE_RETRY(::close(sock));

#else
        ::shutdown(sock, SD_BOTH);
        ::closesocket(sock);

#endif
        LOCK_SDK_LOG
        DEBUG_SDK << "Close socket " << sock;
        UNLOCK_SDK_LOG
        sock = INVALID_SOCKET;
    }
}

void Socket::SetNonBlock(SOCKET sock, bool nonblock)
{
    int flag = -1;
#if defined(__gnu_linux__)
    flag = ::fcntl(sock, F_GETFL, 0); 
    assert(flag >= 0 && "Non Block failed");

    if (nonblock)
        flag = ::fcntl(sock, F_SETFL, flag | O_NONBLOCK);
    else
        flag = ::fcntl(sock, F_SETFL, flag & ~O_NONBLOCK);
    
#else
    unsigned long nonBlock = (nonblock ? 1 : 0);
    flag = ::ioctlsocket(sock, FIONBIO, &nonBlock);

#endif
    assert(flag >= 0 && "Non Block failed");
}

void Socket::SetNodelay(SOCKET sock)
{
    int nodelay = 1;
    ::setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(int));
}

void Socket::SetSendBuffer(SOCKET sock, socklen_t winsize)
{
    ::setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char*)&winsize, sizeof(socklen_t));
}

void Socket::SetRecvBuffer(SOCKET sock, socklen_t winsize)
{
    ::setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)&winsize, sizeof(socklen_t));
}

void Socket::SetReuseAddr(SOCKET sock)
{
    int reuse = 1;
    ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(int));
}

bool Socket::GetLocalAddr(SOCKET  sock, SocketAddr& addr)
{
    sockaddr_in localAddr;
    socklen_t   len = sizeof localAddr;

    if (0 == ::getsockname(sock, (struct sockaddr*)&localAddr, &len))
    {
        addr.Init(localAddr);
    }
    else
    {
        return  false;
    }

    return  true;
}

bool Socket::GetPeerAddr(SOCKET  sock, SocketAddr& addr)
{
    sockaddr_in  peerAddr;
    socklen_t    len = sizeof peerAddr;
    if (0 == ::getpeername(sock, (struct sockaddr*)&peerAddr, &len))
    {
        addr.Init(peerAddr);
    }
    else
    {
        return  false;
    }

    return  true;
}

#if !defined(__gnu_linux__)
LPFN_CONNECTEX  Socket::GetConnect(SOCKET sock)
{
    if (NULL == m_connectEx)
    { 
        unsigned long   bytes   = 0;       
        GUID            guid    = WSAID_CONNECTEX;

        if (0 != ::WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
            &guid, sizeof(guid), &m_connectEx, sizeof m_connectEx,
            &bytes, 0, 0))
        {
            return m_connectEx = NULL;
        }
    }

    return  m_connectEx;
}

bool    Socket::UpdateContext(SOCKET sock)
{
    return 0 == ::setsockopt(sock,
        SOL_SOCKET, 
        SO_UPDATE_CONNECT_CONTEXT, 
        NULL, 
        0 );
}

#endif
