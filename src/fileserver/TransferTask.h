#pragma once    

#include "slite/TCPConnection.h"
#include "base/singleton.h"

#include <cstring>
#include <map>


// 状态机
enum TransferTaskState {
    kTransferTaskStateInvalid = 0,              // 非法状态
    
    kTransferTaskStateReady = 1,                // 已经准备好了
    
    kTransferTaskStateWaitingSender = 2,        // 等待发起者
    kTransferTaskStateWaitingReceiver = 3,      // 等待接收者
    kTransferTaskStateWaitingTransfer = 4,      // 准备就绪
    kTransferTaskStateTransfering = 5,          // 正在传输中
    kTransferTaskStateTransferDone = 6,          // 传输完成
    
    kTransferTaskStateWaitingUpload = 7,        // 等待上传
    kTransferTaskStateUploading = 8,            // 正在上传中
    kTransferTaskStateUploadEnd = 9,            // 正在上传中
    
    kTransferTaskStateWaitingDownload = 10,      // 等待下载
    kTransferTaskStateDownloading = 11,          // 正在下载中
    kTransferTaskStateDownloadEnd = 12,          // 下载完成

    kTransferTaskStateError = 13,               // 传输失败
};

struct OfflineFileHeader {
    OfflineFileHeader () {
        taskId_[0] = '\0';
        fromUserId_[0] = '\0';
        toUserId_[0] = '\0';
        createTime_[0] = '\0';
        fileName_[0] = '\0';
        fileSize_[0] = '\0';
        //file_type[0] = '\0';
    }
    
    void setTaskId(std::string& taskId) {
        strncpy(taskId_, taskId.c_str(), 128 < taskId.length() ? 128 : taskId.length());
    }
    
    void setFromUserId(uint32_t id) {
        sprintf(fromUserId_, "%u", id);
    }
    
    void setToUserId(uint32_t id) {
        sprintf(toUserId_, "%u", id);
    }
    
    void setCreateTime(time_t t) {
        sprintf(createTime_, "%ld", t);
    }
    
    void setFileName(const char* p) {
        sprintf(fileName_, p, 512 < strlen(p) ? 512 : strlen(p));
    }
    
    void setFileSize(uint32_t size) {
        sprintf(fileSize_, "%u", size);
    }
    
    std::string getTaskId() const {
        return taskId_;
    }
    
    uint32_t getFromUserId() const {
        return std::stoi(std::string(fromUserId_));
    }

    uint32_t getToUserId() const {
        return std::stoi(std::string(toUserId_));
    }

    uint32_t getCreateTime() const {
        return std::stoi(std::string(createTime_));
    }
    
    std::string getFileName() const {
        return fileName_;
    }

    uint32_t getFileSize() const {
        return std::stoi(std::string(fileSize_));
    }

    char taskId_[128];
    char fromUserId_[64];
    char toUserId_[64];
    char createTime_[128];
    char fileName_[512];
    char fileSize_[64];
};


//----------------------------------------------------------------------------
class BaseTransferTask {
public:
    BaseTransferTask(const std::string& taskId, uint32_t fromUserId, uint32_t toUserId, const std::string& fileName, uint32_t fileSize);
    virtual ~BaseTransferTask() { }
    
    virtual uint32_t getTransMode() const = 0;
    
    const std::string& taskId() const { return taskId_; }
    uint32_t fromUserId() const { return fromUserId_; }
    uint32_t toUserId() const { return  toUserId_; }
    uint32_t fileSize() const { return fileSize_; }
    const std::string& fileName() const { return fileName_; }
    time_t createTime() const { return  createTime_; }
    void setState(int state) { state_ = state; }
    int state() const { return state_; }
    
    uint32_t getOpponent(uint32_t userId) const 
    { return (userId == fromUserId_ ? userId : fromUserId_); }
    
     slite::TCPConnectionPtr getOpponentConn(uint32_t userId) const 
     { return (userId == fromUserId_ ? toConn_ : fromConn_); }

     slite::TCPConnectionPtr getFromConn() 
     { return fromConn_; }
     
     slite::TCPConnectionPtr getToConn() 
     { return toConn_; }
     
     slite::TCPConnectionPtr getConnByUserID(uint32_t userId) const {
        if (fromUserId_ == userId) {
            return fromConn_;
        } else if (toUserId_ == userId) {
            return toConn_;
        } else {
            return NULL;
        }
     }
    
     void setConnByUserID(uint32_t userId, const slite::TCPConnectionPtr& conn) {
        if (fromUserId_ == userId) {
            fromConn_ = conn;
        } else if (toUserId_ == userId) {
            toConn_ = conn;
        }
     }
    
    bool checkFromUserID(uint32_t userId) const 
    { return fromUserId_ == userId; }

    bool checkToUserID(uint32_t userId) const 
    { return toUserId_ == userId; }

    bool checkUserID(uint32_t userId) const 
    { return userId == fromUserId_ || userId == toUserId_; }
    
    bool isWaitTranfering() const {
        bool rv = false;
        if (state_ == kTransferTaskStateWaitingTransfer || state_ == kTransferTaskStateWaitingUpload || kTransferTaskStateWaitingDownload) {
            rv = true;
        }
        return rv;
    }
    
    void setLastUpdateTime();
    
    // 检查状态
    virtual bool changePullState(uint32_t userId, int fileRole) { return false; }
    
    // 检查输入是否合法
    virtual bool checkByUserIDAndFileRole(uint32_t userId, int fileRole) const { return false; }
    virtual int doRecvData(uint32_t userId, uint32_t offset, const char* data, uint32_t dataSize) { return false; }
    virtual int doPullFileRequest(uint32_t userId, uint32_t offset, uint32_t dataSize, std::string* data) { return false; }

protected:
    std::string taskId_; // uuid_unparse char[37]
    uint32_t fromUserId_;
    uint32_t toUserId_; // if offline or mobile, null
    std::string fileName_;
    uint32_t fileSize_;
    time_t createTime_;

    int state_;         // 传输状态

    slite::TCPConnectionPtr fromConn_;
    slite::TCPConnectionPtr toConn_;
    
    // uint64_t    last_update_time_;
};

typedef std::map<std::string, BaseTransferTask*> TransferTaskMap;
typedef std::map<slite::TCPConnectionPtr, BaseTransferTask*> TransferTaskConnkMap;

//----------------------------------------------------------------------------
class OnlineTransferTask : public BaseTransferTask {
public:
    OnlineTransferTask(const std::string& taskId, uint32_t fromUserId, uint32_t toUserId, const std::string& fileName, uint32_t fileSize)
        : BaseTransferTask(taskId, fromUserId, toUserId, fileName, fileSize),
        macSeqNum_(0) 
    {
    }
    
    virtual ~OnlineTransferTask() { }
    
    virtual uint32_t getTransMode() const;
    
    virtual bool changePullState(uint32_t userId, int fileRole);
    virtual bool checkByUserIDAndFileRole(uint32_t userId, int fileRole) const;
    
    virtual int doRecvData(uint32_t userId, uint32_t offset, const char* data, uint32_t dataSize);
    virtual int doPullFileRequest(uint32_t userId, uint32_t offset, uint32_t dataSize, std::string* data);
    
    void setSeqNum(uint32_t seqNum) 
    { macSeqNum_ = seqNum; }
    
    uint32_t getSeqNum() const 
    { return macSeqNum_; }
private:
    // mac客户端需要保证seqNum，但客户端目前机制无法处理在线文件传输的seq_num，故服务端纪录并设置seq_num
    uint32_t macSeqNum_;
};

//----------------------------------------------------------------------------
#define SEGMENT_SIZE 32768

class OfflineTransferTask : public BaseTransferTask {
public:
    OfflineTransferTask(const std::string& taskId, uint32_t fromUserId, uint32_t toUserId, const std::string& fileName, uint32_t fileSize)
        : BaseTransferTask(taskId, fromUserId, toUserId, fileName, fileSize),
        fp_(nullptr),
        transferedIdx_(0) 
    {
        sengmentSize_ = setMaxSegmentSize(fileSize);
    }

    virtual ~OfflineTransferTask() {
        if (fp_) {
            fclose(fp_);
            fp_ = nullptr;
        }
    }
    
    static OfflineTransferTask* loadFromDisk(const std::string& taskId, uint32_t userId);
    
    
    virtual uint32_t getTransMode() const;
    
    virtual bool changePullState(uint32_t userId, int file_role);
    virtual bool checkByUserIDAndFileRole(uint32_t userId, int file_role) const;
    
    virtual int doRecvData(uint32_t userId, uint32_t offset, const char* data, uint32_t data_size);
    virtual int doPullFileRequest(uint32_t userId, uint32_t offset, uint32_t data_size, std::string* data);
   
    int getSegmentSize() const 
    { return sengmentSize_; }
    
    int getNextSegmentBlockSize() {
        int blockSize = SEGMENT_SIZE;
        if (transferedIdx_ + 1 == sengmentSize_) {
            blockSize = fileSize_ - transferedIdx_ * SEGMENT_SIZE;
        }
        return blockSize;
    }

    uint32_t getNextOffset() const {
        return SEGMENT_SIZE * transferedIdx_;
    }
    
private:
    // 迭代器
    inline int setMaxSegmentSize(uint32_t file_size) {
        int seg_size = file_size / SEGMENT_SIZE;
        if (fileSize_%SEGMENT_SIZE != 0) {
            seg_size = file_size / SEGMENT_SIZE + 1;
        }
        return seg_size;
    }

    FILE* fp_;
    
    // 已经传输
    int transferedIdx_;
    int sengmentSize_;
};

std::string generateUUID();
const char* getCurrentOfflinePath();

//----------------------------------------------------------------------------
class TransferTaskManager : public Singleton<TransferTaskManager> {
public:
    ~TransferTaskManager();
    
    void onTimer(uint64_t tick);
    
    BaseTransferTask* newTransferTask(uint32_t transMode, const std::string& taskId, uint32_t fromUserId, uint32_t toUserId, const std::string& fileName, uint32_t file_size);

    OfflineTransferTask* newTransferTask(const std::string& taskId, uint32_t toUserId);

    bool deleteTransferTask(const std::string& taskId);
    bool deleteTransferTaskByConnClose(const std::string& taskId);
    
    BaseTransferTask* findByTaskID(const std::string& taskId) {
        BaseTransferTask* transferTask = NULL;
        
        TransferTaskMap::iterator it = transferTasks_.find(taskId);
        if (it != transferTasks_.end()) {
            transferTask = it->second;
        }
        
        return transferTask;
    }
    
private:
    friend class Singleton<TransferTaskManager>;
    
    TransferTaskManager();
    
    TransferTaskMap transferTasks_;
    // TransferTaskConnkMap conn_tasks_;
};