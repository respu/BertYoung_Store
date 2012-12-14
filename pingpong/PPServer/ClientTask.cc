#include "ClientTask.h"

bool ClientTask::_HandlePacket(AttachedBuffer& buf)
{
    return SendPacket(buf);
}