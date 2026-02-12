#ifndef AUTH_SERVICE_IMPL_H
#define AUTH_SERVICE_IMPL_H

#include "permission_dao.h"
#include "auth.pb.h"
#include "local_cache.h"
#include <brpc/server.h>
#include <butil/logging.h>
#include <unordered_set>

class AuthServiceImpl : public siqi::auth::AuthService {
private:
    PermissionDAO dao_;
    // 缓存用户的所有权限Key (Set 用于快速查找)
    // Key: "app_code:user_id"
    // Value: Set of permission keys
    std::shared_ptr<LocalCache<std::unordered_set<std::string>>> cache_;
    int cache_ttl_;
    
public:
    // 构造函数
    AuthServiceImpl(std::shared_ptr<LocalCache<std::unordered_set<std::string>>> cache,
                    const std::string& host,
                    int port,
                    const std::string& user,
                    const std::string& password,
                    const std::string& database,
                    int cache_ttl);
    
    // 权限检查接口
    void Check(google::protobuf::RpcController* cntl,
               const siqi::auth::CheckRequest* request,
               siqi::auth::CheckResponse* response,
               google::protobuf::Closure* done) override;
    
    // 批量权限检查接口
    void BatchCheck(google::protobuf::RpcController* cntl,
                    const siqi::auth::BatchCheckRequest* request,
                    siqi::auth::BatchCheckResponse* response,
                    google::protobuf::Closure* done) override;
    
    // 获取服务状态（可选）
    bool isReady() const;
};

#endif // AUTH_SERVICE_IMPL_H