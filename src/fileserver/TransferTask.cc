//
//  transferTask.cpp
//  im-server-mac-new
//
//  Created by wubenqi on 15/7/16.
//  Copyright (c) 2015年 benqi. All rights reserved.
//

#include "TransferTask.h"

#include "slite/Logger.h"
#include "pbs/IM.BaseDefine.pb.h"

#include <uuid/uuid.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

using namespace slite;

std::string generateUUID() {
    std::string rv;
    
    uuid_t uid = {0};
    uuid_generate(uid);
    if (!uuid_is_null(uid)) {
        char str_uuid[64] = {0};
        uuid_unparse(uid, str_uuid);
        rv = str_uuid;
    }
    
    return rv;
}

const char* getCurrentOfflinePath() {
    static const char* g_currentSavePath = nullptr;
    if (g_currentSavePath == nullptr) {
        static char s_tmp[BUFSIZ];
        char workPath[BUFSIZ];
        if (!getcwd(workPath, BUFSIZ)) {
            LOG_ERROR << "getcwd " << workPath << " failed";
        } else {
            snprintf(s_tmp, BUFSIZ, "%s/offline_file", workPath);
        }
        
        LOG_INFO << "save offline files to " << s_tmp;
        
        int ret = mkdir(s_tmp, 0755);
        if ((ret != 0) && (errno != EEXIST)) {
            LOG_ERROR << "mkdir " << s_tmp << " failed to save offline files";
        }
        
        g_currentSavePath = s_tmp;
    }
    return g_currentSavePath;
}

static FILE* openByRead(const std::string& taskId, uint32_t userId) {
    FILE* fp = nullptr;
    if (taskId.length() >= 2) {
        char savePath[BUFSIZ];
        snprintf(savePath, BUFSIZ, "%s/%s/%s", getCurrentOfflinePath(), taskId.substr(0, 2).c_str() , taskId.c_str());
        fp = fopen(savePath, "rb");  // save fp
        if (!fp) {
            LOG_ERROR << "Open file " << savePath << " for read failed";
        }
    }
    return fp;
}

static FILE* openByWrite(const std::string& taskId, uint32_t userId) {
    FILE* fp = nullptr;
    if (taskId.length() >= 2) {
        char savePath[BUFSIZ];
        snprintf(savePath, BUFSIZ, "%s/%s", getCurrentOfflinePath(), taskId.substr(0, 2).c_str());
        int ret = mkdir(savePath, 0755);
        if ( (ret != 0) && (errno != EEXIST) ) {
            LOG_ERROR << "mkdir failed for path: " << savePath;
        } else {
            // save as g_current_save_path/to_id_url/taskId
            strncat(savePath, "/", BUFSIZ);
            strncat(savePath, taskId.c_str(), BUFSIZ);
            
            fp = fopen(savePath, "ab+");
            if (!fp) {
                LOG_ERROR << "Open file for write failed";
            }
        }
    }
    
    return fp;
}


//----------------------------------------------------------------------------
BaseTransferTask::BaseTransferTask(const std::string& taskId, uint32_t fromUserId, uint32_t toUserId, const std::string& fileName, uint32_t fileSize)
    : taskId_(taskId),
      fromUserId_(fromUserId),
      toUserId_(toUserId),
      fileName_(fileName),
      fileSize_(fileSize),
      state_(kTransferTaskStateReady),
      createTime_(time(NULL)),
      fromConn_(nullptr),
      toConn_(nullptr)
{             
}

void BaseTransferTask::setLastUpdateTime() 
{
    createTime_ = time(NULL);
}

//----------------------------------------------------------------------------
uint32_t OnlineTransferTask::getTransMode() const 
{
    return IM::BaseDefine::FILE_TYPE_ONLINE;
}

bool OnlineTransferTask::changePullState(uint32_t userId, int fileRole) {
    // 在线文件传输，初始状态：kTransferTaskStateReady
    //  状态转换流程 kTransferTaskStateReady
    //        --> kTransferTaskStateWaitingSender或kTransferTaskStateWaitingReceiver
    //        --> kTransferTaskStateWaitingTransfer
    //
    bool rv = checkByUserIDAndFileRole(userId, fileRole);
    if (!rv) {
        LOG_ERROR << "Check error! userId=" << userId << ", fileRole=" << fileRole;
        return false;
    }
    
    if (state_ != kTransferTaskStateReady && state_ != kTransferTaskStateWaitingSender && state_ != kTransferTaskStateWaitingReceiver) {
        LOG_ERROR << "Invalid state, valid state is kTransferTaskStateReady \
            or kTransferTaskStateWaitingSender or kTransferTaskStateWaitingReceiver, but state is " << state_;
        return false;
    }
    
    if (state_ == kTransferTaskStateReady) {
        // 第一个用户进来
        // 如果是sender，则-->kTransferTaskStateWaitingReceiver
        // 如果是receiver，则-->kTransferTaskStateWaitingSender
        if (fileRole == IM::BaseDefine::ClientFileRole::CLIENT_REALTIME_SENDER) {
            state_ = kTransferTaskStateWaitingReceiver;
        } else {
            state_ = kTransferTaskStateWaitingSender;
        }
        // 第二个用户进来
    } else {
        if (state_ == kTransferTaskStateWaitingReceiver) {
            // 此时必须是receiver
            // 需要检查是否是receiver
            if (fileRole != IM::BaseDefine::ClientFileRole::CLIENT_REALTIME_RECVER) {
                LOG_ERROR << "Invalid user, userId=" << userId << ", but toUserId_" << toUserId_;
                return false;
            }
        } else if (state_ == kTransferTaskStateWaitingSender) {
            // 此时必须是sender
            // 需要检查是否是sender
            if (fileRole != IM::BaseDefine::ClientFileRole::CLIENT_REALTIME_SENDER) {
                LOG_ERROR << "Invalid user, userId=" << userId << ", but toUserId_" << toUserId_;
                return false;
            }
        }
        
        state_ = kTransferTaskStateWaitingTransfer;
        
    }
    setLastUpdateTime();

    return true;
}

bool OnlineTransferTask::checkByUserIDAndFileRole(uint32_t userId, int fileRole) const 
{
    // 在线文件传输
    // 1. fileRole必须是CLIENT_REALTIME_SENDER或CLIENT_REALTIME_RECEIVER
    // 2. CLIENT_REALTIME_SENDER则userId==fromUserId_
    // 3. CLIENT_REALTIME_RECVER则userId==toUserId_
    
    if (fileRole == IM::BaseDefine::ClientFileRole::CLIENT_REALTIME_SENDER){
        if (!checkFromUserID(userId)) {
            return false;
        }
    } else if (fileRole == IM::BaseDefine::ClientFileRole::CLIENT_REALTIME_RECVER) {
        if (!checkToUserID(userId)) {
            return false;
        }
    }
    
    return true;
}

int OnlineTransferTask::doRecvData(uint32_t userId, uint32_t offset, const char* data, uint32_t dataSize) 
{
    // 检查是否发送者
    if (!checkFromUserID(userId)) {
        LOG_ERROR << "Check error! userId=" << userId << ", fromUserId=" << fromUserId_ << ", toUserId=" << toUserId_;
        return -1;
    }

    // 检查状态
    if (state_ != kTransferTaskStateWaitingTransfer && state_ != kTransferTaskStateTransfering) {
        LOG_ERROR << "check state_ error, userId=" << userId 
            << ", state=" << state_ 
            << ", but state need kTransferTaskStateWaitingTransfer or kTransferTaskStateTransfering";
        return -1;
    }

    // TODO: 检查文件大小
    if (state_ == kTransferTaskStateWaitingTransfer) {
        state_ = kTransferTaskStateTransfering;
    }
    
    setLastUpdateTime();
    return 0;
}

int OnlineTransferTask::doPullFileRequest(uint32_t userId, uint32_t offset, uint32_t dataSize, std::string* data) 
{
    // 在线
    // 1. 检查状态
    if (state_ != kTransferTaskStateWaitingTransfer && state_ != kTransferTaskStateTransfering) {
    LOG_ERROR << "check state_ error, userId=" << userId 
        << ", state=" << state_ 
        << ", but state need kTransferTaskStateWaitingTransfer or kTransferTaskStateTransfering";
        return -1;
    }
    
    if (state_ == kTransferTaskStateWaitingTransfer) {
        state_ = kTransferTaskStateTransfering;
    }
    setLastUpdateTime();
    return 0;
}

//----------------------------------------------------------------------------
OfflineTransferTask* OfflineTransferTask::loadFromDisk(const std::string& taskId, uint32_t userId) 
{
    OfflineTransferTask* offline = nullptr;
    
    FILE* fp = openByRead(taskId, userId);
    if (fp) {
        OfflineFileHeader fileHeader;
        size_t size = fread(&fileHeader, 1, sizeof(fileHeader), fp);
        if (size == sizeof(fileHeader)) {
            fseek(fp, 0L, SEEK_END);
            size_t fileSize = static_cast<size_t>(ftell(fp))-size;
            if (fileSize == fileHeader.getFileSize()) {
                offline = new OfflineTransferTask(fileHeader.getTaskId(),
                                                  fileHeader.getFromUserId(),
                                                  fileHeader.getToUserId(),
                                                  fileHeader.getFileName(),
                                                  fileHeader.getFileSize());
                if (offline) {
                    offline->setState(kTransferTaskStateWaitingDownload);
                }
            } else {
                LOG_ERROR << "Offile file size by taskId=" << taskId 
                    << ", userId=" << userId << ", header_file_size=" << 
                    fileHeader.getFileSize() << ", disk_file_size=" << fileSize;
            }
        } else {
            LOG_ERROR << "Read fileHeader error by taskId=" << taskId << ", userId=" << userId;
        }
        fclose(fp);
    }
    
    return offline;
}

uint32_t OfflineTransferTask::getTransMode() const 
{
    return IM::BaseDefine::FILE_TYPE_OFFLINE;
}

bool OfflineTransferTask::changePullState(uint32_t userId, int fileRole) 
{
    // 离线文件传输
    // 1. 如果是发送者，状态转换 kTransferTaskStateReady－->kTransferTaskStateWaitingUpload
    // 2. 如果是接收者，状态转换 kTransferTaskStateUploadEnd --> kTransferTaskStateWaitingDownload

//    if (CheckFromUserID(userId)) {
//        // 如果是发送者
//        // 当前状态必须为kTransferTaskStateReady
//        if () {
//
//        }
//    } else {
//        // 如果是接收者
//    }
    bool rv = checkByUserIDAndFileRole(userId, fileRole);
    if (!rv) {
        LOG_ERROR << "check error! userId=" << userId << ", fileRole=" << fileRole;
        return false;
    }
    
    if (state_ != kTransferTaskStateReady &&
            state_ != kTransferTaskStateUploadEnd &&
            state_ != kTransferTaskStateWaitingDownload) {
        LOG_ERROR << "Invalid state, valid state is kTransferTaskStateReady or kTransferTaskStateUploadEnd, but state is " << state_;
        return false;
    }
    
    if (state_ == kTransferTaskStateReady) {
        // 第一个用户进来，必须是CLIENT_OFFLINE_UPLOAD
        // 必须是kTransferTaskStateReady，则-->kTransferTaskStateWaitingUpload
        if (IM::BaseDefine::ClientFileRole::CLIENT_OFFLINE_UPLOAD == fileRole) {
            state_ = kTransferTaskStateWaitingUpload;
        } else {
            LOG_ERROR << "Offline upload: fileRole is CLIENT_OFFLINE_UPLOAD but fileRole=" << fileRole;
            return false;
        }
    } else {
        if (fileRole == IM::BaseDefine::ClientFileRole::CLIENT_OFFLINE_DOWNLOAD) {
            state_ = kTransferTaskStateWaitingDownload;
        } else {
            LOG_ERROR << "Offline upload: fileRole is CLIENT_OFFLINE_DOWNLOAD but fileRole=" << fileRole;
            return false;
        }
    }
    
    setLastUpdateTime();
    return true;
}

bool OfflineTransferTask::checkByUserIDAndFileRole(uint32_t userId, int fileRole) const 
{
    // 离线文件传输
    // 1. fileRole必须是CLIENT_OFFLINE_UPLOAD或CLIENT_OFFLINE_DOWNLOAD
    // 2. CLIENT_OFFLINE_UPLOAD则userId==fromUserId_
    // 3. CLIENT_OFFLINE_DOWNLOAD则userId==toUserId_
    if (fileRole == IM::BaseDefine::ClientFileRole::CLIENT_OFFLINE_UPLOAD){
        if (checkFromUserID(userId)) {
            return true;
        }
    } else if (fileRole == IM::BaseDefine::ClientFileRole::CLIENT_OFFLINE_DOWNLOAD) {
        if (checkToUserID(userId)) {
            return true;
        }
    }
    
    return false;
}

int OfflineTransferTask::doRecvData(uint32_t userId, uint32_t offset, const char* data, uint32_t dataSize) 
{
    // 离线文件上传
    
    // 检查是否发送者
    if (!checkFromUserID(userId)) {
        LOG_ERROR << "rsp userId=" << userId << ", but senderId is " << fromUserId_;
        return -1;
    }
    
    // 检查状态
    if (state_ != kTransferTaskStateWaitingUpload && state_ != kTransferTaskStateUploading) {
        LOG_ERROR << "state=" << state_ << " error, need kTransferTaskStateWaitingUpload or kTransferTaskStateUploading";
        return -1;
    }
    
    // 检查offset是否有效
    if (offset != transferedIdx_ * SEGMENT_SIZE) {
        LOG_ERROR << "offset error, " << "should be" 
            << transferedIdx_ * SEGMENT_SIZE << ", but offset is" << offset;
        return -1;
    }

    // todo
    // 检查文件大小    
    dataSize = getNextSegmentBlockSize();
    LOG_INFO << "doRecvData, offset=" << offset << ", dataSize=" << dataSize << ", segment_size=" << sengmentSize_;
    
    if (state_ == kTransferTaskStateWaitingUpload) {
        if (fp_ == NULL) {
            fp_ = openByWrite(taskId_, toUserId_);
            if (fp_ == NULL) {
                return -1;
            }
        }

        // 写文件头
        OfflineFileHeader fileHeader;
        memset(&fileHeader, 0, sizeof(fileHeader));
        fileHeader.setCreateTime(time(NULL));
        fileHeader.setTaskId(taskId_);
        fileHeader.setFromUserId(fromUserId_);
        fileHeader.setToUserId(toUserId_);
        fileHeader.setFileName("");
        fileHeader.setFileSize(fileSize_);
        fwrite(&fileHeader, 1, sizeof(fileHeader), fp_);
        fflush(fp_);

        state_ = kTransferTaskStateUploading;
    }
    
    // 存储
    if (fp_ == nullptr) {
        return -1;
    }
    
    fwrite(data, 1, dataSize, fp_);
    fflush(fp_);

    ++transferedIdx_;
    setLastUpdateTime();

    if (transferedIdx_ == sengmentSize_) {
        state_ = kTransferTaskStateUploadEnd;
        fclose(fp_);
        fp_ = nullptr;
    } else {
        return 1;
    }
    return 0;
}

int OfflineTransferTask::doPullFileRequest(uint32_t userId, uint32_t offset, uint32_t dataSize, std::string* data) 
{
    LOG_ERROR << "Recv pull file request: userId=" << userId << ", offset=" << offset << ", dataSize=" << dataSize;
    // 1. 首先检查状态，必须为kTransferTaskStateWaitingDownload或kTransferTaskStateDownloading
    if (state_ != kTransferTaskStateWaitingDownload && state_ != kTransferTaskStateDownloading) {
        LOG_ERROR << "state=" << state_ 
            << " error, need kTransferTaskStateWaitingDownload or kTransferTaskStateDownloading";
        return -1;
    }
    
    // 2. 处理kTransferTaskStateWaitingDownload
    if(state_ == kTransferTaskStateWaitingDownload) {
        if (transferedIdx_ != 0)
            transferedIdx_ = 0;
        
        if (fp_ != nullptr) {
            fclose(fp_);
            fp_ = nullptr;
        }

        fp_ = openByRead(taskId_, userId);
        if (fp_ == nullptr) {
            return -1;
        }

        OfflineFileHeader fileHeader;
        size_t size = fread(&fileHeader, 1, sizeof(fileHeader), fp_); // read header
        if (sizeof(fileHeader) != size) {
            // close to ensure next time will read again
            LOG_ERROR << "read file head failed";
            fclose(fp_); // error to get header
            fp_ = NULL;
            return -1;
            
        }

        state_ = kTransferTaskStateDownloading;
    } else {
        // 检查文件是否打开
        if (fp_ == NULL) {
            // 不可能发生
            return -1;
        }
    }
    
    // 检查offset是否有效
    if (offset != transferedIdx_ * SEGMENT_SIZE) {
        LOG_ERROR << "Recv offset error, offser=" << offset << ", transfered offset=" << transferedIdx_ * SEGMENT_SIZE;
        return -1;
    }
    
    dataSize = getNextSegmentBlockSize();
    
    LOG_INFO << "Ready send data, offset=" << offset << ", dataSize=%d" << dataSize;
    
    // the header won't be sent to recver, because the msg svr had already notified it.
    // if the recver needs to check it, it could be helpful
    // or sometime later, the recver needs it in some way.
    
        
    // read data and send based on offset and datasize.
    char* tmpbuf = new char[dataSize];
    if (nullptr == tmpbuf) {
        // alloc mem failed
        LOG_ERROR << "alloc mem failed";
        return -1;
    }
    memset(tmpbuf, 0, dataSize);
    
    size_t size = fread(tmpbuf, 1, dataSize, fp_);
    if (size != dataSize) {
        LOG_ERROR << "Read size error, dataSize=" << dataSize << ", but read_size=" << size;
        delete [] tmpbuf;
        return -1;
    }
    
    data->append(tmpbuf, dataSize);
    delete [] tmpbuf;

    transferedIdx_++;
    
    setLastUpdateTime();
    if (transferedIdx_ == sengmentSize_) {
        LOG_INFO << "pull req end";
        state_ = kTransferTaskStateUploadEnd;
        fclose(fp_);
        fp_ = NULL;
    } else {
        return 1;
    }

    return 0;
}


TransferTaskManager::TransferTaskManager() 
{
}

void TransferTaskManager::onTimer(uint64_t tick) {
    for (TransferTaskMap::iterator it = transferTasks_.begin(); it != transferTasks_.end();) 
    {
        BaseTransferTask* task = it->second;
        if (task == nullptr) {
            transferTasks_.erase(it++);
            continue;
        }
        
        if (task->state() != kTransferTaskStateWaitingUpload &&
            task->state() == kTransferTaskStateTransferDone) {
            long esp = time(NULL) - task->create_time();
            // ConfigUtil::getInstance()->GetTaskTimeout()
            if (esp > 3000) {
                if (task->getFromConn()) {
                    TCPConnectionPtr conn = task->getFromConn();
                    conn->setContext(nullptr);
                }
                if (task->getToConn()) {
                    TCPConnectionPtr conn = task->getToConn();
                    conn->setContext(nullptr);
                }
                delete task;
                transferTasks_.erase(it++);
                continue;
            }
        }
        
        ++it;
    }
}

BaseTransferTask* TransferTaskManager::newTransferTask(uint32_t transMode, const std::string& taskId, uint32_t from_user_id, uint32_t to_user_id, const std::string& file_name, uint32_t file_size) 
{
    BaseTransferTask* transferTask = nullptr;
    
    TransferTaskMap::iterator it = transferTasks_.find(taskId);
    if (it == transferTasks_.end()) {
        if (transMode == IM::BaseDefine::FILE_TYPE_ONLINE) {
            transferTask = new OnlineTransferTask(taskId, from_user_id, to_user_id, file_name, file_size);
        } else if (transMode == IM::BaseDefine::FILE_TYPE_OFFLINE) {
            transferTask = new OfflineTransferTask(taskId, from_user_id, to_user_id, file_name, file_size);
        } else {
            LOG_ERROR << "Invalid transMode=" << transMode;
        }
        
        if (transferTask) {
            transferTasks_.insert(std::make_pair(taskId, transferTask));
        }
    } else {
        LOG_ERROR << "Task existed by taskId=" << taskId << ", why?????";
    }
    
    return transferTask;
}

OfflineTransferTask* TransferTaskManager::newTransferTask(const std::string& taskId, uint32_t toUserId) 
{
    OfflineTransferTask* transferTask = OfflineTransferTask::loadFromDisk(taskId, toUserId);
    if (transferTask) {
        transferTasks_.insert(std::make_pair(taskId, transferTask));
    }
    return transferTask;
}

bool TransferTaskManager::deleteTransferTaskByConnClose(const std::string& taskId) 
{
    TransferTaskMap::iterator it = transferTasks_.find(taskId);
    if (it!=transferTasks_.end()) {
        BaseTransferTask* transferTask = it->second;
        if (transferTask->getTransMode() == IM::BaseDefine::FILE_TYPE_ONLINE) {
            if (transferTask->getFromConn() == NULL && transferTask->getToConn() == NULL) {
                delete transferTask;
                transferTasks_.erase(it);
                return true;
            }
        } else {
            if (transferTask->state() != kTransferTaskStateWaitingUpload) {
                delete transferTask;
                transferTasks_.erase(it);
                return true;
            }
        }
    }
    
    return false;
}

bool TransferTaskManager::deleteTransferTask(const std::string& taskId) 
{
    TransferTaskMap::iterator it = transferTasks_.find(taskId);
    if (it != transferTasks_.end()) {
        delete it->second;
        transferTasks_.erase(it);
        return true;
    }
    
    return false;
}
