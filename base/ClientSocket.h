
#ifndef BERT_CLIENTSOCKET_H
#define BERT_CLIENTSOCKET_H

#include "Socket.h"

// Abstraction for a TCP client socket
class ClientSocket : public Socket
{
public:
#if !defined(__gnu_linux__)

    ClientSocket();

    bool    PostConnect();
#endif

    virtual ~ClientSocket();
    bool    Connect(const char* ip, int port = INVALID_PORT);
    bool    OnWritable();
    void    OnError();
    SOCKTYPE GetSocketType() const { return CLIENTSOCKET; }

private:
    SocketAddr       m_peerAddr;
#if !defined(__gnu_linux__)
    OverlappedStruct m_ovConnect;
#endif
};

#endif
