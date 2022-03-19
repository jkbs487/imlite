#include "DBPool.h"
#include "CachePool.h"
#include "pbs/IM.BaseDefine.pb.h"

class GroupModel
{
public:
    GroupModel(DBPoolPtr dbPool, CachePoolPtr cachePool);
    ~GroupModel();

    uint32_t createGroup(uint32_t userId, const string& groupName, const string& groupAvatar, uint32_t groupType, set<uint32_t>& members);

    void getUserGroup(uint32_t userId, list<IM::BaseDefine::GroupVersionInfo>& groups, uint32_t groupType);
    void getUserGroupIds(uint32_t nUserId, list<uint32_t>& lsGroupId, uint32_t nLimited = 100);
    void getGroupUser(uint32_t groupId, list<uint32_t>& userIds);
    void getGroupInfo(map<uint32_t,IM::BaseDefine::GroupVersionInfo>& mapGroupId, list<IM::BaseDefine::GroupInfo>& groupInfos);
    bool isInGroup(uint32_t userId, uint32_t groupId);
    bool isValidateGroupId(uint32_t nGroupId);
    uint32_t getUserJoinTime(uint32_t groupId, uint32_t userId);
    void updateGroupChat(uint32_t groupId);
    void clearGroupMember(uint32_t groupId);

private:
    void getGroupVersion(list<uint32_t>&lsGroupId, list<IM::BaseDefine::GroupVersionInfo>& lsGroup, uint32_t nGroupType);
    bool insertNewGroup(uint32_t userId, const string& groupName, const string& groupAvatar, uint32_t groupType, uint32_t memberCnt, uint32_t& groupId);
    bool insertNewMember(uint32_t groupId, set<uint32_t>& users);
    void fillGroupMember(list<IM::BaseDefine::GroupInfo>& groups);

    DBPoolPtr dbPool_;
    CachePoolPtr cachePool_;
};
