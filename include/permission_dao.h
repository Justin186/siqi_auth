#ifndef PERMISSION_DAO_H
#define PERMISSION_DAO_H

#include <string>
#include <vector>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <mysql_driver.h>
#include <mysql_connection.h>

class PermissionDAO {
private:
    struct DBConfig {
        std::string host;
        int port;
        std::string user;
        std::string password;
        std::string database;
    } config_;

    std::queue<sql::Connection*> connection_pool_;
    std::mutex pool_mutex_;
    std::condition_variable pool_cond_;
    size_t initial_pool_size_ = 5;
    size_t max_pool_size_ = 50;
    size_t current_pool_size_ = 0;

    std::string last_error_;
    mutable std::mutex error_mutex_; // 保护 last_error

    sql::Connection* getConnection();
    void releaseConnection(sql::Connection* conn);
    sql::Connection* createConnection();

public:
    // 构造函数
    PermissionDAO(const std::string& host,
                  int port,
                  const std::string& user,
                  const std::string& password,
                  const std::string& database);
    
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
    struct AppInfo {
        int64_t id;
        std::string app_name;
        std::string app_code;
        std::string app_secret;
        std::string description;
        int32_t status;
        std::string created_at;
        std::string updated_at;
    };

    bool createApp(const std::string& app_name,
                   const std::string& app_code,
                   const std::string& description,
                   std::string& out_app_secret);

    bool updateApp(const std::string& app_code,
                   const std::string* app_name,
                   const std::string* description,
                   const int32_t* status);

    bool deleteApp(const std::string& app_code);

    bool getApp(const std::string& app_code, AppInfo& out_app);

    std::vector<AppInfo> listApps(int32_t page, int32_t page_size,
                                  const std::string* app_name,
                                  const int32_t* status,
                                  int64_t& out_total);

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
    int64_t getAppId(const std::string& app_code);
    
    // RAII 风格的连接守卫，作用域结束自动归还连接
    class ConnectionGuard {
    public:
        ConnectionGuard(PermissionDAO* dao) : dao_(dao) {
            conn_ = dao_->getConnection();
        }
        ~ConnectionGuard() {
            if (conn_) dao_->releaseConnection(conn_);
        }
        sql::Connection* operator->() { return conn_; }
        sql::Connection* get() { return conn_; }
        bool isValid() { return conn_ != nullptr; }
    private:
        PermissionDAO* dao_;
        sql::Connection* conn_;
    };
};

#endif // PERMISSION_DAO_H