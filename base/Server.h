
#ifndef BERT_TCPSERVER_H
#define BERT_TCPSERVER_H


#if !defined(__gnu_linux__)
    #include "./Threads/Thread.h"
#endif

#include "StreamSocket.h"
#include "TaskManager.h"

// Singleton
class Server
{
#if !defined(__gnu_linux__)
private:
    Thread  m_thread;

protected:
    bool   _StartListen();
    void   _StopListen();
#endif

protected:
    virtual bool _RunLogic() {  return  m_tasks.DoMsgParse(); }
    virtual void _Recycle() ;
    virtual bool _Init() = 0;
    bool _Bind(int port);
    bool _Connect(const char* ip, int port);
    Server();
    
public:
    virtual ~Server();
    static Server*  Instance() {   return   sm_instance;  }
    bool IsTerminate() const { return m_bTerminate; }
    void Terminate()  { m_bTerminate = true; }
    void MainLoop();
    void NewConnection(SOCKET connfd = INVALID_SOCKET);

private:
    virtual SharedPtr<StreamSocket>   _OnNewConnection(SOCKET  connfd) = 0;

    volatile bool m_bTerminate;
    TaskManager   m_tasks;
    static Server*   sm_instance;
};


#endif
