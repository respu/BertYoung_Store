#include <cstdio>
#include "ServerTask.h"
#include "PPClient.h"
#include "../../base/Log/Logger.h"


static int  seq = 0; 
static char msg[32];
static int  msgLen = 0;


bool ServerTask::SendMsg()
{
    ++ seq;
    msgLen = snprintf(msg, sizeof msg, "hello server%d", seq);

    return  SendPacket(msg, msgLen);
}

bool ServerTask::_HandlePacket(AttachedBuffer& buf)
{
    return SendMsg();
}
