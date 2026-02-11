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
    
public:
    AdminServiceImpl(std::shared_ptr<LocalCache<std::unordered_set<std::string>>> cache,
                     const std::string& host = "localhost",
                     const std::string& user = "siqi_dev",
                     const std::string& password = "siqi123",
                     const std::string& database = "siqi_auth");
                     
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
                                  
    // TODO: Implement other methods (CreateApp, ListApps, etc.)
    // For now, we only implement the prioritized ones to get started.
    // The others can return "Unimplemented" error by default or be added later.
};

#endif // ADMIN_SERVICE_IMPL_H
