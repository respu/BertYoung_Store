#ifndef BERT_PPSERVER_H
#define BERT_PPSERVER_H

#include "../../base/Server.h"

class ClientTask;
class Logger;

class PPServer : public Server
{
public:
    PPServer();
    ~PPServer();

    Logger*  Log() { return m_pLog; }
private:
    SharedPtr<StreamSocket>   _OnNewConnection(SOCKET fd);
    bool    _Init();
    bool    _RunLogic();
    Logger* m_pLog;
};

#define SERVER static_cast<PPServer* >(Server::Instance())

#endif