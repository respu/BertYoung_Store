#ifndef BERT_SERVERTASK_H
#define BERT_SERVERTASK_H

#include "../../base/StreamSocket.h"

class ServerTask : public StreamSocket
{
public:
    bool SendMsg();

private:
    bool _HandlePacket(AttachedBuffer& buf);
};

#endif

