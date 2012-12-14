
#ifndef BERT_LISTENSOCKET_H
#define BERT_LISTENSOCKET_H

#include "Socket.h"

class ListenSocket : public Socket
{
    static const int LISTENQ;
public:
    ListenSocket();
    ~ListenSocket();
    
    bool Bind(int port);
    
protected:
    SOCKET _Accept();

public:
    SOCKTYPE GetSocketType() const { return LISTENSOCKET; }

    bool OnReadable();
    bool OnWritable();
    void OnError();

private:
    sockaddr_in     m_addrClient;
    unsigned short  m_localPort;
};



#if !defined(__gnu_linux__)
#include <map>
#include "./Threads/Thread.h"

class ListenThread : public Runnable
{
public:
    bool            AddSocket(SharedPtr<ListenSocket> pSock);
    virtual void    Run();

private:
    typedef std::map<SOCKET, SharedPtr<ListenSocket> > SOCKETS;
    SOCKETS         m_sockets;
};

#endif

#endif
