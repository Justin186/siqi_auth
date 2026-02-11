#include "auth_service_impl.h"
#include <brpc/controller.h>
#include <errno.h>

AuthServiceImpl::AuthServiceImpl(const std::string& host,
                                 const std::string& user,
                                 const std::string& password,
                                 const std::string& database)
    : dao_(host, user, password, database) {
    
    if (!dao_.isConnected()) {
        LOG(ERROR) << "数据库连接失败，服务启动可能受影响";
    } else {
        LOG(INFO) << "数据库连接成功";
    }
}

void AuthServiceImpl::Check(google::protobuf::RpcController* cntl,
                           const siqi::auth::CheckRequest* request,
                           siqi::auth::CheckResponse* response,
                           google::protobuf::Closure* done) {
    
    // 确保done会被调用（RAII方式）
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* bcntl = static_cast<brpc::Controller*>(cntl);
    
    // 1. 参数验证
    if (request->app_code().empty() || 
        request->user_id().empty() || 
        request->perm_key().empty()) {
        
        response->set_allowed(false);
        response->set_reason("参数不完整");
        bcntl->SetFailed(EINVAL, "参数不完整");
        return;
    }
    
    // 2. 执行权限检查
    bool allowed = false;
    LOG(INFO) << "Querying DB...";
    try {
        allowed = dao_.checkPermission(
            request->app_code(),
            request->user_id(),
            request->perm_key(),
            request->resource_id()
        );
    } catch (const std::exception& e) {
        LOG(ERROR) << "权限检查异常: " << e.what();
        response->set_allowed(false);
        response->set_reason("系统内部错误");
        bcntl->SetFailed(brpc::EINTERNAL, "系统内部错误");
        return;
    }
    
    // 3. 设置响应
    response->set_allowed(allowed);
    if (!allowed) {
        response->set_reason("用户没有该权限");
        
        // 可以添加建议角色（高级功能）
        auto roles = dao_.getUserRoles(request->app_code(), request->user_id());
        if (roles.empty()) {
            response->set_suggest_role("user");  // 默认角色
        }
    }
    
    // 4. 记录访问日志（生产环境需要）
    LOG(INFO) << "[AuthCheck] app=" << request->app_code()
              << " user=" << request->user_id()
              << " perm=" << request->perm_key()
              << " resource=" << request->resource_id()
              << " result=" << (allowed ? "ALLOW" : "DENY")
              << " latency=" << bcntl->latency_us() << "us";
}

void AuthServiceImpl::BatchCheck(google::protobuf::RpcController* cntl,
                                const siqi::auth::BatchCheckRequest* request,
                                siqi::auth::BatchCheckResponse* response,
                                google::protobuf::Closure* done) {
    
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* bcntl = static_cast<brpc::Controller*>(cntl);
    
    // 1. 参数验证
    if (request->app_code().empty() || request->items_size() == 0) {
        bcntl->SetFailed(EINVAL, "参数不完整");
        return;
    }
    
    // 2. 准备批量查询数据
    std::vector<std::tuple<std::string, std::string>> queries;
    for (int i = 0; i < request->items_size(); i++) {
        const auto& item = request->items(i);
        queries.emplace_back(item.user_id(), item.perm_key());
    }
    
    // 3. 执行批量检查
    std::vector<bool> results;
    try {
        results = dao_.batchCheckPermissions(request->app_code(), queries);
    } catch (const std::exception& e) {
        LOG(ERROR) << "批量权限检查异常: " << e.what();
        bcntl->SetFailed(brpc::EINTERNAL, "系统内部错误");
        return;
    }
    
    // 4. 构建响应
    for (size_t i = 0; i < results.size(); i++) {
        auto* result_item = response->add_results();
        const auto& request_item = request->items(i);
        
        result_item->set_user_id(request_item.user_id());
        result_item->set_perm_key(request_item.perm_key());
        result_item->set_allowed(results[i]);
        
        if (!results[i]) {
            result_item->set_reason("用户没有该权限");
        }
    }
    
    LOG(INFO) << "[BatchCheck] app=" << request->app_code()
              << " count=" << request->items_size()
              << " latency=" << bcntl->latency_us() << "us";
}

bool AuthServiceImpl::isReady() const {
    return dao_.isConnected();
}