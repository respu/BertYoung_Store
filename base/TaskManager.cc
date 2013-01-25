#include "TaskManager.h"


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
