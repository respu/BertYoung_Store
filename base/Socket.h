
#ifndef BERT_SOCKET_H
#define BERT_SOCKET_H

#if defined(__gnu_linux__)
    #include <arpa/inet.h>

#else
    #include <WS2tcpip.h>    // socklen_t
    #include <MswSock.h>

#endif

#include <string>
#include "CommonDef.h"
#include "./SmartPtr/EnableShareMe.h"
#include "./EntryManager.h"


class NetThread;
class SocketAddr;

// Abstraction for a TCP socket
class Socket : public Entry, public EnableShareMe<Socket>
{
    friend class NetThread;
    friend class ListenThread;
protected:
    // Constructor
    Socket();

public:
    // Destructor
    virtual ~Socket();

    enum SOCKTYPE
    {
        INVALIDSOCKET = -1,
        LISTENSOCKET,
        CLIENTSOCKET,
        STREAMSOCKET,
    };

    virtual SOCKTYPE GetSocketType() const { return INVALIDSOCKET; }

protected:
    // The local socket
    SOCKET    m_localSock;

#if defined(__gnu_linux__)
    NetThread*      m_pNetThread;
#endif
    volatile bool   m_invalid;
    
public:
    bool Invalid() const { return m_invalid || INVALID_SOCKET == m_localSock; }
#if defined(__gnu_linux__)
    void SetNetThread(NetThread* p) { m_pNetThread = p; }
#endif

    // 连接的唯一id
    unsigned int GetID() const         {    return Entry::id; }

    virtual bool OnReadable() { return false; }
    virtual bool OnWritable() { return false; }
    virtual void OnError();

    static SOCKET CreateSocket();
    static void CloseSocket(SOCKET &sock);
    static void SetNonBlock(SOCKET sock, bool nonBlock = true);
    static void SetNodelay(SOCKET sock);
    static void SetSendBuffer(SOCKET sock, socklen_t size = 128 * 1024);
    static void SetRecvBuffer(SOCKET sock, socklen_t size = 128 * 1024);
    static void SetReuseAddr(SOCKET sock);
    static bool GetLocalAddr(SOCKET sock, SocketAddr& );
    static bool GetPeerAddr(SOCKET sock,  SocketAddr& );

#if !defined(__gnu_linux__)
    static LPFN_CONNECTEX  GetConnect(SOCKET sock);
    static bool UpdateContext(SOCKET sock);
#endif
    
private:
#if !defined(__gnu_linux__)
    static LPFN_CONNECTEX       m_connectEx;
#endif

    static IdCreator32<Socket>  sm_idCreator;

    // Avoid copy
    NONCOPYABLE(Socket);
};


class SocketAddr
{
public:
    SocketAddr()
    {
        m_addr.sin_family = 0;
        m_addr.sin_addr.s_addr = 0;
        m_addr.sin_port   = 0;
    }

    SocketAddr(const sockaddr_in& addr)
    {
        Init(addr);
    }

    SocketAddr(uint32_t  netip, uint16_t netport)
    {
        Init(netip, netport);
    }

    SocketAddr(const char* ip, uint16_t hostport)
    {
        Init(ip, hostport);
    }

    void Init(const sockaddr_in& addr)
    {
        memcpy(&m_addr, &addr, sizeof(addr));
    }

    void Init(uint32_t  netip, uint16_t netport)
    {
        m_addr.sin_family = AF_INET;
        m_addr.sin_addr.s_addr = netip;
        m_addr.sin_port   = netport;
    }

    void Init(const char* ip, uint16_t hostport)
    {
        m_addr.sin_family = AF_INET;
        m_addr.sin_addr.s_addr = ::inet_addr(ip);
        m_addr.sin_port   = htons(hostport);
    }

    const sockaddr_in& GetAddr() const
    {
        return m_addr;
    }

    const char* GetIP() const
    {
        return ::inet_ntoa(m_addr.sin_addr);
    }

    unsigned short GetPort() const
    {
        return  ntohs(m_addr.sin_port);
    
    }

    bool  Empty() const { return  0 == m_addr.sin_family; }

private:
    sockaddr_in  m_addr;
};

#endif
