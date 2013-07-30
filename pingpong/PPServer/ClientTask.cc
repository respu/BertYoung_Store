#include "ClientTask.h"
#include "../../base/Log/Logger.h"
#include "PPServer.h"

HEAD_LENGTH_T ClientTask::_HandleHead(AttachedBuffer& buf, BODY_LENGTH_T* bodyLen)
{
    if (buf.SizeForRead() >= sizeof(*bodyLen))
    {
        buf >> *bodyLen;
        *bodyLen = Net2Host(*bodyLen);
        return sizeof(*bodyLen);
    }

    return 0;
}

void ClientTask::_HandlePacket(AttachedBuffer& buf)
{
    WRN(SERVER->Log()) << "Recv " << buf.ReadAddr();

    StackBuffer<2 * 1024>  cmd;
    cmd << Host2Net(buf.SizeForRead());
    cmd.PushData(buf.ReadAddr(), buf.SizeForRead());

    SendPacket(cmd);
}

