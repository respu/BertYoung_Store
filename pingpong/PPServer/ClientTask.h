#ifndef BERT_CLIENTTASK_H
#define BERT_CLIENTTASK_H


#include "../../base/StreamSocket.h"

class ClientTask : public StreamSocket
{
private:
    bool _HandlePacket(AttachedBuffer& buf);
};


#endif

