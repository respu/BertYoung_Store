#ifndef BERT_PPCLIENT_H
#define BERT_PPCLIENT_H

#include "../../base/Server.h"

class ServerTask;
class Logger;

class PPClient : public Server
{
public:
    PPClient();
    ~PPClient();

    Logger*  Log() { return m_pLog; }

private:
    SharedPtr<StreamSocket>   _OnNewConnection(SOCKET fd);
    bool _Init();
    bool _RunLogic();

    Logger* m_pLog;
};

#define SERVER static_cast<PPClient* >(TCPServer::Instance())

#endif
