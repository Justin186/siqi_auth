#ifndef ADMIN_SERVICE_IMPL_H
#define ADMIN_SERVICE_IMPL_H

#include "permission_dao.h"
#include "auth.pb.h"
#include "local_cache.h"
#include <brpc/server.h>
#include <butil/logging.h>
#include <unordered_set>

class AdminServiceImpl : public siqi::auth::AdminService {
private:
    PermissionDAO dao_;
    std::shared_ptr<LocalCache<std::unordered_set<std::string>>> cache_;
    int session_ttl_;
    
public:
    AdminServiceImpl(std::shared_ptr<LocalCache<std::unordered_set<std::string>>> cache,
                     const std::string& host,
                     int port,
                     const std::string& user,
                     const std::string& password,
                     const std::string& database,
                     int session_ttl);
                     
    // ------------------------- 应用管理 -------------------------
    void CreateApp(google::protobuf::RpcController* cntl,
                   const siqi::auth::CreateAppRequest* request,
                   siqi::auth::AdminResponse* response,
                   google::protobuf::Closure* done) override;

    void UpdateApp(google::protobuf::RpcController* cntl,
                   const siqi::auth::UpdateAppRequest* request,
                   siqi::auth::AdminResponse* response,
                   google::protobuf::Closure* done) override;

    void DeleteApp(google::protobuf::RpcController* cntl,
                   const siqi::auth::DeleteAppRequest* request,
                   siqi::auth::AdminResponse* response,
                   google::protobuf::Closure* done) override;

    void GetApp(google::protobuf::RpcController* cntl,
                const siqi::auth::GetAppRequest* request,
                siqi::auth::GetAppResponse* response,
                google::protobuf::Closure* done) override;

    void ListApps(google::protobuf::RpcController* cntl,
                  const siqi::auth::ListAppsRequest* request,
                  siqi::auth::ListAppsResponse* response,
                  google::protobuf::Closure* done) override;

    // ------------------------- 用户-角色授权 -------------------------
    void GrantRoleToUser(google::protobuf::RpcController* cntl,
                         const siqi::auth::GrantRoleToUserRequest* request,
                         siqi::auth::AdminResponse* response,
                         google::protobuf::Closure* done) override;

    void RevokeRoleFromUser(google::protobuf::RpcController* cntl,
                            const siqi::auth::RevokeRoleFromUserRequest* request,
                            siqi::auth::AdminResponse* response,
                            google::protobuf::Closure* done) override;
                            
    // ------------------------- 角色-权限绑定 -------------------------
    void AddPermissionToRole(google::protobuf::RpcController* cntl,
                             const siqi::auth::AddPermissionToRoleRequest* request,
                             siqi::auth::AdminResponse* response,
                             google::protobuf::Closure* done) override;

    void RemovePermissionFromRole(google::protobuf::RpcController* cntl,
                                  const siqi::auth::RemovePermissionFromRoleRequest* request,
                                  siqi::auth::AdminResponse* response,
                                  google::protobuf::Closure* done) override;
                                  
    // ------------------------- 创建管理 -------------------------
    void CreateRole(google::protobuf::RpcController* cntl,
                    const siqi::auth::CreateRoleRequest* request,
                    siqi::auth::AdminResponse* response,
                    google::protobuf::Closure* done) override;

    void CreatePermission(google::protobuf::RpcController* cntl,
                          const siqi::auth::CreatePermissionRequest* request,
                          siqi::auth::AdminResponse* response,
                          google::protobuf::Closure* done) override;

    void DeleteRole(google::protobuf::RpcController* cntl,
                    const siqi::auth::DeleteRoleRequest* request,
                    siqi::auth::AdminResponse* response,
                    google::protobuf::Closure* done) override;

    void DeletePermission(google::protobuf::RpcController* cntl,
                          const siqi::auth::DeletePermissionRequest* request,
                          siqi::auth::AdminResponse* response,
                          google::protobuf::Closure* done) override;

    void ListRoles(google::protobuf::RpcController* cntl,
                   const siqi::auth::ListRolesRequest* request,
                   siqi::auth::ListRolesResponse* response,
                   google::protobuf::Closure* done) override;

    void ListPermissions(google::protobuf::RpcController* cntl,
                         const siqi::auth::ListPermissionsRequest* request,
                         siqi::auth::ListPermissionsResponse* response,
                         google::protobuf::Closure* done) override;

    void UpdateRole(google::protobuf::RpcController* cntl,
                    const siqi::auth::UpdateRoleRequest* request,
                    siqi::auth::AdminResponse* response,
                    google::protobuf::Closure* done) override;

    void UpdatePermission(google::protobuf::RpcController* cntl,
                          const siqi::auth::UpdatePermissionRequest* request,
                          siqi::auth::AdminResponse* response,
                          google::protobuf::Closure* done) override;

    // 登录
    void Login(google::protobuf::RpcController* cntl,
               const siqi::auth::LoginRequest* request,
               siqi::auth::LoginResponse* response,
               google::protobuf::Closure* done) override;    

    struct SessionInfo {
        int64_t user_id;
        std::string username;
        std::string real_name;
    };

private:
   // Validate token and return session info. Returns false if invalid.
   bool ValidateToken(brpc::Controller* cntl, SessionInfo& session);
   
   LocalCache<SessionInfo> session_cache_;
};

#endif // ADMIN_SERVICE_IMPL_H
