#ifndef BERT_SERVERTASK_H
#define BERT_SERVERTASK_H

#include "../../base/StreamSocket.h"

class ServerTask : public StreamSocket
{
public:
    bool SendMsg();

private:
    HEAD_LENGTH_T  _HandleHead(AttachedBuffer& buf, BODY_LENGTH_T* bodyLen);
    void _HandlePacket(AttachedBuffer& buf);
};

#endif

