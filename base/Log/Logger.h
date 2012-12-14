#ifndef BERT_LOGGER_H
#define BERT_LOGGER_H

#include <string>
#include "../Threads/Thread.h"
#include "../Buffer.h"

class Logger
{
public:
    enum LogLevel
    {
        logINFO     = 0x01 << 0,
        logDEBUG    = 0x01 << 1,
        logWARN     = 0x01 << 2,
        logERROR    = 0x01 << 3,
        logCRITICAL = 0x01 << 4,
        logUSR      = 0x01 << 5,
        logALL      = 0xFFFFFFFF,
    };

    enum LogDest
    {
        logConsole  = 0x01 << 0,
        logFILE     = 0x01 << 1,
#if !defined(__gnu_linux__)
        logTrace    = 0x01 << 2,
#endif
        logSocket   = 0x01 << 3,  // TODO : LOG SERVER
    };

    Logger();
    ~Logger();
    bool Init(unsigned int level = logDEBUG,
        unsigned int dest = logConsole,
        const char* pDir  = 0);
    void Flush(LogLevel  level);
    bool IsLevelForbid(unsigned int level)  {  return  !(level & m_level); };

    Logger&  operator<<(const char* msg);
    Logger&  operator<<(const unsigned char* msg);
    Logger&  operator<<(void* );
    Logger&  operator<<(unsigned char a);
    Logger&  operator<<(char a);
    Logger&  operator<<(unsigned short a);
    Logger&  operator<<(short a);
    Logger&  operator<<(unsigned int a);
    Logger&  operator<<(int a);
    Logger&  operator<<(unsigned long a);
    Logger&  operator<<(long a);
    Logger&  operator<<(unsigned long long a);
    Logger&  operator<<(long long a);

    Logger& SetCurLevel(unsigned int level)
    {
        m_curLevel = level;
        return *this;
    }

    bool   Update();
    bool   Valid() const { return m_valid; }
    void   SetValid(bool valid = false) { m_valid = valid; }

private:
    NONCOPYABLE(Logger);

    static const int MAXLINE_LOG = 2048;
    char            m_tmpBuffer[MAXLINE_LOG];
    int             m_pos;
    Buffer          m_buffer;

    unsigned int    m_level;
    std::string     m_directory;
    unsigned int    m_dest;
    std::string     m_fileName;

    unsigned int    m_curLevel;

    char*           m_pMemory;
    int             m_offset;
    HANDLE          m_file;
    volatile  bool  m_valid;
#if !defined(__gnu_linux__)
    HANDLE          m_hConsole;
#endif
    bool    _CheckChangeFile();
    const std::string& _MakeFileName();
    bool    _OpenLogFile(const char* name);
    bool    _CloseLogFile();
    void    _Color(unsigned int color);
    void    _Reset();

    static bool _MakeDir(const char* pDir);
};



class LogHelper
{
public:
    LogHelper(Logger::LogLevel level);
    Logger& operator=(Logger& log);

private:
    Logger::LogLevel    m_level;
};



class LogManager
{
public:
    static LogManager& Instance();

    ~LogManager();

    Logger*  CreateLog(unsigned int level = Logger::logDEBUG,
                       unsigned int dest = Logger::logConsole,
                       const char* pDir  = 0);

    bool    StartLog();
    void    StopLog();
    bool    Update();

    Logger* NullLog()  {  return  &m_nullLog;  }

private:
    NONCOPYABLE(LogManager);

    LogManager();

    Logger  m_nullLog;

    typedef std::set<Logger* >  LOG_SET;
    LOG_SET m_logs;
    
    Thread  m_logThread;
    
    class LogThread : public Runnable
    {
    public:
        virtual void Run();
    };
};



#undef INF
#undef DBG
#undef WRN
#undef ERR
#undef CRI
#undef USR

#define  INF(pLog)      (LogHelper(Logger::logINFO))=(pLog)->SetCurLevel(Logger::logINFO)
#define  DBG(pLog)      (LogHelper(Logger::logDEBUG))=(pLog)->SetCurLevel(Logger::logDEBUG)
#define  WRN(pLog)      (LogHelper(Logger::logWARN))=(pLog)->SetCurLevel(Logger::logWARN)
#define  ERR(pLog)      (LogHelper(Logger::logERROR))=(pLog)->SetCurLevel(Logger::logERROR)
#define  CRI(pLog)      (LogHelper(Logger::logCRITICAL))=(pLog)->SetCurLevel(Logger::logCRITICAL)
#define  USR(pLog)      (LogHelper(Logger::logUSR))=(pLog)->SetCurLevel(Logger::logUSR)


// Ugly debug codes, only for test
#ifdef  DEBUG_BERT_SDK
    extern  Logger*  g_sdkLog;
    extern  Mutex    g_sdkMutex;
    
    #define LOCK_SDK_LOG    g_sdkMutex.Lock();
    #define UNLOCK_SDK_LOG  g_sdkMutex.Unlock();
    #define DEBUG_SDK       DBG(g_sdkLog ? g_sdkLog : LogManager::Instance().NullLog())

#else
    #define LOCK_SDK_LOG
    #define UNLOCK_SDK_LOG
    #define DEBUG_SDK       DBG(LogManager::Instance().NullLog())

#endif


#endif
