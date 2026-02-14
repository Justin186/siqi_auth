#ifndef AUTH_AGENT_H
#define AUTH_AGENT_H

#include "auth.pb.h"
#include "permission_dao.h" 
#include <brpc/server.h>
#include <memory>

// Agent 业务逻辑实现类
class AuthAgentImpl : public siqi::auth::AuthService {
private:
    // 不再持有 Channel，而是持有 DAO 指针
    PermissionDAO* dao_;

public:
    // 构造函数
    // dao: 已初始化的数据库访问对象
    AuthAgentImpl(PermissionDAO* dao);
    
    // 实现 AuthService 的 Check 接口
    void Check(google::protobuf::RpcController* cntl_base,
               const siqi::auth::CheckRequest* request,
               siqi::auth::CheckResponse* response,
               google::protobuf::Closure* done) override;
};

#endif // AUTH_AGENT_H
