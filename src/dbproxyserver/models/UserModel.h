#include "DBPool.h"
#include "CachePool.h"
#include "pbs/IM.BaseDefine.pb.h"

class UserModel
{
public:
    UserModel(DBPoolPtr dbPool, CachePoolPtr cachePool);
    ~UserModel();

    void getChangedId(uint32_t& nLastTime, list<uint32_t>& lsIds);
    void getUsers(list<uint32_t> lsIds, list<IM::BaseDefine::UserInfo>& lsUsers);
    bool getUserSingInfo(uint32_t userId, string* signInfo);
    bool getPushShield(uint32_t userId, uint32_t* shieldStatus);
    void clearUserCounter(uint32_t userId, uint32_t peerId, IM::BaseDefine::SessionType sessionType);

    bool updateUserSignInfo(uint32_t userId, const string& signInfo);
private:
    DBPoolPtr dbPool_;
    CachePoolPtr cachePool_;
};
