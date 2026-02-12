#include "auth_service_impl.h"
#include <brpc/controller.h>
#include <errno.h>

AuthServiceImpl::AuthServiceImpl(std::shared_ptr<LocalCache<std::unordered_set<std::string>>> cache,
                                 const std::string& host,
                                 int port,
                                 const std::string& user,
                                 const std::string& password,
                                 const std::string& database,
                                 int cache_ttl)
    : dao_(host, port, user, password, database), cache_(cache), cache_ttl_(cache_ttl) {
    
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

    // 1. Params Validation
    if (request->app_code().empty() || request->user_id().empty() || request->perm_key().empty()) {
        response->set_allowed(false);
        response->set_reason("参数不完整");
        return;
    }

    // 2. Cache Lookup
    std::string cache_key = request->app_code() + ":" + request->user_id();
    std::unordered_set<std::string> user_perms;
    
    bool cache_hit = false;
    if (cache_) {
        cache_hit = cache_->Get(cache_key, user_perms);
    }
    
    if (cache_hit) {
        // Cache Hit logic
        bool allowed = (user_perms.count(request->perm_key()) > 0);
        response->set_allowed(allowed);
        if (!allowed) {
             response->set_reason("用户没有该权限 (Cache)");
        }
        LOG(INFO) << "Check " << request->user_id() << " -> " << request->perm_key() 
                  << (allowed ? " [ALLOW]" : " [DENY]") << " (Hit)";
        return;
    }

    // 3. Cache Miss - Load from DB
    try {
        // Here we assume getUserPermissions gets all effective permissions for the user
        // This avoids complex SQL in AuthServiceImpl and leverages DAO
        auto perms = dao_.getUserPermissions(request->app_code(), request->user_id());
        user_perms.clear();
        for (const auto& p : perms) {
            user_perms.insert(p.first); // Use perm_key (first), not perm_name (second)
        }
    } catch (const std::exception& e) {
         LOG(ERROR) << "DB Error: " << e.what();
         response->set_allowed(false);
         response->set_reason("系统错误");
         return;
    }
    
    // 4. Update Cache (TTL from config)
    if (cache_) {
        cache_->Put(cache_key, user_perms, cache_ttl_);
    }

    // 5. Final Check
    bool allowed = (user_perms.count(request->perm_key()) > 0);
    response->set_allowed(allowed);
    if (!allowed) {
        response->set_reason("用户没有该权限");
    }
    
    LOG(INFO) << "Check " << request->user_id() << " -> " << request->perm_key() 
              << (allowed ? " [ALLOW]" : " [DENY]") << (cache_hit ? " (Hit)" : " (Miss)");
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