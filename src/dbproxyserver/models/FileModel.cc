#include "FileModel.h"

#include "slite/Logger.h"

using namespace slite;

FileModel::FileModel(DBPoolPtr dbPool, CachePoolPtr cachePool)
    : dbPool_(dbPool),
    cachePool_(cachePool)
{
}

FileModel::~FileModel()
{
}

void FileModel::getOfflineFile(uint32_t userId, list<IM::BaseDefine::OfflineFileInfo>& offlines)
{
    DBConn* dbConn = dbPool_->getDBConn();
    if (dbConn) {
        string strSql = "select * from IMTransmitFile where to_id="+std::to_string(userId) + " and status=0 order by created";
        ResultSet* resultSet = dbConn->executeQuery(strSql);
        if (resultSet) {
            while (resultSet->next()) {
                IM::BaseDefine::OfflineFileInfo offlineFile;
                offlineFile.set_from_user_id(resultSet->getInt("from_id"));
                offlineFile.set_task_id(resultSet->getString("task_id"));
                offlineFile.set_file_name(resultSet->getString("filename"));
                offlineFile.set_file_size(resultSet->getInt("size"));
                offlines.push_back(offlineFile);
            }
            delete resultSet;
        } else {
            LOG_ERROR << "no result for:" << strSql;
        }
        dbPool_->relDBConn(dbConn);
    } else {
        LOG_ERROR << "no db connection for teamtalk_slave";
    }
}

void FileModel::addOfflineFile(uint32_t fromId, uint32_t toId, string& taskId, string& fileName, uint32_t fileSize)
{
    DBConn* dbConn = dbPool_->getDBConn();
    if (dbConn) {
        string strSql = "insert into IMTransmitFile (`from_id`,`to_id`,`filename`,`size`,`task_id`,`status`,`created`,`updated`) values(?,?,?,?,?,?,?,?)";
        
        // 必须在释放连接前delete CPrepareStatement对象，否则有可能多个线程操作mysql对象，会crash
        PrepareStatement* stmt = new PrepareStatement();
        if (stmt->init(dbConn->getMysql(), strSql)) {
            uint32_t status = 0;
            uint32_t created = static_cast<uint32_t>(time(NULL));
            
            uint32_t index = 0;
            stmt->setParam(index++, fromId);
            stmt->setParam(index++, toId);
            stmt->setParam(index++, fileName);
            stmt->setParam(index++, fileSize);
            stmt->setParam(index++, taskId);
            stmt->setParam(index++, status);
            stmt->setParam(index++, created);
            stmt->setParam(index++, created);
            
            bool ret = stmt->executeUpdate();
            
            if (!ret) {
                LOG_ERROR << "insert message failed: " << strSql;
            }
        }
        delete stmt;
        dbPool_->relDBConn(dbConn);
    } else {
        LOG_ERROR << "no db connection for teamtalk_master";
    }
}