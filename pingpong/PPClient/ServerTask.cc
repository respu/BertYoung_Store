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
    msgLen = snprintf(msg + sizeof(BODY_LENGTH_T), sizeof msg - sizeof(BODY_LENGTH_T), "hello server%d", seq);
    *(BODY_LENGTH_T*)msg = Host2Net(msgLen);
    SendPacket(msg, sizeof(BODY_LENGTH_T));
    Thread::YieldCPU();
    SendPacket(msg + sizeof(BODY_LENGTH_T), msgLen);

 //   return SendPacket(msg, msgLen + sizeof(BODY_LENGTH_T));
}

HEAD_LENGTH_T ServerTask::_HandleHead(AttachedBuffer& buf, BODY_LENGTH_T* bodyLen)
{
    if (buf.SizeForRead() >= sizeof(*bodyLen))
    {
        buf >> *bodyLen;
        return sizeof(*bodyLen);
    }

    return 0;
}

void ServerTask::_HandlePacket(AttachedBuffer& buf)
{
    DBG(SERVER->Log()) << "Recv " << buf.ReadAddr();

    SendMsg();
}
