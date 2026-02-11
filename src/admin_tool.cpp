#include <brpc/channel.h>
#include <gflags/gflags.h>
#include <iostream>
#include "auth.pb.h"

DEFINE_string(server, "127.0.0.1:8888", "IP Address of server");
DEFINE_string(op, "", "Operation: grant_role, revoke_role, add_perm, remove_perm");
DEFINE_string(app, "qq_bot", "App code");
DEFINE_string(user, "", "User ID");
DEFINE_string(role, "", "Role Key");
DEFINE_string(perm, "", "Permission Key");
DEFINE_string(operator_id, "1", "Operator ID (Admin ID)");
DEFINE_string(name, "", "Name for role or permission");
DEFINE_string(desc, "", "Description");
DEFINE_bool(is_default, false, "Is default role");

int main(int argc, char* argv[]) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    
    brpc::Channel channel;
    brpc::ChannelOptions options;
    options.timeout_ms = 2000;
    options.max_retry = 3;
    if (channel.Init(FLAGS_server.c_str(), &options) != 0) {
        LOG(ERROR) << "Fail to initialize channel";
        return -1;
    }

    siqi::auth::AdminService_Stub stub(&channel);
    siqi::auth::AdminResponse response;
    brpc::Controller cntl;

    if (FLAGS_op == "grant_role") {
        if (FLAGS_user.empty() || FLAGS_role.empty()) {
            std::cerr << "Missing --user or --role" << std::endl;
            return -1;
        }
        siqi::auth::GrantRoleToUserRequest request;
        request.set_operator_id(FLAGS_operator_id);
        request.set_app_code(FLAGS_app);
        request.set_user_id(FLAGS_user);
        request.set_role_key(FLAGS_role);
        
        stub.GrantRoleToUser(&cntl, &request, &response, NULL);
    } else if (FLAGS_op == "revoke_role") {
        if (FLAGS_user.empty() || FLAGS_role.empty()) {
            std::cerr << "Missing --user or --role" << std::endl;
            return -1;
        }
        siqi::auth::RevokeRoleFromUserRequest request;
        request.set_operator_id(FLAGS_operator_id);
        request.set_app_code(FLAGS_app);
        request.set_user_id(FLAGS_user);
        request.set_role_key(FLAGS_role);
        
        stub.RevokeRoleFromUser(&cntl, &request, &response, NULL);
    } else if (FLAGS_op == "add_perm") {
        if (FLAGS_role.empty() || FLAGS_perm.empty()) {
            std::cerr << "Missing --role or --perm" << std::endl;
            return -1;
        }
        siqi::auth::AddPermissionToRoleRequest request;
        request.set_operator_id(FLAGS_operator_id);
        request.set_app_code(FLAGS_app);
        request.set_role_key(FLAGS_role);
        request.set_perm_key(FLAGS_perm);
        
        stub.AddPermissionToRole(&cntl, &request, &response, NULL);
    } else if (FLAGS_op == "create_role") {
        if (FLAGS_role.empty() || FLAGS_name.empty()) {
            std::cerr << "Missing --role or --name" << std::endl;
            return -1;
        }
        siqi::auth::CreateRoleRequest request;
        request.set_operator_id(FLAGS_operator_id);
        request.set_app_code(FLAGS_app);
        request.set_role_key(FLAGS_role);
        request.set_role_name(FLAGS_name);
        request.set_description(FLAGS_desc);
        request.set_is_default(FLAGS_is_default);
        
        stub.CreateRole(&cntl, &request, &response, NULL);
    } else if (FLAGS_op == "create_perm") {
        if (FLAGS_perm.empty() || FLAGS_name.empty()) {
            std::cerr << "Missing --perm or --name" << std::endl;
            return -1;
        }
        siqi::auth::CreatePermissionRequest request;
        request.set_operator_id(FLAGS_operator_id);
        request.set_app_code(FLAGS_app);
        request.set_perm_key(FLAGS_perm);
        request.set_perm_name(FLAGS_name);
        request.set_description(FLAGS_desc);
        
        stub.CreatePermission(&cntl, &request, &response, NULL);
    } else if (FLAGS_op == "delete_role") {
        if (FLAGS_role.empty()) {
            std::cerr << "Missing --role" << std::endl;
            return -1;
        }
        siqi::auth::DeleteRoleRequest request;
        request.set_operator_id(FLAGS_operator_id);
        request.set_app_code(FLAGS_app);
        request.set_role_key(FLAGS_role);
        
        stub.DeleteRole(&cntl, &request, &response, NULL);
    } else if (FLAGS_op == "delete_perm") {
        if (FLAGS_perm.empty()) {
            std::cerr << "Missing --perm" << std::endl;
            return -1;
        }
        siqi::auth::DeletePermissionRequest request;
        request.set_operator_id(FLAGS_operator_id);
        request.set_app_code(FLAGS_app);
        request.set_perm_key(FLAGS_perm);
        
        stub.DeletePermission(&cntl, &request, &response, NULL);
    } else if (FLAGS_op == "update_role") {
        if (FLAGS_role.empty()) {
            std::cerr << "Missing --role" << std::endl;
            return -1;
        }
        siqi::auth::UpdateRoleRequest request;
        request.set_operator_id(FLAGS_operator_id);
        request.set_app_code(FLAGS_app);
        request.set_role_key(FLAGS_role);
        if (!FLAGS_name.empty()) request.set_role_name(FLAGS_name);
        if (!FLAGS_desc.empty()) request.set_description(FLAGS_desc);
        if (gflags::GetCommandLineFlagInfoOrDie("is_default").is_default == false) {
             request.set_is_default(FLAGS_is_default);
        }

        stub.UpdateRole(&cntl, &request, &response, NULL);
    } else if (FLAGS_op == "update_perm") {
        if (FLAGS_perm.empty()) {
            std::cerr << "Missing --perm" << std::endl;
            return -1;
        }
        siqi::auth::UpdatePermissionRequest request;
        request.set_operator_id(FLAGS_operator_id);
        request.set_app_code(FLAGS_app);
        request.set_perm_key(FLAGS_perm);
        if (!FLAGS_name.empty()) request.set_perm_name(FLAGS_name);
        if (!FLAGS_desc.empty()) request.set_description(FLAGS_desc);

        stub.UpdatePermission(&cntl, &request, &response, NULL);
    } else if (FLAGS_op == "list_roles") {
        siqi::auth::ListRolesRequest request;
        request.set_app_code(FLAGS_app);
        siqi::auth::ListRolesResponse list_response;
        
        stub.ListRoles(&cntl, &request, &list_response, NULL);
        if (!cntl.Failed()) {
            std::cout << "Roles (" << list_response.total() << "):" << std::endl;
            for (const auto& role : list_response.roles()) {
                std::cout << " - " << role.role_name() << " [" << role.role_key() << "] " 
                          << (role.is_default() ? "(Default) " : "") 
                          << ": " << role.description() << std::endl;
            }
            return 0;
        }
    } else if (FLAGS_op == "list_perms") {
        siqi::auth::ListPermissionsRequest request;
        request.set_app_code(FLAGS_app);
        siqi::auth::ListPermissionsResponse list_response;
        
        stub.ListPermissions(&cntl, &request, &list_response, NULL);
        if (!cntl.Failed()) {
            std::cout << "Permissions (" << list_response.total() << "):" << std::endl;
            for (const auto& perm : list_response.permissions()) {
                std::cout << " - " << perm.perm_name() << " [" << perm.perm_key() << "]: " 
                          << perm.description() << std::endl;
            }
            return 0;
        }
    } else {
        std::cerr << "Unknown operation: " << FLAGS_op << ". Use --help for usage." << std::endl;
        return -1;
    }

    if (!cntl.Failed()) {
        if (response.success()) {
            std::cout << "✅ 操作成功: " << response.message() << std::endl;
        } else {
            std::cout << "❌ 操作失败: " << response.message() << " (Code: " << response.code() << ")" << std::endl;
        }
    } else {
        LOG(ERROR) << "RPC Error: " << cntl.ErrorText();
    }

    return 0;
}
