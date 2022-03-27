#include "slite/TCPConnection.h"
#include "slite/TCPServer.h"
#include "slite/TCPClient.h"
#include "slite/EventLoop.h"
#include "slite/protobuf/dispatcher.h"
#include "slite/protobuf/codec.h"

#include "base/messagePtr.h"
#include "TransferTask.h"

#include <set>
#include <memory>
#include <functional>

namespace IM {

class FileServer2
{
public:
    FileServer2(std::string host, uint16_t port, slite::EventLoop* loop);
    ~FileServer2();

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

    void onFileTransferRequest(const slite::TCPConnectionPtr& conn, const FileTransferReqPtr& message, int64_t receiveTime);
    void onGetServerAddressRequest(const slite::TCPConnectionPtr& conn, const FileServerIPReqPtr& message, int64_t receiveTime);

    slite::TCPServer server_;
    slite::EventLoop *loop_;
    ProtobufDispatcher dispatcher_;
    slite::ProtobufCodec codec_;
    std::set<slite::TCPConnectionPtr> clientConns_;

    static const int kHeartBeatInterVal = 5000;
    static const int kTimeout = 30000;
};

}