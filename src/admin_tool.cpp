#include <brpc/channel.h>
#include <gflags/gflags.h>
#include <iostream>
#include <fstream>
#include "auth.pb.h"

DEFINE_string(server, "127.0.0.1:8888", "服务器地址 (默认 127.0.0.1:8888)");
DEFINE_string(op, "", "操作指令: \n"
                      "    [角色管理] create_role, update_role, delete_role, list_roles\n"
                      "    [权限管理] create_perm, update_perm, delete_perm, list_perms\n"
                      "    [授权管理] grant_role, revoke_role, add_perm, remove_perm");
DEFINE_string(app, "qq_bot", "应用代号 (App Code)");
DEFINE_string(user, "", "用户 ID (User ID)");
DEFINE_string(role, "", "角色标识 Key (Role Key)");
DEFINE_string(perm, "", "权限标识 Key (Permission Key)");
DEFINE_string(name, "", "名称 (角色名或权限名)");
DEFINE_string(desc, "", "描述信息");
DEFINE_string(password, "", "登录密码");
DEFINE_bool(is_default, false, "是否为默认角色");

const char* TOKEN_FILE = ".auth_token";

std::string LoadToken() {
    std::ifstream ifs(TOKEN_FILE);
    if (!ifs.is_open()) return "";
    std::string token;
    std::getline(ifs, token);
    return token;
}

void SaveToken(const std::string& token) {
    std::ofstream ofs(TOKEN_FILE);
    if (ofs.is_open()) {
        ofs << token;
    }
}

int main(int argc, char* argv[]) {
    std::string usage_msg = 
        "\nSiqi Auth 管理工具 (Admin Tool)\n"
        "用途: 管理权限系统的角色、权限定义以及用户授权关系。\n\n"
        "常见用法示例:\n"
        "  1. 登录验证: ./admin_tool --op=login --user=admin --password=admin\n"
        "  2. 创建角色: ./admin_tool --op=create_role --role=admin --name=管理员 --desc=超级管理\n"
        "  3. 创建权限: ./admin_tool --op=create_perm --perm=user:del --name=删用户\n"
        "  4. 角色绑定: ./admin_tool --op=add_perm --role=admin --perm=user:del\n"
        "  5. 用户授权: ./admin_tool --op=grant_role --user=10086 --role=admin\n"
        "  6. 查看列表: ./admin_tool --op=list_roles\n\n"
        "提示: 若只想查看本工具的参数说明，请使用: ./admin_tool --helpon=admin_tool";
    
    gflags::SetUsageMessage(usage_msg);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    
    brpc::Channel channel;
    brpc::ChannelOptions options;
    options.protocol = "http"; // 强制使用 HTTP 协议，以便传递 Authorization Header
    options.timeout_ms = 2000;
    options.max_retry = 3;
    if (channel.Init(FLAGS_server.c_str(), &options) != 0) {
        LOG(ERROR) << "Fail to initialize channel";
        return -1;
    }

    siqi::auth::AdminService_Stub stub(&channel);
    siqi::auth::AdminResponse response;
    brpc::Controller cntl;
    
    // Load Token and set header for all requests except login
    if (FLAGS_op != "login") {
        std::string token = LoadToken();
        if (!token.empty()) {
             // Bearer Token standard
            cntl.http_request().SetHeader("Authorization", "Bearer " + token);
        }
    }

    if (FLAGS_op == "login") {
        if (FLAGS_user.empty() || FLAGS_password.empty()) {
            std::cerr << "Missing --user or --password" << std::endl;
            return -1;
        }
        siqi::auth::LoginRequest request;
        request.set_username(FLAGS_user);
        request.set_password(FLAGS_password);
        siqi::auth::LoginResponse login_response;
        
        stub.Login(&cntl, &request, &login_response, NULL);
        
        if (!cntl.Failed()) {
            if (login_response.success()) {
                std::cout << "✅ 登录成功! Token: " << login_response.token() << std::endl;
                SaveToken(login_response.token());
                std::cout << "Token 已保存至 " << TOKEN_FILE << std::endl;
            } else {
                std::cout << "❌ 登录失败: " << login_response.message() << std::endl;
            }
            return 0;
        }
    } else if (FLAGS_op == "grant_role") {
        if (FLAGS_user.empty() || FLAGS_role.empty()) {
            std::cerr << "Missing --user or --role" << std::endl;
            return -1;
        }
        siqi::auth::GrantRoleToUserRequest request;
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
        request.set_app_code(FLAGS_app);
        request.set_role_key(FLAGS_role);
        request.set_perm_key(FLAGS_perm);
        
        stub.AddPermissionToRole(&cntl, &request, &response, NULL);
    } else if (FLAGS_op == "remove_perm") {
         if (FLAGS_role.empty() || FLAGS_perm.empty()) {
            std::cerr << "Missing --role or --perm" << std::endl;
            return -1;
        }
        siqi::auth::RemovePermissionFromRoleRequest request;
        request.set_app_code(FLAGS_app);
        request.set_role_key(FLAGS_role);
        request.set_perm_key(FLAGS_perm);
        
        stub.RemovePermissionFromRole(&cntl, &request, &response, NULL);
    } else if (FLAGS_op == "create_role") {
        if (FLAGS_role.empty() || FLAGS_name.empty()) {
            std::cerr << "Missing --role or --name" << std::endl;
            return -1;
        }
        siqi::auth::CreateRoleRequest request;
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
        request.set_app_code(FLAGS_app);
        request.set_role_key(FLAGS_role);
        
        stub.DeleteRole(&cntl, &request, &response, NULL);
    } else if (FLAGS_op == "delete_perm") {
        if (FLAGS_perm.empty()) {
            std::cerr << "Missing --perm" << std::endl;
            return -1;
        }
        siqi::auth::DeletePermissionRequest request;
        request.set_app_code(FLAGS_app);
        request.set_perm_key(FLAGS_perm);
        
        stub.DeletePermission(&cntl, &request, &response, NULL);
    } else if (FLAGS_op == "update_role") {
        if (FLAGS_role.empty()) {
            std::cerr << "Missing --role" << std::endl;
            return -1;
        }
        siqi::auth::UpdateRoleRequest request;
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
