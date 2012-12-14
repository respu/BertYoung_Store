#include "TaskManager.h"
#include "StreamSocket.h"


TaskManager::~TaskManager()
{
    assert(Empty() && "Why you do not clear container before exit?");

    ScopeMutex  guard(m_lock);
    m_newTasks.clear();
    m_newCnt = 0;
}
    
     
bool TaskManager::ExecEveryTask(Callback<StreamSocket> & exec)
{
    return ExecEveryEntry(exec);
}


bool TaskManager::AddTask(SharedPtr<StreamSocket> task)    
{   
    {
        ScopeMutex guard(m_lock);
        m_newTasks.push_back(task);
        ++ m_newCnt;
    }

    return  true;    
}


bool TaskManager::RemoveTask(SharedPtr<StreamSocket> task )    
{
    return RemoveEntry(task);    
}


bool TaskManager::Broadcast(StackBuffer& cmd)
{
    struct Broadcast : public Callback<StreamSocket>
    {
    private:
        StackBuffer& cmd;

    public:
        Broadcast(StackBuffer& rcmd) : cmd(rcmd)
        {
        }

        bool Exec(SharedPtr<StreamSocket> pSock)
        {
            if (!pSock.Get() || pSock->Invalid())
                return false;

            return pSock->SendPacket(cmd);
        }
    } sendToAll(cmd);

    ExecEveryTask(sendToAll);
    return true;
}


bool TaskManager::SendToID(StackBuffer& cmd, unsigned int id)
{
    SharedPtr<StreamSocket> pSock = GetEntry(id);

    if (!pSock.Get() || pSock->Invalid())
        return false;

    return pSock->SendPacket(cmd);
}


bool TaskManager::BroadcastExceptID(StackBuffer& cmd, unsigned int id)
{
    struct Broadcast : public Callback<StreamSocket>
    {
        StackBuffer&    cmd;
        unsigned int    id;

        Broadcast(StackBuffer& rcmd, unsigned int i) : cmd(rcmd), id(i)
        {
        }
        
        bool Exec(SharedPtr<StreamSocket> pSock)
        {
            if (pSock.Get() && pSock->id != id)
                pSock->SendPacket(cmd);
            
            return true;
        }
    } sendAllButOne(cmd, id);

    ExecEveryTask(sendAllButOne);
    return true;
}
    
bool TaskManager::DoMsgParse()
{
    struct DoCmd : public Callback<StreamSocket>
    {
        TaskManager*  m_pTaskMgr;
        bool          m_busy;

        DoCmd() : m_pTaskMgr(0), m_busy(false) { }

        bool Exec(SharedPtr<StreamSocket> pSock)
        {  
            // 这里会将无效连接删除掉
            if (!pSock.Get() || pSock->Invalid())
            {
                m_pTaskMgr->RemoveTask(pSock);
            }
            else
            {
                bool busy = pSock->DoMsgParse();
                if (!m_busy && busy)  m_busy = true;
            }

            return true;
        }
    } parseCmd;

    // 先尝试接受新连接
    if (m_newCnt > 0 && m_lock.TryLock())
    {
        NEWTASKS_T  tmpNewTask;
        tmpNewTask.swap(m_newTasks);
        m_newCnt = 0;
        m_lock.Unlock();

        for (NEWTASKS_T::iterator it(tmpNewTask.begin());
            it != tmpNewTask.end();
            ++ it)
        {
            if (!_AddTask(*it))
            {
                int* p = 0;
                *p = 0;
            }
        }
    }

    parseCmd.m_pTaskMgr = this;
    ExecEveryTask(parseCmd);

    return parseCmd.m_busy;
}
