
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <errno.h>

#if defined(__gnu_linux__)
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <sys/mman.h>

    #define RED_COLOR       1
    #define GREEN_COLOR     2
    #define YELLOW_COLOR    3
    #define NORMAL_COLOR    4
    #define BLUE_COLOR      5
    #define PURPLE_COLOR    6
    #define WHITE_COLOR     7
#else
    #include <direct.h>

    #define RED_COLOR       (FOREGROUND_RED | FOREGROUND_INTENSITY)
    #define GREEN_COLOR     (FOREGROUND_GREEN | FOREGROUND_INTENSITY)
    #define YELLOW_COLOR    (FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY)
    #define NORMAL_COLOR    (FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE)
    #define BLUE_COLOR      (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
    #define PURPLE_COLOR    (FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY)
    #define WHITE_COLOR     (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#endif
#define COLOR_MAX        8

#include "Logger.h"
#include "../Timer.h"



#ifdef  DEBUG_BERT_SDK
    Logger*  g_sdkLog = NULL;
    Mutex    g_sdkMutex;
#endif

static const int DEFAULT_LOGFILESIZE = 8 * 1024 * 1024;
static const int MAXLINE_LOG         = 1024;
static const int PREFIX_LEVEL_LEN  = 6;
static const int PREFIX_TIME_LEN   = 23;

Logger::Logger() : m_buffer(64 * 1024 * 1024),
m_level(0),
m_dest(0),
m_pMemory(0),
m_offset(DEFAULT_LOGFILESIZE), 
m_file(INVALID_HANDLE_VALUE),
m_valid(true)
{
    _Reset();
    m_fileName.reserve(32);
#if !defined(__gnu_linux__)
    m_hConsole = ::GetStdHandle(STD_OUTPUT_HANDLE);
#endif
}

Logger::~Logger()
{
    _CloseLogFile();
}

bool Logger::Init(unsigned int level, unsigned int dest, const char* pDir)
{
    m_level      = level;
    m_dest       = dest;
    m_directory  = pDir ? pDir : ".";

    if (0 == m_level)
    {
        std::cout << "Init log with level 0\n";
        return  true;
    }
  
    if (m_dest & logFILE)
    {
        if (m_directory != ".")
            return _MakeDir(m_directory.c_str());
    }

    return true;
}

bool Logger::_CheckChangeFile()
{
    if (INVALID_HANDLE_VALUE == m_file)
        return true;
    
    return m_offset + MAXLINE_LOG >= DEFAULT_LOGFILESIZE;
}

const std::string& Logger::_MakeFileName()
{
    char   name[32];
    Time   now;
    now.FormatTime(name, sizeof(name) - 1);

    m_fileName  = m_directory + "/" + name;
    m_fileName += ".log";

    return m_fileName;
}

bool Logger::_OpenLogFile(const char* name)
{ 
    // CLOSE PREVIOUS LOG FILE PLEASE!
    if (INVALID_HANDLE_VALUE != m_file)
        return false;  

#if defined(__gnu_linux__)
    m_file = ::open(name, O_RDWR | O_CREAT | O_APPEND, 0644);
    if (INVALID_HANDLE_VALUE == m_file)
        return false;

    struct stat st;
    fstat(m_file, &st);
    m_offset  = st.st_size; // for append

    ::ftruncate(m_file, DEFAULT_LOGFILESIZE);
    m_pMemory = (char* )::mmap(0, DEFAULT_LOGFILESIZE, PROT_WRITE, MAP_SHARED, m_file, 0);
    return (char*)-1 != m_pMemory;

#else
    m_file = ::CreateFileA(name, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (INVALID_HANDLE_VALUE == m_file)
    {
        std::cerr << "Can not open file " << name << ", because " << GetLastError();
        return false;
    }

    DWORD dummyHighSize = 0;
    m_offset = ::GetFileSize(m_file, &dummyHighSize); // for append

    HANDLE hFileMapping = ::CreateFileMappingA(m_file, 0, PAGE_READWRITE, 0, DEFAULT_LOGFILESIZE, 0);
    if (!hFileMapping)
        return false;

    m_pMemory = (char* )::MapViewOfFile(hFileMapping, FILE_MAP_WRITE, 0, 0, DEFAULT_LOGFILESIZE);
    ::CloseHandle(hFileMapping);
    return 0 != m_pMemory;

#endif
}

bool Logger::_CloseLogFile()
{
    bool bOk = true;
    if (INVALID_HANDLE_VALUE != m_file)
    {
#if defined(__gnu_linux__)
        ::munmap(m_pMemory, DEFAULT_LOGFILESIZE);
        ::ftruncate(m_file, m_offset);
        ::close(m_file);

#else
        ::UnmapViewOfFile(m_pMemory); // UNMAP FIRST, OR CAN NOT SET SIZE
        ::SetFilePointer(m_file, m_offset, 0, FILE_BEGIN);
        bOk = (TRUE == ::SetEndOfFile(m_file));
        ::CloseHandle(m_file);

#endif
        m_pMemory   = 0;
        m_offset    = 0;
        m_file      = INVALID_HANDLE_VALUE;
    }
    return bOk;    
}


void Logger::Flush(enum LogLevel level)
{
    if (IsLevelForbid(m_curLevel) ||
        !(level & m_curLevel) ||
         (m_pos <= PREFIX_TIME_LEN + PREFIX_LEVEL_LEN)) 
    {
        _Reset();
        return;
    }

    g_now.Now();
    g_now.FormatTime(m_tmpBuffer, PREFIX_TIME_LEN + 1);

    switch(level)  
    {
    case logINFO:
        strncpy(m_tmpBuffer + PREFIX_TIME_LEN, "[INF]:", PREFIX_LEVEL_LEN);
        break;

    case logDEBUG:
        strncpy(m_tmpBuffer + PREFIX_TIME_LEN, "[DBG]:", PREFIX_LEVEL_LEN);
        break;

    case logWARN:
        strncpy(m_tmpBuffer + PREFIX_TIME_LEN, "[WRN]:", PREFIX_LEVEL_LEN);
        break;

    case logERROR:
        strncpy(m_tmpBuffer + PREFIX_TIME_LEN, "[ERR]:", PREFIX_LEVEL_LEN);
        break;

    case logCRITICAL:
        strncpy(m_tmpBuffer + PREFIX_TIME_LEN, "[CRI]:", PREFIX_LEVEL_LEN);
        break;

    case logUSR:
        strncpy(m_tmpBuffer + PREFIX_TIME_LEN, "[USR]:", PREFIX_LEVEL_LEN);
        break;

    default:    
        strncpy(m_tmpBuffer + PREFIX_TIME_LEN, "[???]:", PREFIX_LEVEL_LEN);
        break;
    }

#if !defined(__gnu_linux__)
    m_tmpBuffer[m_pos ++] = '\r';

#endif

    m_tmpBuffer[m_pos ++] = '\n';
    m_tmpBuffer[m_pos] = '\0';

    // Format: level info, length info, log msg
    int logLevel = level;
    if (m_buffer.PushDataAt(&logLevel, sizeof logLevel) && 
        m_buffer.PushDataAt(&m_pos, sizeof m_pos, sizeof logLevel) && 
        m_buffer.PushDataAt(m_tmpBuffer, m_pos, sizeof logLevel + sizeof m_pos))
    {
        m_buffer.AdjustWritePtr(sizeof logLevel + sizeof m_pos + m_pos);
    }

    _Reset();
}

void Logger::_Color(unsigned int color)
{
#if defined(__gnu_linux__)
    static const char* colorstrings[COLOR_MAX] = {
        "",
        "\033[1;31;40m",
        "\033[1;32;40m",
        "\033[1;33;40m",
        "\033[0m",
        "\033[1;34;40m",
        "\033[1;35;40m",
        "\033[1;37;40m",
    };
    fprintf(stdout, colorstrings[color]);
#else
    ::SetConsoleTextAttribute(m_hConsole, (WORD)color);
#endif
}

bool Logger::_MakeDir(const char* pDir)
{
#if defined(__gnu_linux__)
    if (pDir && 0 != mkdir(pDir, 0755))
#else
    if (pDir && 0 != _mkdir(pDir))
#endif
    {
        if (EEXIST != errno)
            return false;
    }
    return true;
}

Logger&  Logger::operator<< (const char* msg)
{
    if (IsLevelForbid(m_curLevel))
    {
        return *this;
    }

    const int len = strlen(msg);
    if (m_pos + len >= MAXLINE_LOG)  
    {
        return *this;
    }

    memcpy(m_tmpBuffer + m_pos, msg, len);
    m_pos += len;

    return  *this;
}

Logger&  Logger::operator<< (const unsigned char* msg)
{
    return operator<<(reinterpret_cast<const char*>(msg));
}

Logger&  Logger::operator<< (void* ptr)
{
    if (IsLevelForbid(m_curLevel))
    {
        return *this;
    }
    
    if (m_pos + 18 < MAXLINE_LOG)
    {  
        unsigned long ptrValue = (unsigned long)ptr;
        int nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%#018lx", ptrValue);
        if (nbytes > 0) m_pos += nbytes;
    }

    return  *this;
}


Logger&  Logger::operator<< (unsigned char a)
{
    if (IsLevelForbid(m_curLevel))
    {
        return *this;
    }

    if (m_pos + 3 < MAXLINE_LOG)
    {
        int  nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%hhd", a);
        if (nbytes > 0) m_pos += nbytes;
    }

    return  *this;
}


Logger&  Logger::operator<< (char a)
{
    if (IsLevelForbid(m_curLevel))
    {
        return *this;
    }
    if (m_pos + 3 < MAXLINE_LOG)
    {
        int  nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%hhu", a);
        if (nbytes > 0) m_pos += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (unsigned short a)
{
    if (IsLevelForbid(m_curLevel))
    {
        return *this;
    }
    if (m_pos + 5 < MAXLINE_LOG)
    {
        int  nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%hu", a);
        if (nbytes > 0) m_pos += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (short a)
{
    if (IsLevelForbid(m_curLevel))
    {
        return *this;
    }
    if (m_pos + 5 < MAXLINE_LOG)
    {
        int  nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%hd", a);
        if (nbytes > 0) m_pos += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (unsigned int a)
{
    if (IsLevelForbid(m_curLevel))
    {
        return *this;
    }
    if (m_pos + 10 < MAXLINE_LOG)
    {
        int  nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%u", a);
        if (nbytes > 0) m_pos += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (int a)
{
    if (IsLevelForbid(m_curLevel))
    {
        return *this;
    }
    if (m_pos + 10 < MAXLINE_LOG)
    {
        int  nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%d", a);
        if (nbytes > 0) m_pos += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (unsigned long a)
{
    if (IsLevelForbid(m_curLevel))
    {
        return *this;
    }
    if (m_pos + 20 < MAXLINE_LOG)
    {
        int  nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%lu", a);
        if (nbytes > 0) m_pos += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (long a)
{
    if (IsLevelForbid(m_curLevel))
    {
        return *this;
    }
    if (m_pos + 20 < MAXLINE_LOG)
    {
        int  nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%ld", a);
        if (nbytes > 0) m_pos += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (unsigned long long a)
{
    if (IsLevelForbid(m_curLevel))
    {
        return *this;
    }
    if (m_pos + 20 < MAXLINE_LOG)
    {
        int  nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%llu", a);
        if (nbytes > 0) m_pos += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (long long a)
{
    if (IsLevelForbid(m_curLevel))
    {
        return *this;
    }
    if (m_pos + 20 < MAXLINE_LOG)
    {
        int  nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%lld", a);
        if (nbytes > 0) m_pos += nbytes;
    }

    return  *this;
}


void   Logger::_Reset()
{
    m_curLevel = 0;
    m_pos  = PREFIX_LEVEL_LEN + PREFIX_TIME_LEN ;
}

bool Logger::Update()
{
    if (m_buffer.IsEmpty())
        return false;

    while (!m_buffer.IsEmpty())
    {
        int level = 0;
        m_buffer.PeekDataAt(&level, sizeof level);
        m_buffer.AdjustReadPtr(sizeof level);

        int nLen = 0;
        m_buffer.PeekDataAt(&nLen, sizeof nLen);
        m_buffer.AdjustReadPtr(sizeof nLen);

        BufferSequence  bf;
        m_buffer.GetDatum(bf, nLen);

        AttachedBuffer   content(bf);

        if (m_dest & logConsole)
        {
            switch (level)
            {
            case logINFO:
                _Color(GREEN_COLOR);
                break;

            case logDEBUG:
                _Color(WHITE_COLOR);
                break;

            case logWARN:
                _Color(YELLOW_COLOR);
                break;

            case logERROR:
                _Color(RED_COLOR);
                break;

            case logCRITICAL:
                _Color(PURPLE_COLOR);
                break;

            case logUSR:
                _Color(BLUE_COLOR);
                break;

            default:
                _Color(RED_COLOR);
                break;
            }

            fprintf(stdout, "%.*s", content.SizeForRead(), content.ReadAddr());
            _Color(NORMAL_COLOR);
        }

#if !defined(__gnu_linux__)
        if (m_dest & logTrace)
            OutputDebugStringA(content.ReadAddr());
#endif

        if (m_dest & logFILE)
        {
            while (_CheckChangeFile())
            {
                if (!_CloseLogFile() || !_OpenLogFile(_MakeFileName().c_str()))
                {   //OOPS!!! IMPOSSIBLE!
                    break;
                }
            }

            if (m_pMemory && m_pMemory != (char*)-1)
            {
                ::memcpy(m_pMemory + m_offset, content.ReadAddr(), content.SizeForRead());
                m_offset += nLen;
            }
        }

        m_buffer.AdjustReadPtr(nLen);
    }

    return true;
}


LogHelper::LogHelper(Logger::LogLevel level) : m_level(level)    
{
}
    
Logger& LogHelper::operator=(Logger& log) 
{
    log.Flush(m_level);
    return  log;
}



LogManager& LogManager::Instance()
{
    static LogManager mgr;
    return mgr;
}

LogManager::LogManager()
{
    m_nullLog.Init(0);
}

LogManager::~LogManager()
{
    LOG_SET::iterator it(m_logs.begin());
    for ( ; it != m_logs.end(); ++ it)
    {
        (*it)->Update();
        delete *it;    
    }

    m_logs.clear();
}

Logger*  LogManager::CreateLog(unsigned int level ,
               unsigned int dest ,
               const char* pDir)
{
    assert (m_logThread.Idle() && "You must create log before log-thread running");

    Logger*  pLog(new Logger);
    if (!pLog->Init(level, dest, pDir))
    {
        delete pLog;
        return &m_nullLog;
    }
    else
    {
        m_logs.insert(pLog);
    }

    return pLog;
}

bool LogManager::StartLog()
{
    assert (m_logThread.Idle());
    m_logThread.SetRunnableEntity(new LogThread);
    return m_logThread.Start();
}

void LogManager::StopLog()
{
    m_logThread.StopAndWait();
}

bool LogManager::Update()
{
    bool busy = false;

    for (LOG_SET::iterator it(m_logs.begin());
        it != m_logs.end();
        )
    {
        Logger* pLog = *it;

        if (pLog->Update())
        {
            busy = true;
        }

        // Delete after update because when other thread exit
        // Log-thread will still flush the buffered log message.
        if (pLog->Valid())
        {
            ++ it;
        }
        else
        {
            delete  pLog;
            m_logs.erase(it ++);
        }
    }

    return  busy;
}


void  LogManager::LogThread::Run()
{
    while (IsAlive())
    {
        if (!LogManager::Instance().Update())
            Thread::YieldCPU();
    }
}
