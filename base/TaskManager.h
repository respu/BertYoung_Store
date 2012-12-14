#ifndef BERT_TASKMANAGER_H
#define BERT_TASKMANAGER_H

#include <vector>
#include "./Threads/IPC.h"
#include "EntryManager.h"

/* old code need mutex, because net thread find tcp disconnect,
 * it will delete the tcp pointer here
 * now use shared_ptr, mutex is not need. net thread only set invalid
 * logic thread oper TaskManager，find invalid, then delete
 * Net: lock(); push_new_task(); unlock();
 * Logical; trylock(); pop_new_task(); unlock();
 */

class StreamSocket;

template <typename T>
class CircularBuffer;

typedef CircularBuffer<char [16 * 1024]> StackBuffer;

class TaskManager : public EntryManager<StreamSocket>
{
protected:
    
    typedef std::vector<SharedPtr<StreamSocket> >  NEWTASKS_T;

    // Lock for new tasks
    Mutex           m_lock;
    NEWTASKS_T      m_newTasks; 
    volatile  int   m_newCnt;
    /* because vector::empty() is not thread-safe
     * I use an int var to represent number of new conn
     * when logical thread find it zero, no need try lock
     */

private:
    // Add task
    bool _AddTask(SharedPtr<StreamSocket> task)  {   return AddEntry(task);  }

public:
    TaskManager() : m_newCnt(0) { }

    // Destructor
    virtual ~TaskManager();
    
     //    Callback on every TASK
    bool ExecEveryTask(Callback<StreamSocket> & exec);
    // Add task
    bool AddTask(SharedPtr<StreamSocket> task);

    // Remove task
    bool RemoveTask(SharedPtr<StreamSocket> task );

    // 得到连接数量
    unsigned int GetSize() const {   return Size();    }

    // Broadcast msg to all tasks
    bool Broadcast(StackBuffer& cmd);

    // send msg to only one
    bool SendToID(StackBuffer& cmd, unsigned int id);

    // Broadcast msg to all tasks
    bool BroadcastExceptID(StackBuffer& cmd, unsigned int id);

    //  逻辑线程解析每一条连接收到的消息
    bool DoMsgParse();

};

#endif

