#include "admin_service_impl.h"
#include <brpc/controller.h>
#include <gflags/gflags.h>

AdminServiceImpl::AdminServiceImpl(const std::string& host,
                                   const std::string& user,
                                   const std::string& password,
                                   const std::string& database)
    : dao_(host, user, password, database) {
}

void AdminServiceImpl::GrantRoleToUser(google::protobuf::RpcController* cntl_base,
                                       const siqi::auth::GrantRoleToUserRequest* request,
                                       siqi::auth::AdminResponse* response,
                                       google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
    
    // 参数校验
    if (request->operator_id().empty() || request->app_code().empty() || 
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
        // 注意：这里operator_id实际上应该是个ID，但proto里是string。
        // 这里只是示例，实际需要根据业务转换。暂定operator_id转int64，或者你可以修改createAuditLog接受string
        // 假设operator_id是数字字符串:
        int64_t op_id = 0;
        try { op_id = std::stoll(request->operator_id()); } catch (...) {}
        
        dao_.createAuditLog(op_id, 
                            "Admin", // 暂时拿不到名字，可以改Proto加operator_name或者查表
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
    
    if (request->operator_id().empty() || request->app_code().empty() || 
        request->user_id().empty() || request->role_key().empty()) {
        response->set_success(false);
        response->set_message("缺少必要参数");
        return;
    }
    
    if (dao_.removeRoleFromUser(request->app_code(), request->user_id(), request->role_key())) {
        response->set_success(true);
        response->set_message("撤销成功");
        
        int64_t op_id = 0;
        try { op_id = std::stoll(request->operator_id()); } catch (...) {}
        
        dao_.createAuditLog(op_id, "Admin", request->app_code(), 
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
    
    if (request->app_code().empty() || request->role_key().empty() || request->perm_key().empty()) {
        response->set_success(false);
        response->set_message("缺少必要参数");
        return;
    }
    
    if (dao_.addPermissionToRole(request->app_code(), request->role_key(), request->perm_key())) {
        response->set_success(true);
        response->set_message("绑定成功");
        
        int64_t op_id = 0;
        try { op_id = std::stoll(request->operator_id()); } catch (...) {}
        
        dao_.createAuditLog(op_id, "Admin", request->app_code(), 
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
    
    if (request->app_code().empty() || request->role_key().empty() || request->perm_key().empty()) {
        response->set_success(false);
        response->set_message("缺少必要参数");
        return;
    }
    
    if (dao_.removePermissionFromRole(request->app_code(), request->role_key(), request->perm_key())) {
        response->set_success(true);
        response->set_message("解绑成功");
        
        int64_t op_id = 0;
        try { op_id = std::stoll(request->operator_id()); } catch (...) {}
        
        dao_.createAuditLog(op_id, "Admin", request->app_code(), 
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
        
        int64_t op_id = 0;
        try { op_id = std::stoll(request->operator_id()); } catch (...) {}
        
        dao_.createAuditLog(op_id, "Admin", request->app_code(), 
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
    
    if (request->app_code().empty() || request->perm_key().empty() || request->perm_name().empty()) {
        response->set_success(false);
        response->set_message("缺少必要参数");
        return;
    }
    
    if (dao_.createPermission(request->app_code(), request->perm_name(), request->perm_key(), request->description())) {
        response->set_success(true);
        response->set_message("创建权限成功");
        
        int64_t op_id = 0;
        try { op_id = std::stoll(request->operator_id()); } catch (...) {}
        
        dao_.createAuditLog(op_id, "Admin", request->app_code(), 
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
    
    if (request->app_code().empty() || request->role_key().empty()) {
        response->set_success(false);
        response->set_message("缺少必要参数");
        return;
    }
    
    if (dao_.deleteRole(request->app_code(), request->role_key())) {
        response->set_success(true);
        response->set_message("删除角色成功");
        
        int64_t op_id = 0;
        try { op_id = std::stoll(request->operator_id()); } catch (...) {}
        
        dao_.createAuditLog(op_id, "Admin", request->app_code(), 
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
    
    if (request->app_code().empty() || request->perm_key().empty()) {
        response->set_success(false);
        response->set_message("缺少必要参数");
        return;
    }
    
    if (dao_.deletePermission(request->app_code(), request->perm_key())) {
        response->set_success(true);
        response->set_message("删除权限成功");
        
        int64_t op_id = 0;
        try { op_id = std::stoll(request->operator_id()); } catch (...) {}
        
        dao_.createAuditLog(op_id, "Admin", request->app_code(), 
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
        
        int64_t op_id = 0;
        try { op_id = std::stoll(request->operator_id()); } catch (...) {}
        
        dao_.createAuditLog(op_id, "Admin", request->app_code(), 
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
        
        int64_t op_id = 0;
        try { op_id = std::stoll(request->operator_id()); } catch (...) {}
        
        dao_.createAuditLog(op_id, "Admin", request->app_code(), 
                            "UPDATE_PERM", 
                            "PERM", request->perm_key());
    } else {
        response->set_success(false);
        response->set_message(dao_.getLastError());
    }
}
