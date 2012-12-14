 
#ifndef BERT_ENTRYMANAGER_H
#define BERT_ENTRYMANAGER_H

#include <map>
#include "SmartPtr/SharePtr.h"

struct Entry
{
public:
    virtual ~Entry()
    {
    }

    union
    {
        unsigned int id;
        struct
        {
            unsigned short word1;
            unsigned short word2;
        };
        struct
        {
            unsigned char byte1;
            unsigned char byte2;
            unsigned char byte3;
            unsigned char byte4;
        };
    };
};


template < typename T, typename RETURNTYPE = bool>
struct Callback
{
    virtual RETURNTYPE Exec(SharedPtr<T> entry) = 0;

    virtual ~Callback()
    {
    }
};

template <typename ENTRY>
class EntryManager
{
public:
    EntryManager() { }
    
    // Insert entry
    bool    AddEntry( SharedPtr<ENTRY> entry)
    {
        return container.insert(std::make_pair(entry->id, entry)).second;
    }

    // Remove entry
    bool    RemoveEntry( SharedPtr<ENTRY> entry)
    {
        if (!entry.Get())  return false;

         return container.erase(entry->id) > 0;
    }
    
    // Remove entry
    bool    RemoveEntry( unsigned int id)
    {
        return container.erase(id) > 0;
    }

    // Get entry
    SharedPtr<ENTRY>  GetEntry( unsigned int id )
    {
        typename CONTAINER::iterator it = container.find(id);
        if (it != container.end())
            return it->second;
        else
            return SharedPtr<ENTRY>();
    }

    unsigned int    Size() const    { return container.size();    }

    // Empty?
    bool    Empty() const    {    return container.empty();    }

    // Clear all entries
    void    Clear()    {    container.clear(); }
    
    // Callback on every entry
    bool    ExecEveryEntry(Callback<ENTRY> & exec)
    {
        typename CONTAINER::iterator it = container.begin();
        typename CONTAINER::iterator tmp;
        while (it != container.end())
        {
            tmp = it;
            ++it;
            if (!exec.Exec((SharedPtr<ENTRY>)tmp->second))
                return false;
        }
        return true;
    }

private:
    // Container for manage entries
    typedef std::map<unsigned int, SharedPtr<ENTRY> > CONTAINER;
    CONTAINER     container;

    EntryManager(const EntryManager& );
    EntryManager& operator = (const EntryManager& );
};

#endif
