
#ifndef BERT_STREAMSOCKET_H
#define BERT_STREAMSOCKET_H

#include "./Threads/Atomic.h"
#include "Buffer.h"
#include "Socket.h"

#if defined(__gnu_linux__)
    #include <sys/types.h>
    #include <sys/socket.h>

#endif


// Abstraction for a TCP connection
class StreamSocket : public Socket
{
#if   !defined(__gnu_linux__)
protected:
    friend class        NetThread;
    OverlappedStruct    m_ovSend;
    OverlappedStruct    m_ovRecv;

public:
    bool    BeginRecv();
    void    EndRecv();
    void    EndSend();

#endif

public:
    // Constructor
    StreamSocket();
    
    // Destructor
    ~StreamSocket();

    bool       Init(SOCKET localfd, int localport, int peerport);
    SOCKTYPE   GetSocketType() const { return STREAMSOCKET; }
    typedef    int32_t  LENGTH_T;

public:
    // Receive data
    int    Recv();
public:
    // Send data
    bool   SendPacket(const char* pData, int nBytes);
    bool   SendPacket(Buffer&  bf);
    bool   SendPacket(StackBuffer&  sbf);
    bool   SendPacket(AttachedBuffer& abf);

    bool   OnReadable();
    bool   OnWritable();
    bool   Send();

    bool  DoMsgParse(); // false if no msg

#if !defined(__gnu_linux__)
    bool  IsRecvPending();
    bool  IsSendPending();
    bool  IsIOPending()     { return IsRecvPending() || IsSendPending();  }
#endif

private:
    int    _Send(const BufferSequence& buffers);
    bool   _SendFully(BufferSequence& buffers);
    bool   _FinalSync();
    virtual bool _HandlePacket(AttachedBuffer& buf) { return false; }
    LENGTH_T  m_currentPacketLen;

#if defined(__gnu_linux__)
    bool   m_bShouldEpollOut;
#endif

protected:
    unsigned short  m_localPort;
    unsigned short  m_peerPort;

    // For human readability
    enum
    {
        TIMEOUTSOCKET =  0,
        ERRORSOCKET   = -1,
        EOFSOCKET     = -2,
    };
    
    Buffer   m_recvBuf;
    Buffer   m_sendBuf;
};


#endif
