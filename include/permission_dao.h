#ifndef PERMISSION_DAO_H
#define PERMISSION_DAO_H

#include <string>
#include <vector>
#include <memory>
#include <mysql_driver.h>
#include <mysql_connection.h>

class PermissionDAO {
private:
    std::unique_ptr<sql::Connection> connection_;
    std::string last_error_;
    
public:
    // 构造函数
    PermissionDAO(const std::string& host = "localhost",
                  const std::string& user = "siqi_dev",
                  const std::string& password = "siqi123",
                  const std::string& database = "siqi_auth");
    
    // 析构函数
    ~PermissionDAO();
    
    // 检查单个权限
    bool checkPermission(const std::string& app_code,
                         const std::string& user_id,
                         const std::string& perm_key,
                         const std::string& resource_id = "");
    
    // 批量检查权限
    std::vector<bool> batchCheckPermissions(
        const std::string& app_code,
        const std::vector<std::tuple<std::string, std::string>>& requests);  // (user_id, perm_key)
    
    // 获取用户所有权限
    std::vector<std::pair<std::string, std::string>> 
    getUserPermissions(const std::string& app_code,
                       const std::string& user_id);
    
    // 获取用户角色
    std::vector<std::string> getUserRoles(const std::string& app_code,
                                          const std::string& user_id);
    
    // 管理接口（根据需要添加）
    bool createRole(const std::string& app_code,
                    const std::string& role_name,
                    const std::string& role_key,
                    const std::string& description,
                    bool is_default = false);

    bool createPermission(const std::string& app_code,
                          const std::string& perm_name,
                          const std::string& perm_key,
                          const std::string& description);

    bool updateRole(const std::string& app_code,
                    const std::string& role_key,
                    const std::string* role_name,
                    const std::string* description,
                    const bool* is_default);

    bool updatePermission(const std::string& app_code,
                          const std::string& perm_key,
                          const std::string* perm_name,
                          const std::string* description);

    bool deleteRole(const std::string& app_code, const std::string& role_key);
    bool deletePermission(const std::string& app_code, const std::string& perm_key);

    struct RoleInfo {
        int64_t id;
        std::string role_name;
        std::string role_key;
        std::string description;
        bool is_default;
    };
    std::vector<RoleInfo> listRoles(const std::string& app_code);

    struct PermInfo {
        int64_t id;
        std::string perm_name;
        std::string perm_key;
        std::string description;
    };
    std::vector<PermInfo> listPermissions(const std::string& app_code);

    bool assignRoleToUser(const std::string& app_code,
                          const std::string& user_id,
                          const std::string& role_key);
    
    bool removeRoleFromUser(const std::string& app_code,
                            const std::string& user_id,
                            const std::string& role_key);

    // 角色-权限管理
    bool addPermissionToRole(const std::string& app_code,
                             const std::string& role_key,
                             const std::string& perm_key);
                             
    bool removePermissionFromRole(const std::string& app_code,
                                  const std::string& role_key,
                                  const std::string& perm_key);

    // 审计日志
    bool createAuditLog(int64_t operator_id, 
                        const std::string& operator_name,
                        const std::string& app_code,
                        const std::string& action,
                        const std::string& target_type,
                        const std::string& target_id,
                        const std::string& target_name = "",
                        const std::string& object_type = "",
                        const std::string& object_id = "",
                        const std::string& object_name = "");
    
    struct ConsoleUser {
        int64_t id = -1;
        std::string username;
        std::string password_hash;
        std::string real_name;
    };

    // 获取控制台用户详情
    ConsoleUser getConsoleUser(const std::string& username);
    
    // Legacy support (to be removed) - now calls getConsoleUser
    std::string getConsoleUserHash(const std::string& username);

    // 状态检查
    bool isConnected() const;
    std::string getLastError() const;
    
private:
    // 内部辅助方法
    bool reconnectIfNeeded();
    int64_t getAppId(const std::string& app_code);
};

#endif // PERMISSION_DAO_H