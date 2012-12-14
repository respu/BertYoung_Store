
#ifndef BERT_BUFFER_H
#define BERT_BUFFER_H

#include <cassert>
#include <cstring>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <string>
#include "Threads/Atomic.h"

#if defined(__gnu_linux__)
    #include <sys/uio.h>
    #include <arpa/inet.h>
#else
    #include <Winsock2.h>
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


struct BufferSequence
{
#if defined(__gnu_linux__)
    iovec   buffers[2];
    #define IOVEC_BUF    iov_base
    #define IOVEC_LEN    iov_len

#else
    WSABUF  buffers[2];
    #define IOVEC_BUF    buf
    #define IOVEC_LEN    len

#endif

    int     count;

    int TotalBytes() const
    {
        int nBytes = 0;
        for (int i = 0; i < count; ++ i)
            nBytes += buffers[i].IOVEC_LEN;

        return nBytes;
    }
};

static const int  DEFAULT_BUFFER_SIZE = 256 * 1024;

inline int RoundUp2Power(int size)
{
    assert (size > 0 && "Size overflow or underflow");
    int    roundSize = 1;
    while (roundSize < size)
        roundSize <<= 1;

    return roundSize;
}


template <typename BUFFER>
class  CircularBuffer
{
public:
    // Constructor  to be specialized
    explicit CircularBuffer(int maxSize = DEFAULT_BUFFER_SIZE);
    CircularBuffer(const BufferSequence& bf);
    CircularBuffer(char* , int );
   ~CircularBuffer() { }

    bool IsEmpty() const { return m_writePos == m_readPos; }
    bool IsFull()  const { return ((m_writePos + 1) & (m_maxSize - 1)) == m_readPos; }

    // For gather write
    void GetDatum(BufferSequence& buffer, int maxSize = DEFAULT_BUFFER_SIZE - 1, int offset = 0);

    // For scatter read
    void GetSpace(BufferSequence& buffer, int offset = 0);

    // Put data into internal m_buffer
    bool PushData(const void* pData, int nSize);
    bool PushDataAt(const void* pData, int nSize, int offset = 0);

    // Get data from internal m_buffer
    bool PeekData(void* pBuf, int nSize);
    bool PeekDataAt(void* pBuf, int nSize, int offset = 0);

    char* ReadAddr()  { return &m_buffer[m_readPos];  }
    char* WriteAddr() { return &m_buffer[m_writePos]; }

    void AdjustWritePtr(int size);
    void AdjustReadPtr(int size);

    int SizeForRead()  const {  return (m_writePos - m_readPos) & (m_maxSize - 1);  }
    int SizeForWrite() const {  return m_maxSize - SizeForRead();  }

    void Clear() {  AtomicSet(&m_readPos, m_writePos);  }

    int Capacity() const { return m_maxSize; }

    template <typename T>
    CircularBuffer& operator<< (const T& data);
    template <typename T>
    CircularBuffer& operator>> (T& data);

    template <typename T>
    CircularBuffer& operator<< (T* const&ptr);
    template <typename T>
    CircularBuffer& operator>> (T* &ptr);

    template <typename T, int N>
    CircularBuffer& operator<< (T const (& array)[N]);
    template <typename T, int N>
    CircularBuffer& operator>> (T (& array)[N]);

    template <typename T>
    CircularBuffer & operator<< (const std::vector<T>& );
    template <typename T>
    CircularBuffer & operator>> (std::vector<T>& );

    template <typename T>
    CircularBuffer & operator<< (const std::list<T>& );
    template <typename T>
    CircularBuffer & operator>> (std::list<T>& );

    template <typename K>
    CircularBuffer & operator<< (const std::set<K>& );
    template <typename K>
    CircularBuffer & operator>> (std::set<K>& );

    template <typename K>
    CircularBuffer & operator<< (const std::multiset<K>& );
    template <typename K>
    CircularBuffer & operator>> (std::multiset<K>& );

    template <typename K, typename V>
    CircularBuffer & operator<< (const std::map<K,V>& m);
    template <typename K, typename V>
    CircularBuffer & operator>> (std::map<K,V>& m);

    template <typename K, typename V>
    CircularBuffer & operator<< (const std::multimap<K,V>& m);
    template <typename K, typename V>
    CircularBuffer & operator>> (std::multimap<K,V>& m);

    CircularBuffer & operator<< (const std::string& str);
    CircularBuffer & operator>> (std::string& str);

private:
    // The max capacity of m_buffer
    int m_maxSize;

    // The starting address can be read
    volatile int m_readPos;

    // The starting address can be write
    volatile int m_writePos;

    // The real internal buffer
    BUFFER m_buffer;

    bool   m_owned;
};

template <typename BUFFER>
void CircularBuffer<BUFFER>::GetDatum(BufferSequence& buffer, int maxSize, int offset)    
{
    if (offset < 0 ||
        maxSize <= 0 ||
        offset >= SizeForRead()
       )
    {
        buffer.count = 0;
        return;
    }

    assert(m_readPos  >= 0 && m_readPos  < m_maxSize);
    assert(m_writePos >= 0 && m_writePos < m_maxSize);

    int   bufferIndex  = 0;
    const int readPos  = (m_readPos + offset) & (m_maxSize - 1);
    const int writePos = m_writePos;
    assert (readPos != writePos);

    buffer.buffers[bufferIndex].IOVEC_BUF = &m_buffer[readPos];
    if (readPos < writePos)
    {            
        if (maxSize < writePos - readPos)
            buffer.buffers[bufferIndex].IOVEC_LEN = maxSize;
        else
            buffer.buffers[bufferIndex].IOVEC_LEN = writePos - readPos;
    }
    else
    {
        int nLeft = maxSize;
        if (nLeft > (m_maxSize - readPos))
            nLeft = (m_maxSize - readPos);
        buffer.buffers[bufferIndex].IOVEC_LEN = nLeft;
        nLeft = maxSize - nLeft;
 
        if (nLeft > 0 && writePos > 0)
        {
            if (nLeft > writePos)
                nLeft = writePos;

            ++ bufferIndex;
            buffer.buffers[bufferIndex].IOVEC_BUF = &m_buffer[0];
            buffer.buffers[bufferIndex].IOVEC_LEN = nLeft;
        }
    }

    buffer.count = bufferIndex + 1;
}

template <typename BUFFER>
void CircularBuffer<BUFFER>::GetSpace(BufferSequence& buffer, int offset)
{
    assert(m_readPos >= 0 && m_readPos < m_maxSize);
    assert(m_writePos >= 0 && m_writePos < m_maxSize);

    if (SizeForWrite() <= offset + 1)
    {
        buffer.count = 0;
        return;
    }

    int bufferIndex = 0;
    const int readPos  = m_readPos;
    const int writePos = (m_writePos + offset) & (m_maxSize - 1);

    buffer.buffers[bufferIndex].IOVEC_BUF = &m_buffer[writePos];

    if (readPos > writePos)
    {
        buffer.buffers[bufferIndex].IOVEC_LEN = readPos - writePos - 1;
        assert (buffer.buffers[bufferIndex].IOVEC_LEN > 0);
    }
    else
    {
        buffer.buffers[bufferIndex].IOVEC_LEN = m_maxSize - writePos;
        if (0 == readPos)
        {
            buffer.buffers[bufferIndex].IOVEC_LEN -= 1;
        }
        else if (readPos > 1)
        {
            ++ bufferIndex;
            buffer.buffers[bufferIndex].IOVEC_BUF = &m_buffer[0];
            buffer.buffers[bufferIndex].IOVEC_LEN = readPos - 1;
        }
    }

    buffer.count = bufferIndex + 1;
}

template <typename BUFFER>
bool CircularBuffer<BUFFER>::PushDataAt(const void* pData, int nSize, int offset)
{
    if (!pData || 0 >= nSize)
        return true;

    if (offset + nSize + 1 > SizeForWrite())
        return false;

    const int readPos = m_readPos;
    const int writePos = (m_writePos + offset) & (m_maxSize - 1);
    if (readPos > writePos)
    {
        assert(readPos - writePos > nSize);
        ::memcpy(&m_buffer[writePos], pData, nSize);
    }
    else
    {
        int availBytes1 = m_maxSize - writePos;
        int availBytes2 = readPos - 0;
        assert (availBytes1 + availBytes2 >= 1 + nSize);

        if (availBytes1 >= nSize + 1)
        {
            ::memcpy(&m_buffer[writePos], pData, nSize);
        }
        else
        {
            ::memcpy(&m_buffer[writePos], pData, availBytes1);
            int bytesLeft = nSize - availBytes1;
            if (bytesLeft > 0)
                ::memcpy(&m_buffer[0], static_cast<const char*>(pData) + availBytes1, bytesLeft);
        }
    }

    return  true;
}

template <typename BUFFER>
bool CircularBuffer<BUFFER>::PushData(const void* pData, int nSize)
{
    if (!PushDataAt(pData, nSize))
        return false;

    AdjustWritePtr(nSize);
    return true;
}

template <typename BUFFER>
bool CircularBuffer<BUFFER>::PeekDataAt(void* pBuf, int nSize, int offset)
{
    if (!pBuf || 0 >= nSize)
        return true;

    if (nSize + offset > SizeForRead())
        return false;

    const int writePos = m_writePos;
    const int readPos  = (m_readPos + offset) & (m_maxSize - 1);
    if (readPos < writePos)
    {
        assert(writePos - readPos >= nSize);
        ::memcpy(pBuf, &m_buffer[readPos], nSize);
    }
    else
    {
        assert(readPos > writePos);
        int availBytes1 = m_maxSize - readPos;
        int availBytes2 = writePos - 0;
        assert(availBytes1 + availBytes2 >= nSize);

        if (availBytes1 >= nSize)
        {
            ::memcpy(pBuf, &m_buffer[readPos], nSize);            
        }
        else
        {
            ::memcpy(pBuf, &m_buffer[readPos], availBytes1);
            assert(nSize - availBytes1 > 0);
            ::memcpy(static_cast<char*>(pBuf) + availBytes1, &m_buffer[0], nSize - availBytes1);
        }
    }

    return true;
}

template <typename BUFFER>
bool CircularBuffer<BUFFER>::PeekData(void* pBuf, int nSize)
{
    if (PeekDataAt(pBuf, nSize))
        AdjustReadPtr(nSize);
    else 
        return false;

    return true;
}


template <typename BUFFER>
inline void CircularBuffer<BUFFER>::AdjustWritePtr(int size)
{
    int writePos = m_writePos;
    writePos += size;
    writePos &= m_maxSize - 1;
    AtomicSet(&m_writePos, writePos);
}

template <typename BUFFER>
inline void CircularBuffer<BUFFER>::AdjustReadPtr(int size)
{
    int readPos = m_readPos;
    readPos += size;
    readPos &= m_maxSize - 1;
    AtomicSet(&m_readPos, readPos);
}

template <typename BUFFER>
template <typename T>
inline CircularBuffer<BUFFER>& CircularBuffer<BUFFER>::operator<< (const T& data )
{
    T netData = Host2Net(data);
    if (!PushData(&netData, sizeof netData))
        ;//ERR << "Please modify the DEFAULT_BUFFER_SIZE";

    return *this;
}

template <typename BUFFER>
template <typename T>
inline CircularBuffer<BUFFER> & CircularBuffer<BUFFER>::operator>> (T& data )
{
    if (!PeekData(&data, sizeof data))
        ;//ERR << "Not enough data in m_buffer";

    data = Net2Host(data);
    return *this;
}

template <typename BUFFER>
template <typename T>
inline CircularBuffer<BUFFER>& CircularBuffer<BUFFER>::operator<< (T* const& ptr)
{
    return *this;
}

template <typename BUFFER>
template <typename T>
inline CircularBuffer<BUFFER> & CircularBuffer<BUFFER>::operator>> (T*& ptr)
{
    //ERR << "Don't load pointer!";
    return *this;
}

template <typename BUFFER>
template <typename T, int N>
inline CircularBuffer<BUFFER>&	CircularBuffer<BUFFER>::operator<< (T const (& array)[N])
{
    *this << N;
    for (int i = 0; i < N; ++ i)
    {
        *this << array[i];
    }
    return *this;
}

template <typename BUFFER>
template <typename T, int N>
inline CircularBuffer<BUFFER>&	CircularBuffer<BUFFER>::operator>> (T (& array)[N])
{
    int		n;
    *this >> n;
    assert (n == N);
    for (int i = 0; i < N; ++ i)
    {
        *this >> array[i];
    }
    return *this;
}

template <typename BUFFER>
template <typename T>
inline CircularBuffer<BUFFER> & CircularBuffer<BUFFER>::operator<< (const std::vector<T>& v)
{
    if (!v.empty())
    {
        (*this) << static_cast<unsigned short>(v.size());
        for ( typename std::vector<T>::const_iterator it = v.begin(); it != v.end(); ++it )
        {
            (*this) << *it;
        } 
    }
    return *this;
}

template <typename BUFFER>
template <typename T>
inline CircularBuffer<BUFFER> & CircularBuffer<BUFFER>::operator>> (std::vector<T>& v)
{
    v.clear();
    unsigned short size;
    *this >> size;
    v.reserve(size);

    while (size--)
    {
        T t;
        *this >> t;
        v.push_back(t);
    }
    return *this;
}

template <typename BUFFER>
template <typename T>
inline CircularBuffer<BUFFER> & CircularBuffer<BUFFER>::operator<< (const std::list<T> & l)
{
    if (!l.empty())
    {
        (*this) << static_cast<unsigned short>(l.size());
        for ( typename std::list<T>::const_iterator it = l.begin(); it != l.end(); ++it )
        {
            (*this) << *it;
        } 
    }
    return *this;
}

template <typename BUFFER>
template <typename T>
inline CircularBuffer<BUFFER> & CircularBuffer<BUFFER>::operator>> (std::list<T> & l)
{
    l.clear();
    unsigned short size;
    *this >> size;

    while (size--)
    {
        T  t;
        *this >> t;
        l.push_back(t);
    }
    return *this;
}

template <typename BUFFER>
template <typename K>
inline CircularBuffer<BUFFER> & CircularBuffer<BUFFER>::operator<< (const std::set<K>& s)
{
    if (!s.empty())
    {
        (*this) << s.size();
        for ( typename std::set<K>::const_iterator it = s.begin(); it != s.end(); ++it )
        {
            (*this) << *it;
        }
    }
    return *this;
}

template <typename BUFFER>
template <typename K>
inline CircularBuffer<BUFFER> & CircularBuffer<BUFFER>::operator>> (std::set<K>& s)
{
    s.clear();
    size_t size;
    *this >> size;
    
    while (size--)
    {
        K k;
        *this >> k;
        s.insert(k);
    }
    return *this;
}

template <typename BUFFER>
template <typename K>
inline CircularBuffer<BUFFER> & CircularBuffer<BUFFER>::operator<< (const std::multiset<K>& s)
{
    if (!s.empty())
    {
        (*this) << s.size();
        for ( typename std::multiset<K>::const_iterator it = s.begin(); it != s.end(); ++it )
        {
            (*this) << *it;
        }
    }
    return *this;
}

template <typename BUFFER>
template <typename K>
inline CircularBuffer<BUFFER> & CircularBuffer<BUFFER>::operator>> (std::multiset<K>& s)
{
    s.clear();
    size_t size;
    *this >> size;
    
    while (size--)
    {
        K k;
        *this >> k;
        s.insert(k);
    }
    return *this;
}

template <typename BUFFER>
template <typename K, typename V>
inline CircularBuffer<BUFFER> & CircularBuffer<BUFFER>::operator<< (const std::map<K,V>& m)
{
    if (!m.empty())
    {
        (*this) << m.size();
        for (typename std::map<K, V>::const_iterator it = m.begin(); it != m.end(); ++it )
        {
            (*this) << it->first << it->second;
        } 
    }
    return *this;
}

template <typename BUFFER>
template <typename K, typename V>
inline CircularBuffer<BUFFER> & CircularBuffer<BUFFER>::operator>> (std::map<K,V>& m)
{
    m.clear();
    size_t size;
    *this >> size;

    while (size--)
    {
        K k;
        V v;
        *this >> k;
        *this >> v;
        m[k] = v;
    }
    return *this;
}

template <typename BUFFER>
template <typename K, typename V>
inline CircularBuffer<BUFFER> & CircularBuffer<BUFFER>::operator<< (const std::multimap<K,V>& m)
{
    if (!m.empty())
    {
        (*this) << m.size();
        for (typename std::map<K, V>::const_iterator it = m.begin(); it != m.end(); ++it )
        {
            (*this) << it->first << it->second;
        } 
    }
    return *this;
}

template <typename BUFFER>
template <typename K, typename V>
inline CircularBuffer<BUFFER> & CircularBuffer<BUFFER>::operator>> (std::multimap<K,V>& m)
{
    m.clear();
    size_t size;
    *this >> size;

    while (size--)
    {
        K k;
        V v;
        *this >> k;
        *this >> v;
        m[k] = v;
    }
    return *this;
}

template <typename BUFFER>
inline CircularBuffer<BUFFER>& CircularBuffer<BUFFER>::operator<< (const std::string& str)
{
    *this << static_cast<unsigned short>(str.size());
    if (!PushData(str.data(), str.size()))
    {
        AdjustWritePtr(static_cast<int>(0 - sizeof(unsigned short)));
        //ERR << "2Please modify the DEFAULT_BUFFER_SIZE";
    }
    return *this;
}

template <typename BUFFER>
inline CircularBuffer<BUFFER>& CircularBuffer<BUFFER>::operator>> (std::string& str)
{
    unsigned short size = 0;
    *this >> size;
    str.clear();
    str.reserve(size);

    char ch;
    while ( size-- )
    {
        *this >> ch;
        str += ch;
    }
    return *this;
}

///////////////////////////////////////////////////////////////
typedef CircularBuffer< ::std::vector<char> >  Buffer;

template <>
inline Buffer::CircularBuffer(int maxSize) : m_maxSize(RoundUp2Power(maxSize)), m_readPos(0), m_writePos(0), m_buffer(m_maxSize)
{
    assert (0 == (m_maxSize & (m_maxSize - 1)) && "m_maxSize MUST BE power of 2");
}




typedef CircularBuffer<char [16 * 1024]> StackBuffer;

template <>
inline StackBuffer::CircularBuffer(int dummyMaxSize) : m_maxSize(8 * 1024), m_readPos(0), m_writePos(0)
{
    assert (0 == (m_maxSize & (m_maxSize - 1)) && "m_maxSize MUST BE power of 2");
}


//////////////////////////////////////////////////////////////////////////
typedef CircularBuffer<char* > AttachedBuffer;
//template <>
//inline AttachedBuffer::CircularBuffer(int maxSize)  : m_maxSize(0), m_readPos(0), m_writePos(0), m_buffer(NULL)
//{
//    // dummy  只要我不使用这个方法，不实现也无所谓
//}

template <>
inline AttachedBuffer::CircularBuffer(char* pBuf, int len) : m_maxSize(RoundUp2Power(len + 1)),
                                                             m_readPos(0),
                                                             m_writePos(0)
{
    m_buffer = pBuf;
    m_owned  = false;
}

template <>
inline AttachedBuffer::CircularBuffer(const BufferSequence& bf) :
m_readPos(0),
m_writePos(0)
{
    m_owned = false;

    if (0 == bf.count)
    {
        m_buffer = 0;
    }
    else if (1 == bf.count)
    {
        m_buffer   = (char*)bf.buffers[0].IOVEC_BUF;
        m_writePos = bf.buffers[0].IOVEC_LEN;
    }
    else if (2 == bf.count)
    {
        m_owned  = true;
        m_buffer = new char[bf.TotalBytes()];
        memcpy(m_buffer, bf.buffers[0].IOVEC_BUF, bf.buffers[0].IOVEC_LEN);
        memcpy(m_buffer + bf.buffers[0].IOVEC_LEN,
               bf.buffers[1].IOVEC_BUF, bf.buffers[1].IOVEC_LEN);
        m_writePos = bf.TotalBytes();
    }

    m_maxSize = RoundUp2Power(m_writePos - m_readPos + 1);
}

template <>
inline AttachedBuffer::~CircularBuffer()
{
    if (m_owned)
        delete [] m_buffer;
}


#endif
