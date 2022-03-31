#include "GroupModel.h"
#include "GroupMessageModel.h"

#include "slite/Logger.h"

using namespace slite;

GroupModel::GroupModel(DBPoolPtr dbPool, CachePoolPtr cachePool)
    : dbPool_(dbPool),
    cachePool_(cachePool)
{
}

GroupModel::~GroupModel()
{
}

void GroupModel::getGroupUser(uint32_t groupId, list<uint32_t>& userIds)
{
    CacheConn* cacheConn = cachePool_->getCacheConn();
    if (cacheConn)
    {
        string key = "group_member_" + std::to_string(groupId);
        map<string, string> mapAllUser;
        bool ret = cacheConn->hgetAll(key, mapAllUser);
        cachePool_->relCacheConn(cacheConn);
        if(ret) {
            for (const auto& mapUser : mapAllUser) {
                uint32_t userId = std::stoi(mapUser.first);
                userIds.push_back(userId);
            }
        } else {
            LOG_ERROR << "hgetall " << key << " failed!";
        }
    } else {
        LOG_ERROR << "no cache connection for group_member";
    }
}

void GroupModel::getUserGroup(uint32_t userId, list<IM::BaseDefine::GroupVersionInfo>& groups, uint32_t groupType)
{
    list<uint32_t> groupIds;
    getUserGroupIds(userId, groupIds, 0);
    if (groupIds.size() != 0)
    {
        getGroupVersion(groupIds, groups, groupType);
    }
}

void GroupModel::getUserGroupIds(uint32_t userId, list<uint32_t>& groupIds, uint32_t limited)
{
    DBConn* dbConn = dbPool_->getDBConn();
    if(dbConn) {
        string strSql;
        if (limited != 0) {
            strSql = "SELECT groupId FROM IMGroupMember WHERE userId = " + 
                std::to_string(userId) + " AND status = 0 ORDER BY updated DESC, id DESC LIMIT " + std::to_string(limited);
        } else {
            strSql = "SELECT groupId FROM IMGroupMember WHERE userId = " + 
                std::to_string(userId) + " AND status = 0 ORDER BY updated DESC, id DESC";
        }
        
        ResultSet* resultSet = dbConn->executeQuery(strSql);
        if (resultSet) {
            while (resultSet->next()) {
                uint32_t groupId = resultSet->getInt("groupId");
                groupIds.push_back(groupId);
            }
            delete resultSet;
        } else{
            LOG_ERROR << "no result set for sql: %s" << strSql;
        }
        dbPool_->relDBConn(dbConn);
    } else {
        LOG_ERROR << "no db connection for teamtalk";
    }
}

void GroupModel::getGroupVersion(list<uint32_t>& groupIds, list<IM::BaseDefine::GroupVersionInfo>& groups, uint32_t groupType)
{
    if (groupIds.empty()) {
        LOG_ERROR << "group ids is empty";
        return;
    }
    DBConn* dbConn = dbPool_->getDBConn();
    if (dbConn) {
        string clause;
        bool first = true;
        for (const auto& groupId : groupIds) {
            if (first) {
                first = false;
                clause = std::to_string(groupId);
            } else {
                clause += ("," + std::to_string(groupId));
            }
        }
        
        string strSql = "SELECT id, version FROM IMGroup WHERE id IN (" +  clause  + ")";
        if(0 != groupType) {
            strSql += " AND type = " + std::to_string(groupType);
        }
        strSql += " ORDER BY updated DESC";
        
        ResultSet* resultSet = dbConn->executeQuery(strSql);
        if (resultSet) {
            while(resultSet->next()) {
                IM::BaseDefine::GroupVersionInfo group;
                group.set_group_id(resultSet->getInt("id"));
                group.set_version(resultSet->getInt("version"));
                groups.push_back(group);
            }
            delete resultSet;
        } else {
            LOG_ERROR << "no result set for sql: %s" << strSql;
        }
        dbPool_->relDBConn(dbConn);
    }
    else {
        LOG_ERROR << "no db connection for teamtalk";
    }
}

bool GroupModel::isInGroup(uint32_t userId, uint32_t groupId)
{
    bool ret = false;
    CacheConn* cacheConn = cachePool_->getCacheConn();
    if (cacheConn) {
        string key = "group_member_" + std::to_string(groupId);
        string field = std::to_string(userId);
        string value = cacheConn->hget(key, field);
        cachePool_->relCacheConn(cacheConn);
        if (!value.empty()) {
            ret = true;
        }
    } else {
        LOG_ERROR << "no cache connection for group_member";
    }
    return ret;
}

uint32_t GroupModel::getUserJoinTime(uint32_t groupId, uint32_t userId)
{
    uint32_t time = 0;
    CacheConn* cacheConn = cachePool_->getCacheConn();
    if (cacheConn) {
        string key = "group_member_" + std::to_string(groupId);
        string field = std::to_string(userId);
        string value = cacheConn->hget(key, field);
        cachePool_->relCacheConn(cacheConn);
        if (!value.empty()) {
            time = std::stoi(value);
        }
    } else {
        LOG_ERROR << "no cache connection for group_member";
    }
    return time;
}

bool GroupModel::isValidateGroupId(uint32_t groupId)
{
    bool ret = false;
    CacheConn* cacheConn = cachePool_->getCacheConn();
    if (cacheConn) {
        string strKey = "group_member_" + std::to_string(groupId);
        ret = cacheConn->isExists(strKey);
        cachePool_->relCacheConn(cacheConn);
    }
    return ret;
}

void GroupModel::updateGroupChat(uint32_t groupId)
{
    DBConn* dbConn = dbPool_->getDBConn();
    if (dbConn) {
        uint32_t now = static_cast<uint32_t>(time(NULL));
        string strSql = "UPDATE IMGroup SET lastChated=" + std::to_string(now) + " where id=" + std::to_string(groupId);
        dbConn->executeUpdate(strSql);
        dbPool_->relDBConn(dbConn);
    } else {
        LOG_ERROR << "no db connection for teamtalk_master";
    }
}

uint32_t GroupModel::createGroup(uint32_t userId, const string& groupName, const string& groupAvatar, uint32_t groupType, set<uint32_t>& members)
{
    uint32_t groupId = 0;
    do {
        if (groupName.empty()) {
            break;
        }
        if (members.empty()) {
            break;
        }
        // remove repeat user
        
        
        //insert IMGroup
        if(!insertNewGroup(userId, groupName, groupAvatar, groupType, (uint32_t)members.size(), groupId)) {
            break;
        }
        GroupMessageModel groupMessageModel(dbPool_, cachePool_);
        bool bRet = groupMessageModel.resetMsgId(groupId);
        if (!bRet) {
            LOG_ERROR << "reset msgId failed. groupId=" << groupId;
        }
        
        //insert IMGroupMember
        clearGroupMember(groupId);
        insertNewMember(groupId, members);
        
    } while (false);
    
    return groupId;
}

bool GroupModel::insertNewGroup(uint32_t userId, const string& groupName, const string& groupAvatar, uint32_t groupType, uint32_t memberCnt, uint32_t& groupId)
{
    bool ret = false;
    groupId = 0;
    DBConn* dbConn = dbPool_->getDBConn();
    if (dbConn) {
        string strSql = "insert into IMGroup(`name`, `avatar`, `creator`, `type`,`userCnt`, `status`, `version`, `lastChated`, `updated`, `created`) "\
        "values(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
        
        PrepareStatement* stmt = new PrepareStatement();
        if (stmt->init(dbConn->getMysql(), strSql)) {
            uint32_t created = static_cast<uint32_t>(time(NULL));
            uint32_t index = 0;
            uint32_t status = 0;
            uint32_t version = 1;
            uint32_t lastChat = 0;
            stmt->setParam(index++, groupName);
            stmt->setParam(index++, groupAvatar);
            stmt->setParam(index++, userId);
            stmt->setParam(index++, groupType);
            stmt->setParam(index++, memberCnt);
            stmt->setParam(index++, status);
            stmt->setParam(index++, version);
            stmt->setParam(index++, lastChat);
            stmt->setParam(index++, created);
            stmt->setParam(index++, created);
            
            ret = stmt->executeUpdate();
            if(ret) {
                groupId = stmt->getInsertId();
            }
        }
        delete stmt;
        dbPool_->relDBConn(dbConn);
    }
    else {
        LOG_ERROR << "no db connection for teamtalk_master";
    }
    return ret;
}

void GroupModel::clearGroupMember(uint32_t groupId)
{
    DBConn* dbConn = dbPool_->getDBConn();
    if (dbConn) {
        string strSql = "delete from IMGroupMember where groupId=" + std::to_string(groupId);
        dbConn->executeUpdate(strSql);
        dbPool_->relDBConn(dbConn);
    } else {
        LOG_ERROR << "no db connection for teamtalk_master";
    }
    CacheConn* cacheConn = cachePool_->getCacheConn();
    if (cacheConn) {
        string strKey = "group_member_" + std::to_string(groupId);
        map<string, string> mapRet;
        bool ret = cacheConn->hgetAll(strKey, mapRet);
        if (ret) {
            for (auto it = mapRet.begin(); it != mapRet.end(); ++it) {
                cacheConn->hdel(strKey, it->first);
            }
        } else {
            LOG_ERROR << "hgetall " << strKey << " failed";
        }
        cachePool_->relCacheConn(cacheConn);
    } else {
        LOG_ERROR << "no cache connection for group_member";
    }
}

bool GroupModel::insertNewMember(uint32_t groupId, set<uint32_t>& users)
{
    bool ret = false;
    uint32_t userCnt = static_cast<uint32_t>(users.size());
    if (groupId != 0 && userCnt > 0) {
        DBConn* dbConn = dbPool_->getDBConn();
        if (dbConn) {
            uint32_t created = static_cast<uint32_t>(time(NULL));
            // 获取 已经存在群里的用户
            string clause;
            bool first = true;
            for (auto it = users.begin(); it != users.end(); ++it) {
                if (first) {
                    first = false;
                    clause = std::to_string(*it);
                } else {
                    clause += ("," + std::to_string(*it));
                }
            }
            string strSql = "SELECT userId FROM IMGroupMember WHERE groupId=" + std::to_string(groupId) + " AND userId IN (" + clause + ")";
            ResultSet* result = dbConn->executeQuery(strSql);
            set<uint32_t> setHasUser;
            if (result) {
                while (result->next()) {
                    setHasUser.insert(result->getInt("userId"));
                }
                delete result;
            } else {
                LOG_ERROR << "no result for sql:" << strSql;
            }
            dbPool_->relDBConn(dbConn);
            
            dbConn = dbPool_->getDBConn();
            if (dbConn) {
                CacheConn* cacheConn = cachePool_->getCacheConn();
                if (cacheConn) {
                    // 设置已经存在群中人的状态
                    if (!setHasUser.empty()) {
                        clause.clear();
                        first = true;
                        for (auto it=setHasUser.begin(); it!=setHasUser.end(); ++it) {
                            if (first) {
                                first = false;
                                clause = std::to_string(*it);
                            } else {
                                clause += ("," + std::to_string(*it));
                            }
                        }
                        
                        strSql = "update IMGroupMember set status=0, updated=" + 
                            std::to_string(created) + " where groupId=" + 
                            std::to_string(groupId) + " and userId in (" + clause + ")";
                        dbConn->executeUpdate(strSql);
                    }
                    strSql = "INSERT INTO IMGroupMember(`groupId`, `userId`, `status`, `created`, `updated`) values\
                    (?,?,?,?,?)";
                    
                    //插入新成员
                    auto it = users.begin();
                    uint32_t status = 0;
                    uint32_t incMemberCnt = 0;
                    for (;it != users.end();) {
                        uint32_t userId = *it;
                        if (setHasUser.find(userId) == setHasUser.end()) {
                            PrepareStatement* stmt = new PrepareStatement();
                            if (stmt->init(dbConn->getMysql(), strSql)) {
                                uint32_t index = 0;
                                stmt->setParam(index++, groupId);
                                stmt->setParam(index++, userId);
                                stmt->setParam(index++, status);
                                stmt->setParam(index++, created);
                                stmt->setParam(index++, created);
                                stmt->executeUpdate();
                                ++incMemberCnt;
                                delete stmt;
                            } else {
                                users.erase(it++);
                                delete stmt;
                                continue;
                            }
                        }
                        ++it;
                    }
                    if (incMemberCnt != 0) {
                        strSql = "UPDATE IMGroup SET userCnt=userCnt+" + 
                            std::to_string(incMemberCnt) + " where id="+std::to_string(groupId);
                        dbConn->executeUpdate(strSql);
                    }
                    
                    //更新一份到redis中
                    string strKey = "group_member_" + std::to_string(groupId);
                    for (const auto& user : users) {
                        cacheConn->hset(strKey, std::to_string(user), std::to_string(created));
                    }
                    cachePool_->relCacheConn(cacheConn);
                    ret = true;
                } else {
                    LOG_ERROR << "no cache connection";
                }
                dbPool_->relDBConn(dbConn);
            } else {
                LOG_ERROR << "no db connection for teamtalk_master";
            }
        } else {
            LOG_ERROR << "no db connection for teamtalk_slave";
        }
    }
    return ret;
}

void GroupModel::getGroupInfo(map<uint32_t,IM::BaseDefine::GroupVersionInfo>& mapGroupId, list<IM::BaseDefine::GroupInfo>& groupInfos)
{
if (!mapGroupId.empty())
    {
        DBConn* dbConn = dbPool_->getDBConn();
        if (dbConn) {
            string clause;
            bool first = true;
            for (auto it = mapGroupId.begin(); it != mapGroupId.end(); ++it) {
                if (first) {
                    first = false;
                    clause = std::to_string(it->first);
                } else {
                    clause += ("," + std::to_string(it->first));
                }
            }
            string strSql = "SELECT * FROM IMGroup WHERE id IN (" + clause  + ") ORDER BY updated DESC";
            ResultSet* resultSet = dbConn->executeQuery(strSql);
            if (resultSet) {
                while (resultSet->next()) {
                    uint32_t groupId = resultSet->getInt("id");
                    uint32_t version = resultSet->getInt("version");
                    if (mapGroupId[groupId].version() < version) {
                        IM::BaseDefine::GroupInfo cGroupInfo;
                        cGroupInfo.set_group_id(groupId);
                        cGroupInfo.set_version(version);
                        cGroupInfo.set_group_name(resultSet->getString("name"));
                        cGroupInfo.set_group_avatar(resultSet->getString("avatar"));
                        IM::BaseDefine::GroupType groupType = IM::BaseDefine::GroupType(resultSet->getInt("type"));
                        if (IM::BaseDefine::GroupType_IsValid(groupType)) {
                            cGroupInfo.set_group_type(groupType);
                            cGroupInfo.set_group_creator_id(resultSet->getInt("creator"));
                            groupInfos.push_back(cGroupInfo);
                        } else {
                            LOG_ERROR << "invalid groupType. groupId=" << groupId 
                                << ", groupType=" << groupType;
                        }
                    }
                }
                delete resultSet;
            } else {
                LOG_ERROR << "no result set for sql:" << strSql;
            }
            dbPool_->relDBConn(dbConn);
            if (!groupInfos.empty()) {
                for (auto &groupInfo : groupInfos) {
                    list<uint32_t> userIds;
                    uint32_t groupId = groupInfo.group_id();
                    getGroupUser(groupId, userIds);
                    for (auto userId : userIds) {
                        groupInfo.add_group_member_list(userId);
                    }
                }
            }
        } else {
            LOG_ERROR << "no db connection for teamtalk_slave";
        }
    } else {
        LOG_ERROR << "no ids in map";
    }
}