#include "slite/TCPConnection.h"
#include "slite/TCPServer.h"
#include "slite/TCPClient.h"
#include "slite/EventLoop.h"
#include "slite/protobuf/dispatcher.h"

#include "base/messagePtr.h"
#include "base/protobuf_codec.h"
#include "TransferTask.h"

#include <set>
#include <memory>
#include <functional>

namespace IM {

class FileServer
{
public:
    FileServer(std::string host, uint16_t port, slite::EventLoop* loop);
    ~FileServer();

    void start() { server_.start(); }
    std::string host() { return server_.host(); }
    uint16_t port() { return server_.port(); }

    void onTimer();

private:

    struct Context {
        int64_t lastRecvTick;
        int64_t lastSendTick;
        bool auth;
        BaseTransferTask* transferTask;
        uint32_t userId;
    };

    void onConnection(const slite::TCPConnectionPtr& conn);
    void onMessage(const slite::TCPConnectionPtr& conn, std::string& buffer, int64_t receiveTime);
    void onWriteComplete(const slite::TCPConnectionPtr& conn);

    void onUnknownMessage(const slite::TCPConnectionPtr& conn, const MessagePtr& message, int64_t receiveTime);
    void onHeartBeat(const slite::TCPConnectionPtr& conn, const MessagePtr& message, int64_t receiveTime);
    void onFileLoginRequest(const slite::TCPConnectionPtr& conn, const FileLoginReqPtr& message, int64_t receiveTime);
    void onFileState(const slite::TCPConnectionPtr& conn, const FileStatePtr& message, int64_t receiveTime);
    void onFilePullDataRequest(const slite::TCPConnectionPtr& conn, const FilePullDataReqPtr& message, int64_t receiveTime);
    void onFilePullDataResponse(const slite::TCPConnectionPtr& conn, const FilePullDataRspPtr& message, int64_t receiveTime);

    int statesNotify(int state, const std::string& taskId, uint32_t userId, const TCPConnectionPtr& conn);

    slite::TCPServer server_;
    slite::EventLoop *loop_;
    ProtobufDispatcher dispatcher_;
    IM::ProtobufCodec codec_;
    std::set<slite::TCPConnectionPtr> clientConns_;

    static const int kHeartBeatInterVal = 5000;
    static const int kTimeout = 30000;
};

}