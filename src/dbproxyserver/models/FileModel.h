#include "DBPool.h"
#include "CachePool.h"
#include "pbs/IM.BaseDefine.pb.h"

class FileModel
{
public:
    FileModel(DBPoolPtr dbPool, CachePoolPtr cachePool);
    ~FileModel();

    void getOfflineFile(uint32_t userId, list<IM::BaseDefine::OfflineFileInfo>& offlines);
    void addOfflineFile(uint32_t fromId, uint32_t toId, string& taskId, string& fileName, uint32_t fileSize);

private:

    DBPoolPtr dbPool_;
    CachePoolPtr cachePool_;
};
