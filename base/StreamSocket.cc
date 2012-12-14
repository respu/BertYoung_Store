#include <cstdlib>
#include <cstring>

#if defined(__gnu_linux__)
    #include <errno.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <netinet/tcp.h>
    #include <sys/epoll.h>
#else
    #include "./Threads/Atomic.h"
#endif

#include "NetThreadPool.h"
#include "StreamSocket.h"
#include "Log/Logger.h"

#if !defined(__gnu_linux__)

OverlappedStruct::OverlappedStruct()
{
    AtomicSet(&m_lock, OV_UNLOCKED);
    Reset();
    m_pSocket = NULL;
}

void OverlappedStruct::Reset()
{
    memset(&m_overlap, 0, sizeof(OVERLAPPED));
    m_bytes = 0;
}

bool OverlappedStruct::Lock()
{
    long    prevState = AtomicTestAndSet(&m_lock, OV_UNLOCKED, OV_LOCKED);
    return  prevState == OV_UNLOCKED;
}

void OverlappedStruct::Unlock()
{
    AtomicSet(&m_lock, OV_UNLOCKED);
}

bool OverlappedStruct::IsPending()
{
    return AtomicGet(&m_lock) == OV_LOCKED;
}

bool StreamSocket::BeginRecv()
{
    return m_ovRecv.Lock();
}

void StreamSocket::EndRecv()
{
    m_ovRecv.Unlock();
    LOCK_SDK_LOG
    DEBUG_SDK << "EndRecv : Worker thread [" << (Thread::GetCurrentThreadId()&0xffff) << "], socket = " << m_localSock;
    UNLOCK_SDK_LOG
}

void StreamSocket::EndSend()
{
    m_ovSend.Unlock();
    LOCK_SDK_LOG
    DEBUG_SDK << "EndSend : Worker thread [" << (Thread::GetCurrentThreadId()&0xffff) << "], socket = " << m_localSock;
    UNLOCK_SDK_LOG
}

bool StreamSocket::IsRecvPending()
{
    if (m_ovRecv.IsPending())
    {
        if (!HasOverlappedIoCompleted(&m_ovRecv.m_overlap))
        {
            LOCK_SDK_LOG
            DEBUG_SDK << "There is still  recv for socket " << m_localSock;
            UNLOCK_SDK_LOG
        }
        else
        {
            LOCK_SDK_LOG
            DEBUG_SDK << "Something abnormal, there is no recv for socket " << m_localSock
                      << ", event type " << m_ovRecv.m_event;
            UNLOCK_SDK_LOG
        }

        return true;
    }
    else
    { 
        LOCK_SDK_LOG
        DEBUG_SDK << "No pending recv for socket " << m_localSock;
        UNLOCK_SDK_LOG

        return false;
    }
}

bool StreamSocket::IsSendPending()
{
    if (m_ovSend.IsPending())
    {
        if (!HasOverlappedIoCompleted(&m_ovSend.m_overlap))
        {
            LOCK_SDK_LOG
            DEBUG_SDK << "There is still pending send for socket " << m_localSock;
            UNLOCK_SDK_LOG
        }
        else
        {
            LOCK_SDK_LOG
            DEBUG_SDK << "Something abnormal, there is no send for socket " << m_localSock;
            UNLOCK_SDK_LOG
        }

        return true;
    }
    else
    {
        LOCK_SDK_LOG
        DEBUG_SDK << "No pending send for socket " << m_localSock;
        UNLOCK_SDK_LOG

        return false;
    }
}
#endif

StreamSocket::StreamSocket()
#if defined(__gnu_linux__)
: m_bShouldEpollOut(false)
#endif
{
    m_localPort = m_peerPort = INVALID_PORT;
#if !defined(__gnu_linux__)
    m_ovSend.m_event = IO_SEND;
    m_ovSend.m_pSocket = this;
    m_ovRecv.m_event = IO_RECV;
    m_ovRecv.m_pSocket = this;
#endif
    m_currentPacketLen = 0;
}

StreamSocket::~StreamSocket()
{
    LOCK_SDK_LOG
    DEBUG_SDK << "Try close tcp connection " << (m_localSock != INVALID_SOCKET ? m_localSock : 0);
    UNLOCK_SDK_LOG

    if (!_FinalSync())
    {
        LOCK_SDK_LOG
        DEBUG_SDK << "Error finalsync";
        UNLOCK_SDK_LOG
    }
}

bool StreamSocket::Init(SOCKET localfd, int localport, int peerport)
{
    if (INVALID_SOCKET == localfd)
        return false;
    m_localSock = localfd;
    m_localPort = localport;
    m_peerPort  = peerport;
    SetNonBlock(m_localSock);
    LOCK_SDK_LOG
    DEBUG_SDK << "New task, fd = " << m_localSock << ", localport = " << m_localPort << ", peerport = " << peerport;
    UNLOCK_SDK_LOG
    return m_localSock >= 0;
}

int StreamSocket::Recv()
{
    int ret = 0;

#if defined(__gnu_linux__)
    if (0 == m_recvBuf.SizeForWrite())
    {
        return 0;
    }
    
    BufferSequence buffers;
    m_recvBuf.GetSpace(buffers);
    assert(buffers.count != 0);
    ret = ::readv(m_localSock, buffers.buffers, buffers.count);
    if (ret == ERRORSOCKET && (EAGAIN == errno || EWOULDBLOCK == errno))
        return 0;

    if (ret > 0)
        m_recvBuf.AdjustWritePtr(ret);

    return (0 == ret) ? EOFSOCKET : ret;

#else
    assert(m_ovRecv.IsPending() && "You must lock m_ovRecv before post recv");
    DWORD  dummyBytes  = 0;
    DWORD  flags       = 0;

    BufferSequence    buffers;
    m_recvBuf.GetSpace(buffers);

    m_ovRecv.Reset();
    if (0 == buffers.count)
    {
        buffers.buffers[0].IOVEC_BUF = 0;
        buffers.buffers[0].IOVEC_LEN = 0;
        buffers.count = 1;
        LOCK_SDK_LOG
        DEBUG_SDK << "Error! recv buf is full! and send buf has space " << m_sendBuf.SizeForWrite()
                  << ", now PostRecv 0 byte";
        UNLOCK_SDK_LOG

        m_ovRecv.m_event = IO_RECV_ZERO;
    }
    else
    {
        m_ovRecv.m_event = IO_RECV;
    }
    
    ret = ::WSARecv(m_localSock, (LPWSABUF)buffers.buffers, buffers.count, &dummyBytes, &flags, (LPOVERLAPPED)&m_ovRecv, NULL);

    if (m_ovRecv.m_event == IO_RECV_ZERO)
    {
        LOCK_SDK_LOG
        DEBUG_SDK << "000zero m_overlap inner " << m_ovRecv.m_overlap.Internal << ", error " << WSAGetLastError() << ", ret = " << ret;
        UNLOCK_SDK_LOG
    }

    if (SOCKET_ERROR == ret)
    {
        if (WSAGetLastError() == WSA_IO_PENDING)
        {
            ret = 0;    
        }
        else
        {
            LOCK_SDK_LOG
            DEBUG_SDK << "Fatal error when post recv, error " << WSAGetLastError() << ", socket = " << m_localSock;
            UNLOCK_SDK_LOG
        }
    }

    if (SOCKET_ERROR != ret)
    {
        LOCK_SDK_LOG
        DEBUG_SDK << "PostRecv : Worker thread [" << (Thread::GetCurrentThreadId()&0xffff) << "], socket = " << m_localSock;
        UNLOCK_SDK_LOG
    }

    return ret;
#endif
}


int StreamSocket::_Send(const BufferSequence& buffers)
{
    assert (0 != buffers.count);

    int    ret = 0;
#if defined(__gnu_linux__)
    int totalBytes = buffers.TotalBytes();

    ret = ::writev(m_localSock, buffers.buffers, buffers.count);
    if (ERRORSOCKET == ret && (EAGAIN == errno || EWOULDBLOCK == errno))
    {
        m_bShouldEpollOut = true;
        ret = 0;
    }
    else if (ret > 0 && ret < totalBytes)
    {
        m_bShouldEpollOut = true;
    }
    else if (ret == totalBytes)
    {
        m_bShouldEpollOut = false;
    }
#else
    m_ovSend.Reset();

    DWORD dummyBytes = 0;
    ret = ::WSASend(m_localSock, (LPWSABUF)buffers.buffers, buffers.count, &dummyBytes, 0, (LPOVERLAPPED)&m_ovSend, NULL);
    if (SOCKET_ERROR == ret && WSAGetLastError() == WSA_IO_PENDING)
        ret = 0;

#endif

    return ret;
}

bool StreamSocket::_SendFully(BufferSequence& buffers)
{
    int  totalBytes = buffers.TotalBytes();
    int  offset     = 0;

#if defined(__gnu_linux__)
    while (offset < totalBytes)
    {
        int nBytes = _Send(buffers);

        if (ERRORSOCKET == nBytes)
            return false;

        offset += nBytes;
    }
    return offset == totalBytes;

#else
    int iBufferIndex = 0;
    int nSentBytes = 0;
    while (nSentBytes < totalBytes)
    {
        DWORD nBytes = 0;
        int ret = ::WSASend(m_localSock, (LPWSABUF)buffers.buffers + iBufferIndex, buffers.count,
            &nBytes, 0, NULL, NULL);

        if (SOCKET_ERROR == ret)
        {
            int error = WSAGetLastError();
           // if (WSAEWOULDBLOCK != error)
            {
                LOCK_SDK_LOG
                DEBUG_SDK << "Final sync,  failed to send, reason " << error;
                UNLOCK_SDK_LOG
                return false;
            }
        }

        if (0 == nBytes)
            continue;
        
        LOCK_SDK_LOG    
        DEBUG_SDK << "Final sync,  succ to send " << nBytes;
        UNLOCK_SDK_LOG

        nSentBytes += nBytes;
        offset     += nBytes;
        if (offset < static_cast<int>(buffers.buffers[iBufferIndex].IOVEC_LEN))
        {
            buffers.buffers[iBufferIndex].IOVEC_BUF += nBytes;
            buffers.buffers[iBufferIndex].IOVEC_LEN -= nBytes;
        }
        else
        {
            offset -= buffers.buffers[iBufferIndex].IOVEC_LEN;
            buffers.buffers[iBufferIndex].IOVEC_BUF = NULL;
            buffers.buffers[iBufferIndex].IOVEC_LEN = 0;
            if (-- buffers.count <= 0)
                return true;

            ++ iBufferIndex;
            buffers.buffers[iBufferIndex].IOVEC_BUF += offset;
            buffers.buffers[iBufferIndex].IOVEC_LEN -= offset;
        }
    }
    return nSentBytes == totalBytes;

#endif
}

bool StreamSocket::_FinalSync()
{
    if (INVALID_SOCKET == m_localSock)
        return true;
    
    BufferSequence buffers;
    m_sendBuf.GetDatum(buffers);
    if (0 == buffers.count)
        return true;
    
    bool ret = this->_SendFully(buffers);
    m_sendBuf.Clear();
    return ret;
}


bool StreamSocket::SendPacket(const char* pData, int nBytes)
{
    if (!pData || nBytes <= 0)
    {
        return true;
    }

    if (nBytes >= m_sendBuf.SizeForWrite())
    {
        return true;
    }

    LENGTH_T   nBodyLen = static_cast<LENGTH_T>(nBytes);
    if (m_sendBuf.PushDataAt(&nBodyLen, sizeof nBodyLen) &&
        m_sendBuf.PushDataAt(pData, nBodyLen, sizeof nBodyLen))
    {
        m_sendBuf.AdjustWritePtr(nBodyLen + sizeof nBodyLen);
        return true;
    }
    else
    {
        LOCK_SDK_LOG
        DEBUG_SDK << "Can not send packet because send buffer overflow, bytes = " << nBytes
                  << ", but send buf only has space " << m_sendBuf.SizeForWrite();
        UNLOCK_SDK_LOG
        return false;
    }
}


bool  StreamSocket::SendPacket(Buffer& bf)
{
    return  SendPacket(bf.ReadAddr(), bf.SizeForRead());
}


bool  StreamSocket::SendPacket(StackBuffer& sf)
{
    return  SendPacket(sf.ReadAddr(), sf.SizeForRead());
}


bool  StreamSocket::SendPacket(AttachedBuffer& af)
{
    return  SendPacket(af.ReadAddr(), af.SizeForRead());
}


bool  StreamSocket::OnReadable()
{
    int     nBytes = 0;
#if defined(__gnu_linux__)
    nBytes = StreamSocket::Recv();
    if (nBytes < 0)
    {
        LOCK_SDK_LOG
        DEBUG_SDK << "socket " << m_localSock <<", OnRead error , nBytes = " << nBytes;
        UNLOCK_SDK_LOG
        return false;
    }

#else
    nBytes = m_ovRecv.m_bytes;
    if (nBytes <= 0)
    {
        LOCK_SDK_LOG
        DEBUG_SDK << "socket " << m_localSock <<", OnRead error , nBytes = " << nBytes;
        UNLOCK_SDK_LOG
        return false;
    }
    else
    {
        m_recvBuf.AdjustWritePtr(nBytes);
    }

    LOCK_SDK_LOG
    DEBUG_SDK << "OnRecv : Worker thread [" << (Thread::GetCurrentThreadId() & 0xFFFF)
              << "], socket = " << m_localSock
              << ", bytes = " << nBytes;
    UNLOCK_SDK_LOG

    if (SOCKET_ERROR == StreamSocket::Recv())
        return false;

#endif

    return true;
}


// drive by EPOLLOUT
bool StreamSocket::OnWritable()
{
    int nBytes = 0;

#if defined(__gnu_linux__)
    BufferSequence   buffers;
    m_sendBuf.GetDatum(buffers);
    if (buffers.count > 0)
    {
        nBytes = this->_Send(buffers);
        if (nBytes > 0)
            m_sendBuf.AdjustReadPtr(nBytes);
    }
    else
    {
        m_bShouldEpollOut = false;
    }

    if (!m_bShouldEpollOut)
        m_pNetThread->ModSocket(ShareMe(), IO_READ_REQ);

#else
    nBytes = m_ovSend.m_bytes;
    if (nBytes > 0)
    {
        m_sendBuf.AdjustReadPtr(nBytes);
        LOCK_SDK_LOG
            DEBUG_SDK << "OnWritable : Worker thread [" << (Thread::GetCurrentThreadId() & 0xFFFF)
            << "], socket = " << m_localSock
            << ", send buf has remain bytes = " << m_sendBuf.SizeForRead();
        UNLOCK_SDK_LOG
    }

#endif

    return nBytes != ERRORSOCKET;
}


bool StreamSocket::Send()
{
    int nBytes = 0;
#if defined(__gnu_linux__)
    if (m_bShouldEpollOut)
        return true;

    BufferSequence buffers;
    m_sendBuf.GetDatum(buffers);
    if (0 == buffers.count)
        return true;

    nBytes = this->_Send(buffers);
    if (nBytes > 0)
        m_sendBuf.AdjustReadPtr(nBytes);

    if (m_bShouldEpollOut)
        m_pNetThread->ModSocket(ShareMe(), IORequestType(IO_READ_REQ | IO_WRITE_REQ));

#else
    if (m_ovSend.Lock())
    {
        BufferSequence buffers;
        m_sendBuf.GetDatum(buffers);    
        if (0 == buffers.TotalBytes())
        {
            m_ovSend.Unlock();
            return true;
        }

        nBytes = this->_Send(buffers);
        if (0 != nBytes)
        {
            LOCK_SDK_LOG
            DEBUG_SDK << "Error Post send " << WSAGetLastError();
            UNLOCK_SDK_LOG
            m_ovSend.Unlock();
            return false;
        }
        LOCK_SDK_LOG
        DEBUG_SDK << "PostSend : Worker thread [" << (Thread::GetCurrentThreadId()&0xffff)
                  << "], socket = " << m_localSock
                  << ", bytes = " << buffers.TotalBytes();
        UNLOCK_SDK_LOG
    }
    
#endif

    return nBytes != ERRORSOCKET;
}


bool StreamSocket::DoMsgParse()
{
    bool busy = false;
    while (m_recvBuf.SizeForRead() > static_cast<int>(sizeof m_currentPacketLen))
    {
        if (0 == m_currentPacketLen)
        {
            if (!m_recvBuf.PeekDataAt(&m_currentPacketLen, sizeof m_currentPacketLen))
            {
                return  busy;
            }
            m_recvBuf.AdjustReadPtr(sizeof m_currentPacketLen);
        }

        // some defensive code
        if (3 * m_currentPacketLen > 2 * m_recvBuf.Capacity())
        {
            LOCK_SDK_LOG
            DEBUG_SDK << "Too big packet " << m_currentPacketLen << " on socket " << m_localSock;
            UNLOCK_SDK_LOG
            OnError();
            return busy;
        }
        
        if (m_recvBuf.SizeForRead() < m_currentPacketLen)
        {
            return busy;
        }

        busy = true;

        BufferSequence   bf;
        m_recvBuf.GetDatum(bf, m_currentPacketLen); 

        assert (m_currentPacketLen == bf.TotalBytes());

        AttachedBuffer   cmd(bf);
        _HandlePacket(cmd);

        m_recvBuf.AdjustReadPtr(m_currentPacketLen);
        m_currentPacketLen = 0;
    }

    return  busy;
}
