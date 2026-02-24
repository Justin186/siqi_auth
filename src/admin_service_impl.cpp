#include "admin_service_impl.h"
#include <brpc/controller.h>
#include <gflags/gflags.h>
#include <unistd.h>
#include <crypt.h>
#include <random>
#include <sstream>

AdminServiceImpl::AdminServiceImpl(std::shared_ptr<LocalCache<std::unordered_set<std::string>>> cache,
                                   const std::string& host,
                                   int port,
                                   const std::string& user,
                                   const std::string& password,
                                   const std::string& database,
                                   int session_ttl)
    : dao_(host, port, user, password, database), cache_(cache), session_ttl_(session_ttl) {
}

bool AdminServiceImpl::ValidateToken(brpc::Controller* cntl, SessionInfo& session) {
    const std::string* auth_header = cntl->http_request().GetHeader("Authorization");
    if (!auth_header) {
        // 如果是浏览器直接访问，可能没有 Authorization 头
        // 但 CLI 工具应该带上
        return false;
    }
    
    std::string header_val = *auth_header;
    if (header_val.find("Bearer ") != 0) {
        return false;
    }
    
    std::string token = header_val.substr(7);
    bool found = session_cache_.Get(token, session);
    
    if (!found || session.username.empty()) {
        return false;
    }
    return true;
}

void AdminServiceImpl::CreateApp(google::protobuf::RpcController* cntl_base,
                                 const siqi::auth::CreateAppRequest* request,
                                 siqi::auth::AdminResponse* response,
                                 google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
    
    SessionInfo session;
    if (!ValidateToken(cntl, session)) {
        response->set_success(false);
        response->set_message("Unauthorized: Login required");
        return;
    }
    
    if (request->app_code().empty() || request->app_name().empty()) {
        response->set_success(false);
        response->set_code(EINVAL);
        response->set_message("缺少必要参数");
        return;
    }
    
    std::string app_secret;
    if (dao_.createApp(request->app_name(), request->app_code(), request->description(), app_secret)) {
        response->set_success(true);
        response->set_code(0);
        response->set_message("创建应用成功");
        response->set_app_secret(app_secret);
        
        dao_.createAuditLog(session.user_id, session.real_name, request->app_code(), 
                            "CREATE_APP", "APP", request->app_code(), request->app_name());
    } else {
        response->set_success(false);
        response->set_code(1001);
        response->set_message("创建应用失败: " + dao_.getLastError());
    }
}

void AdminServiceImpl::UpdateApp(google::protobuf::RpcController* cntl_base,
                                 const siqi::auth::UpdateAppRequest* request,
                                 siqi::auth::AdminResponse* response,
                                 google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
    
    SessionInfo session;
    if (!ValidateToken(cntl, session)) {
        response->set_success(false);
        response->set_message("Unauthorized: Login required");
        return;
    }
    
    if (request->app_code().empty()) {
        response->set_success(false);
        response->set_code(EINVAL);
        response->set_message("缺少必要参数");
        return;
    }
    
    const std::string* app_name = request->has_app_name() ? &request->app_name() : nullptr;
    const std::string* description = request->has_description() ? &request->description() : nullptr;
    
    int32_t status_val = 0;
    const int32_t* status = nullptr;
    if (request->has_status()) {
        status_val = request->status();
        status = &status_val;
    }
    
    if (dao_.updateApp(request->app_code(), app_name, description, status)) {
        response->set_success(true);
        response->set_code(0);
        response->set_message("更新应用成功");
        
        dao_.createAuditLog(session.user_id, session.real_name, request->app_code(), 
                            "UPDATE_APP", "APP", request->app_code());
    } else {
        response->set_success(false);
        response->set_code(1001);
        response->set_message("更新应用失败: " + dao_.getLastError());
    }
}

void AdminServiceImpl::DeleteApp(google::protobuf::RpcController* cntl_base,
                                 const siqi::auth::DeleteAppRequest* request,
                                 siqi::auth::AdminResponse* response,
                                 google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
    
    SessionInfo session;
    if (!ValidateToken(cntl, session)) {
        response->set_success(false);
        response->set_message("Unauthorized: Login required");
        return;
    }
    
    if (request->app_code().empty()) {
        response->set_success(false);
        response->set_code(EINVAL);
        response->set_message("缺少必要参数");
        return;
    }
    
    if (dao_.deleteApp(request->app_code())) {
        response->set_success(true);
        response->set_code(0);
        response->set_message("删除应用成功");
        
        dao_.createAuditLog(session.user_id, session.real_name, request->app_code(), 
                            "DELETE_APP", "APP", request->app_code());
    } else {
        response->set_success(false);
        response->set_code(1001);
        response->set_message("删除应用失败: " + dao_.getLastError());
    }
}

void AdminServiceImpl::GetApp(google::protobuf::RpcController* cntl_base,
                              const siqi::auth::GetAppRequest* request,
                              siqi::auth::GetAppResponse* response,
                              google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
    
    SessionInfo session;
    if (!ValidateToken(cntl, session)) {
        cntl->SetFailed(brpc::EAUTH, "Unauthorized: Login required");
        return;
    }
    
    if (request->app_code().empty()) {
        cntl->SetFailed(EINVAL, "缺少必要参数");
        return;
    }
    
    PermissionDAO::AppInfo app;
    if (dao_.getApp(request->app_code(), app)) {
        response->set_id(app.id);
        response->set_app_name(app.app_name);
        response->set_app_code(app.app_code);
        response->set_app_secret(app.app_secret);
        response->set_description(app.description);
        response->set_status(app.status);
        response->set_created_at(app.created_at);
        response->set_updated_at(app.updated_at);
    } else {
        cntl->SetFailed(ENOENT, "应用不存在");
    }
}

void AdminServiceImpl::ListApps(google::protobuf::RpcController* cntl_base,
                                const siqi::auth::ListAppsRequest* request,
                                siqi::auth::ListAppsResponse* response,
                                google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
    
    SessionInfo session;
    if (!ValidateToken(cntl, session)) {
        cntl->SetFailed(brpc::EAUTH, "Unauthorized: Login required");
        return;
    }
    
    int32_t page = request->page() > 0 ? request->page() : 1;
    int32_t page_size = request->page_size() > 0 ? request->page_size() : 20;
    
    const std::string* app_name = request->has_app_name() ? &request->app_name() : nullptr;
    
    int32_t status_val = 0;
    const int32_t* status = nullptr;
    if (request->has_status()) {
        status_val = request->status();
        status = &status_val;
    }
    
    int64_t total = 0;
    auto apps = dao_.listApps(page, page_size, app_name, status, total);
    
    for (const auto& app : apps) {
        auto* app_resp = response->add_apps();
        app_resp->set_id(app.id);
        app_resp->set_app_name(app.app_name);
        app_resp->set_app_code(app.app_code);
        app_resp->set_app_secret(app.app_secret);
        app_resp->set_description(app.description);
        app_resp->set_status(app.status);
        app_resp->set_created_at(app.created_at);
        app_resp->set_updated_at(app.updated_at);
    }
    
    response->set_total(total);
    response->set_page(page);
    response->set_page_size(page_size);
}

void AdminServiceImpl::GrantRoleToUser(google::protobuf::RpcController* cntl_base,
                                       const siqi::auth::GrantRoleToUserRequest* request,
                                       siqi::auth::AdminResponse* response,
                                       google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
    
    SessionInfo session;
    if (!ValidateToken(cntl, session)) {
        response->set_success(false);
        response->set_message("Unauthorized: Login required");
        return;
    }
    
    // 缓存失效处理
    if (cache_) cache_->Invalidate(request->app_code() + ":" + request->user_id());

    // 参数校验
    if (request->app_code().empty() || 
        request->user_id().empty() || request->role_key().empty()) {
        response->set_success(false);
        response->set_code(EINVAL);
        response->set_message("缺少必要参数");
        return;
    }
    
    // 执行操作
    if (dao_.assignRoleToUser(request->app_code(), request->user_id(), request->role_key())) {
        response->set_success(true);
        response->set_code(0);
        response->set_message("授权成功");
        
        // 审计日志
        dao_.createAuditLog(session.user_id, 
                            session.real_name,
                            request->app_code(), 
                            "USER_GRANT_ROLE", 
                            "USER", request->user_id(), "", 
                            "ROLE", request->role_key(), "");
    } else {
        response->set_success(false);
        response->set_code(1001); // 业务错误码
        response->set_message("授权失败: " + dao_.getLastError());
    }
}

void AdminServiceImpl::RevokeRoleFromUser(google::protobuf::RpcController* cntl_base,
                                          const siqi::auth::RevokeRoleFromUserRequest* request,
                                          siqi::auth::AdminResponse* response,
                                          google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
    
    SessionInfo session;
    if (!ValidateToken(cntl, session)) {
        response->set_success(false);
        response->set_message("Unauthorized: Login required");
        return;
    }
    
    // 缓存失效处理
    if (cache_) cache_->Invalidate(request->app_code() + ":" + request->user_id());

    if (request->app_code().empty() || 
        request->user_id().empty() || request->role_key().empty()) {
        response->set_success(false);
        response->set_message("缺少必要参数");
        return;
    }
    
    if (dao_.removeRoleFromUser(request->app_code(), request->user_id(), request->role_key())) {
        response->set_success(true);
        response->set_message("撤销成功");
        
        dao_.createAuditLog(session.user_id, session.real_name, request->app_code(), 
                            "USER_REVOKE_ROLE", 
                            "USER", request->user_id(), "", 
                            "ROLE", request->role_key(), "");
    } else {
        response->set_success(false);
        response->set_message("撤销失败: " + dao_.getLastError());
    }
}

void AdminServiceImpl::AddPermissionToRole(google::protobuf::RpcController* cntl_base,
                                           const siqi::auth::AddPermissionToRoleRequest* request,
                                           siqi::auth::AdminResponse* response,
                                           google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
    
    SessionInfo session;
    if (!ValidateToken(cntl, session)) {
        response->set_success(false);
        response->set_message("Unauthorized: Login required");
        return;
    }
    
    // 缓存失效处理:
    // 角色权限变更会影响所有拥有该角色的用户。
    // 由于我们无法反向查找哪些用户拥有此角色，因此失效该 App 下的所有用户缓存。
    // 这种操作频率较低（仅在管理员配置时），因此 O(N) 的遍历清理是可以接受的。
    if (cache_) cache_->InvalidatePrefix(request->app_code() + ":");

    if (request->app_code().empty() || request->role_key().empty() || request->perm_key().empty()) {
        response->set_success(false);
        response->set_message("缺少必要参数");
        return;
    }
    
    if (dao_.addPermissionToRole(request->app_code(), request->role_key(), request->perm_key())) {
        response->set_success(true);
        response->set_message("绑定成功");
        
        dao_.createAuditLog(session.user_id, session.real_name, request->app_code(), 
                            "ROLE_ADD_PERM", 
                            "ROLE", request->role_key(), "", 
                            "PERM", request->perm_key(), "");
    } else {
        response->set_success(false);
        response->set_message("绑定失败: " + dao_.getLastError());
    }
}

void AdminServiceImpl::RemovePermissionFromRole(google::protobuf::RpcController* cntl_base,
                                                const siqi::auth::RemovePermissionFromRoleRequest* request,
                                                siqi::auth::AdminResponse* response,
                                                google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
    
    SessionInfo session;
    if (!ValidateToken(cntl, session)) {
        response->set_success(false);
        response->set_message("Unauthorized: Login required");
        return;
    }
    
    // 缓存失效处理（同上）：清理该应用下的所有缓存
    if (cache_) cache_->InvalidatePrefix(request->app_code() + ":");

    if (request->app_code().empty() || request->role_key().empty() || request->perm_key().empty()) {
        response->set_success(false);
        response->set_message("缺少必要参数");
        return;
    }
    
    if (dao_.removePermissionFromRole(request->app_code(), request->role_key(), request->perm_key())) {
        response->set_success(true);
        response->set_message("解绑成功");
        
        dao_.createAuditLog(session.user_id, session.real_name, request->app_code(), 
                            "ROLE_REMOVE_PERM", 
                            "ROLE", request->role_key(), "", 
                            "PERM", request->perm_key(), "");
    } else {
        response->set_success(false);
        response->set_message("解绑失败: " + dao_.getLastError());
    }
}

void AdminServiceImpl::CreateRole(google::protobuf::RpcController* cntl_base,
                                  const siqi::auth::CreateRoleRequest* request,
                                  siqi::auth::AdminResponse* response,
                                  google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
    
    SessionInfo session;
    if (!ValidateToken(cntl, session)) {
        response->set_success(false);
        response->set_message("Unauthorized: Login required");
        return;
    }
    
    if (request->app_code().empty() || request->role_key().empty() || request->role_name().empty()) {
        response->set_success(false);
        response->set_message("缺少必要参数");
        return;
    }
    
    // 默认值处理
    bool is_default = request->is_default();
    
    if (dao_.createRole(request->app_code(), request->role_name(), request->role_key(), request->description(), is_default)) {
        response->set_success(true);
        response->set_message("创建角色成功");
        
        dao_.createAuditLog(session.user_id, session.real_name, request->app_code(), 
                            "CREATE_ROLE", 
                            "ROLE", request->role_key(), request->role_name());
    } else {
        response->set_success(false);
        response->set_message("创建角色失败: " + dao_.getLastError());
    }
}

void AdminServiceImpl::CreatePermission(google::protobuf::RpcController* cntl_base,
                                        const siqi::auth::CreatePermissionRequest* request,
                                        siqi::auth::AdminResponse* response,
                                        google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
    
    SessionInfo session;
    if (!ValidateToken(cntl, session)) {
        response->set_success(false);
        response->set_message("Unauthorized: Login required");
        return;
    }
    
    if (request->app_code().empty() || request->perm_key().empty() || request->perm_name().empty()) {
        response->set_success(false);
        response->set_message("缺少必要参数");
        return;
    }
    
    if (dao_.createPermission(request->app_code(), request->perm_name(), request->perm_key(), request->description())) {
        response->set_success(true);
        response->set_message("创建权限成功");
        
        dao_.createAuditLog(session.user_id, session.real_name, request->app_code(), 
                            "CREATE_PERM", 
                            "PERM", request->perm_key(), request->perm_name());
    } else {
        response->set_success(false);
        response->set_message("创建权限失败: " + dao_.getLastError());
    }
}

void AdminServiceImpl::DeleteRole(google::protobuf::RpcController* cntl_base,
                                  const siqi::auth::DeleteRoleRequest* request,
                                  siqi::auth::AdminResponse* response,
                                  google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
    
    SessionInfo session;
    if (!ValidateToken(cntl, session)) {
        response->set_success(false);
        response->set_message("Unauthorized: Login required");
        return;
    }
    
    if (request->app_code().empty() || request->role_key().empty()) {
        response->set_success(false);
        response->set_message("缺少必要参数");
        return;
    }
    
    if (dao_.deleteRole(request->app_code(), request->role_key())) {
        response->set_success(true);
        response->set_message("删除角色成功");
        
        dao_.createAuditLog(session.user_id, session.real_name, request->app_code(), 
                            "DELETE_ROLE", 
                            "ROLE", request->role_key());
    } else {
        response->set_success(false);
        response->set_message(dao_.getLastError());
    }
}

void AdminServiceImpl::DeletePermission(google::protobuf::RpcController* cntl_base,
                                        const siqi::auth::DeletePermissionRequest* request,
                                        siqi::auth::AdminResponse* response,
                                        google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
    
    SessionInfo session;
    if (!ValidateToken(cntl, session)) {
        response->set_success(false);
        response->set_message("Unauthorized: Login required");
        return;
    }
    
    if (request->app_code().empty() || request->perm_key().empty()) {
        response->set_success(false);
        response->set_message("缺少必要参数");
        return;
    }
    
    if (dao_.deletePermission(request->app_code(), request->perm_key())) {
        response->set_success(true);
        response->set_message("删除权限成功");
        
        dao_.createAuditLog(session.user_id, session.real_name, request->app_code(), 
                            "DELETE_PERM", 
                            "PERM", request->perm_key());
    } else {
        response->set_success(false);
        response->set_message(dao_.getLastError());
    }
}

void AdminServiceImpl::ListRoles(google::protobuf::RpcController* cntl_base,
                                 const siqi::auth::ListRolesRequest* request,
                                 siqi::auth::ListRolesResponse* response,
                                 google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
    
    SessionInfo session;
    if (!ValidateToken(cntl, session)) {
        cntl->SetFailed(EPERM, "Unauthorized: Login required");
        return;
    }
    
    auto roles = dao_.listRoles(request->app_code());
    for (const auto& r : roles) {
        auto* role_pb = response->add_roles();
        role_pb->set_id(r.id);
        role_pb->set_role_name(r.role_name);
        role_pb->set_role_key(r.role_key);
        role_pb->set_description(r.description);
        role_pb->set_is_default(r.is_default);
    }
    response->set_total(roles.size());
}

void AdminServiceImpl::ListPermissions(google::protobuf::RpcController* cntl_base,
                                       const siqi::auth::ListPermissionsRequest* request,
                                       siqi::auth::ListPermissionsResponse* response,
                                       google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
    
    SessionInfo session;
    if (!ValidateToken(cntl, session)) {
        cntl->SetFailed(EPERM, "Unauthorized: Login required");
        return;
    }
    
    auto perms = dao_.listPermissions(request->app_code());
    for (const auto& p : perms) {
        auto* perm_pb = response->add_permissions();
        perm_pb->set_id(p.id);
        perm_pb->set_perm_name(p.perm_name);
        perm_pb->set_perm_key(p.perm_key);
        perm_pb->set_description(p.description);
    }
    response->set_total(perms.size());
}

void AdminServiceImpl::UpdateRole(google::protobuf::RpcController* cntl_base,
                                  const siqi::auth::UpdateRoleRequest* request,
                                  siqi::auth::AdminResponse* response,
                                  google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
    
    SessionInfo session;
    if (!ValidateToken(cntl, session)) {
        response->set_success(false);
        response->set_message("Unauthorized: Login required");
        return;
    }
    
    if (request->app_code().empty() || request->role_key().empty()) {
        response->set_success(false);
        response->set_message("缺少必要参数");
        return;
    }
    
    const std::string* role_name = request->has_role_name() ? &request->role_name() : nullptr;
    const std::string* description = request->has_description() ? &request->description() : nullptr;
    
    bool is_default_val = false;
    const bool* is_default = nullptr;
    if (request->has_is_default()) {
        is_default_val = request->is_default();
        is_default = &is_default_val;
    }
    
    if (dao_.updateRole(request->app_code(), request->role_key(), role_name, description, is_default)) {
        response->set_success(true);
        response->set_message("更新角色成功");
        
        dao_.createAuditLog(session.user_id, session.real_name, request->app_code(), 
                            "UPDATE_ROLE", 
                            "ROLE", request->role_key());
    } else {
        response->set_success(false);
        response->set_message(dao_.getLastError());
    }
}

void AdminServiceImpl::UpdatePermission(google::protobuf::RpcController* cntl_base,
                                        const siqi::auth::UpdatePermissionRequest* request,
                                        siqi::auth::AdminResponse* response,
                                        google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
    
    SessionInfo session;
    if (!ValidateToken(cntl, session)) {
        response->set_success(false);
        response->set_message("Unauthorized: Login required");
        return;
    }
    
    if (request->app_code().empty() || request->perm_key().empty()) {
        response->set_success(false);
        response->set_message("缺少必要参数");
        return;
    }
    
    const std::string* perm_name = request->has_perm_name() ? &request->perm_name() : nullptr;
    const std::string* description = request->has_description() ? &request->description() : nullptr;
    
    if (dao_.updatePermission(request->app_code(), request->perm_key(), perm_name, description)) {
        response->set_success(true);
        response->set_message("更新权限成功");
        
        dao_.createAuditLog(session.user_id, session.real_name, request->app_code(), 
                            "UPDATE_PERM", 
                            "PERM", request->perm_key());
    } else {
        response->set_success(false);
        response->set_message(dao_.getLastError());
    }
}

void AdminServiceImpl::GetRolePermissions(google::protobuf::RpcController* cntl_base,
                                          const siqi::auth::GetRolePermissionsRequest* request,
                                          siqi::auth::GetRolePermissionsResponse* response,
                                          google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
    
    SessionInfo session;
    if (!ValidateToken(cntl, session)) {
        // Note: GetRolePermissionsResponse doesn't have success/message fields in proto
        // We can set HTTP status code to 401
        cntl->http_response().set_status_code(brpc::HTTP_STATUS_UNAUTHORIZED);
        return;
    }
    
    if (request->app_code().empty() || request->role_key().empty()) {
        cntl->http_response().set_status_code(brpc::HTTP_STATUS_BAD_REQUEST);
        return;
    }
    
    auto perms = dao_.getRolePermissions(request->app_code(), request->role_key());
    for (const auto& perm : perms) {
        response->add_perm_keys(perm);
    }
}

void AdminServiceImpl::GetRoleUsers(google::protobuf::RpcController* cntl_base,
                                    const siqi::auth::GetRoleUsersRequest* request,
                                    siqi::auth::GetRoleUsersResponse* response,
                                    google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
    
    SessionInfo session;
    if (!ValidateToken(cntl, session)) {
        cntl->http_response().set_status_code(brpc::HTTP_STATUS_UNAUTHORIZED);
        return;
    }
    
    if (request->app_code().empty() || request->role_key().empty()) {
        cntl->http_response().set_status_code(brpc::HTTP_STATUS_BAD_REQUEST);
        return;
    }
    
    int32_t page = request->page() > 0 ? request->page() : 1;
    int32_t page_size = request->page_size() > 0 ? request->page_size() : 10;
    
    int64_t total = 0;
    auto users = dao_.getRoleUsers(request->app_code(), request->role_key(), page, page_size, total);
    
    for (const auto& u : users) {
        auto* user = response->add_users();
        user->set_user_id(u.user_id);
        user->set_created_at(u.created_at);
    }
    
    response->set_total(total);
    response->set_page(page);
    response->set_page_size(page_size);
}

void AdminServiceImpl::Login(google::protobuf::RpcController* cntl_base,
                             const siqi::auth::LoginRequest* request,
                             siqi::auth::LoginResponse* response,
                             google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    
    std::string username = request->username();
    std::string password = request->password();
    
    if (username.empty() || password.empty()) {
        response->set_success(false);
        response->set_message("用户名或密码为空");
        return;
    }
    
    // 1. Get User from DB
    auto user_info = dao_.getConsoleUser(username);
    if (user_info.id == -1) {
        response->set_success(false);
        response->set_message("用户不存在或密码错误");
        return;
    }
    
    std::string db_hash = user_info.password_hash;
    
    // 2. Verify Password using crypt
    // crypt(key, salt) -> hash
    // The salt is embedded in the hash itself (a$...)
    char* calc_hash = crypt(password.c_str(), db_hash.c_str());
    if (calc_hash == NULL || db_hash != calc_hash) {
        response->set_success(false);
        response->set_message("用户不存在或密码错误");
        return;
    }
    
    // 3. Generate Simple Token (UUID-like)
    // In production use proper session management / JWT
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    std::stringstream ss;
    for (int i = 0; i < 32; i++) {
        const char* hex = "0123456789abcdef";
        ss << hex[dis(gen)];
    }
    std::string token = ss.str();
    
    // 4. Store Session (TTL 1 Hour)
    SessionInfo session;
    session.user_id = user_info.id;
    session.username = user_info.username;
    session.real_name = user_info.real_name;
    session_cache_.Put(token, session, session_ttl_);
    
    response->set_success(true);
    response->set_message("登录成功");
    response->set_token(token);
    
    LOG(INFO) << "User Logged In: " << username << " (ID: " << user_info.id << ") Token: " << token;
}

void AdminServiceImpl::ListAuditLogs(google::protobuf::RpcController* cntl_base,
                                     const siqi::auth::ListAuditLogsRequest* request,
                                     siqi::auth::ListAuditLogsResponse* response,
                                     google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
    
    SessionInfo session;
    if (!ValidateToken(cntl, session)) {
        cntl->http_response().set_status_code(brpc::HTTP_STATUS_UNAUTHORIZED);
        return;
    }
    
    int32_t page = request->page() > 0 ? request->page() : 1;
    int32_t page_size = request->page_size() > 0 ? request->page_size() : 20;
    
    const std::string* app_code = request->has_app_code() ? &request->app_code() : nullptr;
    const std::string* action = request->has_action() ? &request->action() : nullptr;
    const std::string* operator_id = request->has_operator_id() ? &request->operator_id() : nullptr;
    const std::string* target_id = request->has_target_id() ? &request->target_id() : nullptr;
    
    int64_t total = 0;
    auto logs = dao_.listAuditLogs(page, page_size, app_code, action, operator_id, target_id, total);
    
    for (const auto& l : logs) {
        auto* log = response->add_logs();
        log->set_id(l.id);
        log->set_operator_id(l.operator_id);
        log->set_operator_name(l.operator_name);
        log->set_app_code(l.app_code);
        log->set_action(l.action);
        log->set_target_type(l.target_type);
        log->set_target_id(l.target_id);
        log->set_target_name(l.target_name);
        log->set_object_type(l.object_type);
        log->set_object_id(l.object_id);
        log->set_object_name(l.object_name);
        log->set_created_at(l.created_at);
    }
    
    response->set_total(total);
    response->set_page(page);
    response->set_page_size(page_size);
}
