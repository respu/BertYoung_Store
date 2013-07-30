#ifndef BERT_COMMONDEF_H
#define BERT_COMMONDEF_H

#undef  INVALID_PORT
#define INVALID_PORT (-1)

#undef  NONCOPYABLE
#define NONCOPYABLE(TypeName)       \
    TypeName(const TypeName&);      \
    void operator=(const TypeName&)

enum IORequestType
{
    IO_READ_REQ     = 0x1 << 0,
    IO_WRITE_REQ    = 0x1 << 1,
    IO_CONNECT_REQ  = 0x1 << 2,
    IO_ACCEPT_REQ   = 0x1 << 3,
    IO_ALL_REQ      = 0x0000000F,
};

#if defined(__gnu_linux__)
    #include <stdint.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    
    typedef int HANDLE;
    #define INVALID_HANDLE_VALUE   (-1)

    typedef int SOCKET;
    #define INVALID_SOCKET  (SOCKET)(~0)
    #define SOCKET_ERROR    (-1)
#else
    #include <Windows.h>
    #include <Winsock2.h>
    typedef  __int8   int8_t;
    typedef  __int16  int16_t;
    typedef  __int32  int32_t;
    typedef  __int64  int64_t;

    typedef  unsigned __int8   uint8_t;
    typedef  unsigned __int16  uint16_t;
    typedef  unsigned __int32  uint32_t;
    typedef  unsigned __int64  uint64_t;

    #define    __thread    __declspec(thread)
    #define    snprintf    _snprintf


    enum IOEventType
    {
        IO_RECV = 0,
        IO_RECV_ZERO, // 投递0字节recv，如果接收缓冲区满了
        IO_SEND,
        IO_CONNECT,
        NUM_IO_EVENTS
    };

    class  Socket;
    struct OverlappedStruct
    {
    public:
        OverlappedStruct();
        void    Reset();
        bool    Lock();
        void    Unlock();
        bool    IsPending();

        enum
        {
            OV_LOCKED   = 1,
            OV_UNLOCKED = 0,
        };

        OVERLAPPED  m_overlap;
        IOEventType m_event;
        long        m_lock;
        int         m_bytes;
        Socket*     m_pSocket;
    };

#endif

template <typename T>
inline T Host2Net(T  data)
{
    return data;
}

template <>
inline short Host2Net(short data)
{
    return htons(data);
}

template <>
inline unsigned short Host2Net(unsigned short data)
{
    return htons(data);
}

template <>
inline int Host2Net(int data)
{
    return htonl(data);
}

template <>
inline unsigned int Host2Net(unsigned int data)
{
    return htonl(data);
}


template <>
inline long long Host2Net(long long  data)
{
    return (static_cast<long long>(htonl(data & 0xFFFFFFFF)) << 32) | htonl(data >> 32);
}

template <>
inline unsigned long long Host2Net(unsigned long long data)
{
    return (static_cast<unsigned long long>(htonl(data & 0xFFFFFFFF)) << 32) | htonl(data >> 32);
}



template <typename T>
inline T  Net2Host(T data)
{
    return data;
}


template <>
inline short  Net2Host(short data)
{
    return ntohs(data);
}


template <>
inline unsigned short  Net2Host(unsigned short  data)
{
    return ntohs(data);
}


template <>
inline int Net2Host(int data)
{
    return ntohl(data);
}


template <>
inline unsigned int Net2Host(unsigned int data)
{
    return  ntohl(data);
}


template <>
inline long long Net2Host(long long data)
{
    return (static_cast<long long>(ntohl(data & 0xFFFFFFFF)) << 32) | ntohl(data >> 32);
}


template <>
inline unsigned long long Net2Host(unsigned long long data)
{
    return (static_cast<unsigned long long>(ntohl(data & 0xFFFFFFFF)) << 32) | ntohl(data >> 32);
}

inline int GetNumOfCPU()
{
#if defined(__gnu_linux__)
    return  sysconf(_SC_NPROCESSORS_ONLN);
#else
    SYSTEM_INFO    si;
    GetSystemInfo(&si);
    return  si.dwNumberOfProcessors;
#endif
}

#endif

