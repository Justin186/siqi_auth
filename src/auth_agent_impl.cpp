#include "auth_agent.h"
#include <butil/logging.h>

// 构造函数：注入 DAO 对象
AuthAgentImpl::AuthAgentImpl(PermissionDAO* dao) 
    : dao_(dao) {
}

void AuthAgentImpl::Check(google::protobuf::RpcController* cntl_base,
                   const siqi::auth::CheckRequest* request,
                   siqi::auth::CheckResponse* response,
                   google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);

    // 0. 提取参数 (优先读 PB Request，若为空则读 URL QueryString)
    std::string app_code = request->app_code();
    std::string user_id = request->user_id();
    std::string perm_key = request->perm_key();

    if (app_code.empty()) {
        const std::string* p = cntl->http_request().uri().GetQuery("app_code");
        if (p) app_code = *p;
    }
    if (user_id.empty()) {
        const std::string* p = cntl->http_request().uri().GetQuery("user_id");
        if (p) user_id = *p;
    }
    if (perm_key.empty()) {
        const std::string* p = cntl->http_request().uri().GetQuery("perm_key");
        if (p) perm_key = *p;
    }

    // 1. 参数校验
    if (app_code.empty() || user_id.empty() || perm_key.empty()) {
        LOG(WARNING) << "Agent 收到非法请求: " 
                     << " app_code=" << app_code
                     << " user_id=" << user_id
                     << " perm_key=" << perm_key
                     << " from=" << cntl->remote_side();
        
        response->set_allowed(false);
        response->set_reason("参数不完整 (Agent)");
        return;
    }

    // 2. 直接查询本地数据库 (MySQL Slave)
    // 由于是本地回环 (Localhost Loopback)，延迟极低 (<1ms)，无需额外缓存
    bool allowed = dao_->checkPermission(app_code, user_id, perm_key, ""); // resource_id 暂时留空，视业务需求而定
    
    // 3. 返回结果
    response->set_allowed(allowed);
    if (!allowed) {
        if (!dao_->appExists(app_code)) {
            response->set_reason("应用不存在");
        } else if (!dao_->permissionExists(app_code, perm_key)) {
            response->set_reason("权限不存在");
        } else {
            auto current_roles = dao_->getUserRoles(app_code, user_id);
            std::string reason_prefix = current_roles.empty() ? "用户不存在或未分配任何角色" : "用户没有该权限";
            
            std::string curr_roles_str = current_roles.empty() ? "无" : current_roles[0];
            for (size_t i = 1; i < current_roles.size(); ++i) curr_roles_str += "," + current_roles[i];
            response->set_current_roles(curr_roles_str);
            
            auto required_roles = dao_->getRolesWithPermission(app_code, perm_key);
            if (!required_roles.empty()) {
                std::string suggest = required_roles[0];
                for (size_t i = 1; i < required_roles.size(); ++i) suggest += "," + required_roles[i];
                response->set_suggest_roles(suggest);
            }
            
            response->set_reason(reason_prefix);
        }
    }
    
    // 添加 Header 标识这是本地直连查询
    cntl->http_response().SetHeader("X-Strategy", "Local-DB-Slave");
}
