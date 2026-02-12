#ifndef AUTH_AGENT_H
#define AUTH_AGENT_H

#include "auth.pb.h"
#include "local_cache.h"
#include <brpc/channel.h>
#include <brpc/server.h>
#include <memory>

// Agent 业务逻辑实现类
class AuthAgentImpl : public siqi::auth::AuthService {
private:
    std::unique_ptr<siqi::auth::AuthService_Stub> stub_;
    std::shared_ptr<LocalCache<bool>> cache_; // 缓存: Key -> Allowed(bool)
    int cache_ttl_;
    int timeout_ms_;

public:
    // 构造函数
    // channel: 已初始化的与 Auth Server 的连接通道
    // cache_ttl: 本地缓存时间
    // timeout_ms: RPC 调用超时时间
    AuthAgentImpl(brpc::Channel* channel, int cache_ttl = 60, int timeout_ms = 500);
    
    // 实现 AuthService 的 Check 接口
    void Check(google::protobuf::RpcController* cntl_base,
               const siqi::auth::CheckRequest* request,
               siqi::auth::CheckResponse* response,
               google::protobuf::Closure* done) override;
};

#endif // AUTH_AGENT_H
