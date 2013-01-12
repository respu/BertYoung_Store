#ifndef BERT_CLIENTTASK_H
#define BERT_CLIENTTASK_H


#include "../../base/StreamSocket.h"

class ClientTask : public StreamSocket
{
private:
    HEAD_LENGTH_T _HandleHead(AttachedBuffer& buf, BODY_LENGTH_T* bodyLen);
    void _HandlePacket(AttachedBuffer& buf);
};


#endif

