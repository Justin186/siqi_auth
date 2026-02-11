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
    bool assignRoleToUser(const std::string& app_code,
                          const std::string& user_id,
                          const std::string& role_key);
    
    bool removeRoleFromUser(const std::string& app_code,
                            const std::string& user_id,
                            const std::string& role_key);
    
    // 状态检查
    bool isConnected() const;
    std::string getLastError() const;
    
private:
    // 内部辅助方法
    bool reconnectIfNeeded();
    int64_t getAppId(const std::string& app_code);
};

#endif // PERMISSION_DAO_H