#include "permission_dao.h"
#include <cppconn/prepared_statement.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <iostream>
#include <chrono>
#include <sstream>

PermissionDAO::PermissionDAO(const std::string& host,
                             int port,
                             const std::string& user,
                             const std::string& password,
                             const std::string& database) {
    config_.host = host;
    config_.port = port;
    config_.user = user;
    config_.password = password;
    config_.database = database;

    // 初始化连接池
    for (size_t i = 0; i < initial_pool_size_; ++i) {
        sql::Connection* conn = createConnection();
        if (conn) {
            connection_pool_.push(conn);
            current_pool_size_++;
        }
    }
}

PermissionDAO::~PermissionDAO() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    while (!connection_pool_.empty()) {
        sql::Connection* conn = connection_pool_.front();
        connection_pool_.pop();
        delete conn;
    }
}

sql::Connection* PermissionDAO::createConnection() {
    try {
        sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
        std::string url = "tcp://" + config_.host + ":" + std::to_string(config_.port);
        sql::Connection* conn = driver->connect(url, config_.user, config_.password);
        conn->setSchema(config_.database);
        return conn;
    } catch (const sql::SQLException& e) {
        std::lock_guard<std::mutex> lock(error_mutex_);
        last_error_ = "创建连接失败: " + std::string(e.what());
        std::cerr << last_error_ << std::endl;
        return nullptr;
    }
}

sql::Connection* PermissionDAO::getConnection() {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    
    // 如果没有可用连接，且未达最大上限，创建新连接
    if (connection_pool_.empty() && current_pool_size_ < max_pool_size_) {
        // 创建连接比较耗时，先释放锁
        current_pool_size_++; // 先占位
        lock.unlock();
        
        sql::Connection* new_conn = createConnection();
        if (new_conn) {
             return new_conn;
        } else {
             // 创建失败，回退计数
             lock.lock();
             current_pool_size_--;
             return nullptr;
        }
    }

    // 等待可用连接
    while (connection_pool_.empty()) {
        // 等待 1 秒
        if (pool_cond_.wait_for(lock, std::chrono::seconds(1)) == std::cv_status::timeout) {
            std::cerr << "等待数据库连接超时" << std::endl;
            return nullptr;
        }
    }

    sql::Connection* conn = connection_pool_.front();
    connection_pool_.pop();
    
    // 简单检查连接有效性，失效则重连
    if (conn->isClosed()) {
        delete conn;
        conn = nullptr;
        // 尝试重连一次 (RAII style in createConnection would be nice, but simple here)
        // 注意这里持有锁，所以我们调用 createConnection (它自己不加pool锁)
        // 但 createConnection 可能很慢，简单起见我们这里如果断了就... 实际上 driver->connect 是慢操作
        // 理想做法是 unlock -> create -> return. 
        // 简化处理：既然无效了，就当前线程自己负责造一个新的
        lock.unlock();
        conn = createConnection();
        if (!conn) {
            lock.lock();
            current_pool_size_--; // 彻底失败
        }
        return conn;
    }

    return conn;
}

void PermissionDAO::releaseConnection(sql::Connection* conn) {
    if (!conn) return;
    std::lock_guard<std::mutex> lock(pool_mutex_);
    connection_pool_.push(conn);
    pool_cond_.notify_one();
}

bool PermissionDAO::isConnected() const {
    // 只要池子里有连接或者能创建连接就算连通，这里简单返回 true，具体的 valid 在 getConnection 里做
    return true; 
}

std::string PermissionDAO::getLastError() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

bool PermissionDAO::checkPermission(const std::string& app_code,
                                   const std::string& user_id,
                                   const std::string& perm_key,
                                   const std::string& resource_id) {
    ConnectionGuard conn(this);
    if (!conn.isValid()) return false;
    
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement(
                "SELECT COUNT(*) as cnt "
                "FROM sys_user_roles ur "
                "JOIN sys_apps a ON ur.app_id = a.id "
                "JOIN sys_role_permissions rp ON ur.role_id = rp.role_id "
                "JOIN sys_permissions p ON rp.perm_id = p.id "
                "WHERE a.app_code = ? "
                "  AND ur.app_user_id = ? "
                "  AND p.perm_key = ? "
                "  AND a.status = 1"
            )
        );
        
        pstmt->setString(1, app_code);
        pstmt->setString(2, user_id);
        pstmt->setString(3, perm_key);
        
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        
        if (res->next()) {
            return res->getInt("cnt") > 0;
        }
        
        return false;
        
    } catch (const sql::SQLException& e) {
        std::lock_guard<std::mutex> lock(error_mutex_);
        last_error_ = "查询权限失败: " + std::string(e.what());
        std::cerr << last_error_ << std::endl;
        return false;
    }
}

std::vector<bool> PermissionDAO::batchCheckPermissions(
        const std::string& app_code,
        const std::vector<std::tuple<std::string, std::string>>& requests) {
    std::vector<bool> results;
    results.reserve(requests.size());
    for (const auto& req : requests) {
        results.push_back(checkPermission(app_code, std::get<0>(req), std::get<1>(req)));
    }
    return results;
}

std::vector<std::string> PermissionDAO::getUserRoles(const std::string& app_code,
                                          const std::string& user_id) {
    std::vector<std::string> roles;
    ConnectionGuard conn(this);
    if (!conn.isValid()) return roles;

    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement(
                "SELECT r.role_key "
                "FROM sys_user_roles ur "
                "JOIN sys_apps a ON ur.app_id = a.id "
                "JOIN sys_roles r ON ur.role_id = r.id "
                "WHERE a.app_code = ? "
                "  AND ur.app_user_id = ? "
                "  AND a.status = 1"
            )
        );
        
        pstmt->setString(1, app_code);
        pstmt->setString(2, user_id);
        
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        
        while (res->next()) {
            roles.push_back(res->getString("role_key"));
        }
        
    } catch (const sql::SQLException& e) {
        std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "获取角色失败: " + std::string(e.what());
        std::cerr << last_error_ << std::endl;
    }
    return roles;
}

std::vector<std::pair<std::string, std::string>> 
PermissionDAO::getUserPermissions(const std::string& app_code,
                       const std::string& user_id) {
    std::vector<std::pair<std::string, std::string>> perms;
    ConnectionGuard conn(this); if (!conn.isValid()) return perms;

    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement(
                "SELECT p.perm_key, p.perm_name "
                "FROM sys_user_roles ur "
                "JOIN sys_apps a ON ur.app_id = a.id "
                "JOIN sys_role_permissions rp ON ur.role_id = rp.role_id "
                "JOIN sys_permissions p ON rp.perm_id = p.id "
                "WHERE a.app_code = ? AND ur.app_user_id = ?"
            )
        );
        pstmt->setString(1, app_code);
        pstmt->setString(2, user_id);
        
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        while (res->next()) {
            perms.emplace_back(res->getString("perm_key"), res->getString("perm_name"));
        }
    } catch (const sql::SQLException& e) {
        std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "获取用户权限失败: " + std::string(e.what());
    }
    return perms;
}

bool PermissionDAO::createApp(const std::string& app_name,
                              const std::string& app_code,
                              const std::string& description,
                              std::string& out_app_secret) {
    ConnectionGuard conn(this); if (!conn.isValid()) return false;
    try {
        // Generate a simple random secret
        std::string secret = "secret_" + app_code + "_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        out_app_secret = secret;

        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement(
                "INSERT INTO sys_apps (app_name, app_code, app_secret, description, status) "
                "VALUES (?, ?, ?, ?, 1)"
            )
        );
        pstmt->setString(1, app_name);
        pstmt->setString(2, app_code);
        pstmt->setString(3, secret);
        pstmt->setString(4, description);
        
        pstmt->executeUpdate();
        return true;
    } catch (const sql::SQLException& e) {
        std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "创建应用失败: " + std::string(e.what());
        return false;
    }
}

bool PermissionDAO::updateApp(const std::string& app_code,
                              const std::string* app_name,
                              const std::string* description,
                              const int32_t* status) {
    ConnectionGuard conn(this); if (!conn.isValid()) return false;
    try {
        std::string query = "UPDATE sys_apps SET ";
        std::vector<std::string> params;
        std::vector<int> param_types; // 0: string, 1: int
        std::vector<int32_t> int_params;

        if (app_name) {
            query += "app_name = ?, ";
            params.push_back(*app_name);
            param_types.push_back(0);
        }
        if (description) {
            query += "description = ?, ";
            params.push_back(*description);
            param_types.push_back(0);
        }
        if (status) {
            query += "status = ?, ";
            int_params.push_back(*status);
            param_types.push_back(1);
        }

        if (params.empty() && int_params.empty()) return true; // Nothing to update

        query.pop_back(); query.pop_back(); // Remove last ", "
        query += " WHERE app_code = ?";

        std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(query));
        
        int param_idx = 1;
        int str_idx = 0;
        int int_idx = 0;
        for (int type : param_types) {
            if (type == 0) {
                pstmt->setString(param_idx++, params[str_idx++]);
            } else {
                pstmt->setInt(param_idx++, int_params[int_idx++]);
            }
        }
        pstmt->setString(param_idx, app_code);

        int rows = pstmt->executeUpdate();
        return rows > 0;
    } catch (const sql::SQLException& e) {
        std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "更新应用失败: " + std::string(e.what());
        return false;
    }
}

bool PermissionDAO::deleteApp(const std::string& app_code) {
    ConnectionGuard conn(this); if (!conn.isValid()) return false;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement("DELETE FROM sys_apps WHERE app_code = ?")
        );
        pstmt->setString(1, app_code);
        int rows = pstmt->executeUpdate();
        return rows > 0;
    } catch (const sql::SQLException& e) {
        std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "删除应用失败: " + std::string(e.what());
        return false;
    }
}

bool PermissionDAO::getApp(const std::string& app_code, AppInfo& out_app) {
    ConnectionGuard conn(this); if (!conn.isValid()) return false;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement(
                "SELECT id, app_name, app_code, app_secret, description, status, created_at, updated_at "
                "FROM sys_apps WHERE app_code = ?"
            )
        );
        pstmt->setString(1, app_code);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res->next()) {
            out_app.id = res->getInt64("id");
            out_app.app_name = res->getString("app_name");
            out_app.app_code = res->getString("app_code");
            out_app.app_secret = res->getString("app_secret");
            out_app.description = res->getString("description");
            out_app.status = res->getInt("status");
            out_app.created_at = res->getString("created_at");
            out_app.updated_at = res->getString("updated_at");
            return true;
        }
        return false;
    } catch (const sql::SQLException& e) {
        std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "获取应用失败: " + std::string(e.what());
        return false;
    }
}

std::vector<PermissionDAO::AppInfo> PermissionDAO::listApps(int32_t page, int32_t page_size,
                                                            const std::string* app_name,
                                                            const int32_t* status,
                                                            int64_t& out_total) {
    std::vector<AppInfo> apps;
    out_total = 0;
    ConnectionGuard conn(this); if (!conn.isValid()) return apps;

    try {
        std::string count_query = "SELECT COUNT(*) as cnt FROM sys_apps WHERE 1=1";
        std::string data_query = "SELECT id, app_name, app_code, app_secret, description, status, created_at, updated_at FROM sys_apps WHERE 1=1";
        
        if (app_name) {
            count_query += " AND app_name LIKE ?";
            data_query += " AND app_name LIKE ?";
        }
        if (status) {
            count_query += " AND status = ?";
            data_query += " AND status = ?";
        }
        
        data_query += " ORDER BY id DESC LIMIT ? OFFSET ?";

        // Count
        {
            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(count_query));
            int param_idx = 1;
            if (app_name) pstmt->setString(param_idx++, "%" + *app_name + "%");
            if (status) pstmt->setInt(param_idx++, *status);
            
            std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
            if (res->next()) {
                out_total = res->getInt64("cnt");
            }
        }

        // Data
        {
            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(data_query));
            int param_idx = 1;
            if (app_name) pstmt->setString(param_idx++, "%" + *app_name + "%");
            if (status) pstmt->setInt(param_idx++, *status);
            
            pstmt->setInt(param_idx++, page_size);
            pstmt->setInt(param_idx++, (page - 1) * page_size);
            
            std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
            while (res->next()) {
                AppInfo app;
                app.id = res->getInt64("id");
                app.app_name = res->getString("app_name");
                app.app_code = res->getString("app_code");
                app.app_secret = res->getString("app_secret");
                app.description = res->getString("description");
                app.status = res->getInt("status");
                app.created_at = res->getString("created_at");
                app.updated_at = res->getString("updated_at");
                apps.push_back(app);
            }
        }
    } catch (const sql::SQLException& e) {
        std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "获取应用列表失败: " + std::string(e.what());
    }
    return apps;
}

int64_t PermissionDAO::getAppId(const std::string& app_code) {
    ConnectionGuard conn(this); if (!conn.isValid()) return -1;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement("SELECT id FROM sys_apps WHERE app_code = ?")
        );
        pstmt->setString(1, app_code);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res->next()) {
            return res->getInt64("id");
        }
    } catch (...) {}
    return -1;
}

bool PermissionDAO::createRole(const std::string& app_code,
                               const std::string& role_name,
                               const std::string& role_key,
                               const std::string& description,
                               bool is_default) {
    ConnectionGuard conn(this); if (!conn.isValid()) return false;
    try {
        int64_t app_id = getAppId(app_code);
        if (app_id == -1) {
            std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "应用不存在: " + app_code;
            return false;
        }

        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement(
                "INSERT INTO sys_roles (app_id, role_name, role_key, description, is_default) "
                "VALUES (?, ?, ?, ?, ?)"
            )
        );
        pstmt->setInt64(1, app_id);
        pstmt->setString(2, role_name);
        pstmt->setString(3, role_key);
        pstmt->setString(4, description);
        pstmt->setInt(5, is_default ? 1 : 0);
        
        pstmt->executeUpdate();
        return true;
    } catch (const sql::SQLException& e) {
        std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "创建角色失败: " + std::string(e.what());
        return false;
    }
}

bool PermissionDAO::createPermission(const std::string& app_code,
                                     const std::string& perm_name,
                                     const std::string& perm_key,
                                     const std::string& description) {
    ConnectionGuard conn(this); if (!conn.isValid()) return false;
    try {
        int64_t app_id = getAppId(app_code);
        if (app_id == -1) {
            std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "应用不存在: " + app_code;
            return false;
        }

        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement(
                "INSERT INTO sys_permissions (app_id, perm_name, perm_key, description) "
                "VALUES (?, ?, ?, ?)"
            )
        );
        pstmt->setInt64(1, app_id);
        pstmt->setString(2, perm_name);
        pstmt->setString(3, perm_key);
        pstmt->setString(4, description);
        
        pstmt->executeUpdate();
        return true;
    } catch (const sql::SQLException& e) {
        std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "创建权限失败: " + std::string(e.what());
        return false;
    }
}

bool PermissionDAO::assignRoleToUser(const std::string& app_code,
                          const std::string& user_id,
                          const std::string& role_key) {
    ConnectionGuard conn(this); if (!conn.isValid()) return false;
    try {
        // 1. Get role_id
        std::unique_ptr<sql::PreparedStatement> pstmt_role(
            conn->prepareStatement(
                "SELECT r.id, r.app_id FROM sys_roles r JOIN sys_apps a ON r.app_id = a.id "
                "WHERE a.app_code = ? AND r.role_key = ?"
            )
        );
        pstmt_role->setString(1, app_code);
        pstmt_role->setString(2, role_key);
        std::unique_ptr<sql::ResultSet> res(pstmt_role->executeQuery());
        
        if (!res->next()) {
            std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "角色不存在";
            return false;
        }
        int64_t role_id = res->getInt64("id");
        int64_t app_id = res->getInt64("app_id");

        // 2. Insert mapping
        std::unique_ptr<sql::PreparedStatement> pstmt_insert(
            conn->prepareStatement(
                "INSERT INTO sys_user_roles (app_id, app_user_id, role_id) VALUES (?, ?, ?)"
            )
        );
        pstmt_insert->setInt64(1, app_id);
        pstmt_insert->setString(2, user_id);
        pstmt_insert->setInt64(3, role_id);
        
        pstmt_insert->executeUpdate();
        return true;
    } catch (const sql::SQLException& e) {
        std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "授权失败: " + std::string(e.what());
        return false;
    }
}

bool PermissionDAO::removeRoleFromUser(const std::string& app_code,
                          const std::string& user_id,
                          const std::string& role_key) {
    ConnectionGuard conn(this); if (!conn.isValid()) return false;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement(
                "DELETE ur FROM sys_user_roles ur "
                "JOIN sys_apps a ON ur.app_id = a.id "
                "JOIN sys_roles r ON ur.role_id = r.id "
                "WHERE a.app_code = ? AND ur.app_user_id = ? AND r.role_key = ?"
            )
        );
        pstmt->setString(1, app_code);
        pstmt->setString(2, user_id);
        pstmt->setString(3, role_key);
        
        int rows = pstmt->executeUpdate();
        return rows > 0;
    } catch (const sql::SQLException& e) {
        std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "移除权限失败: " + std::string(e.what());
        return false;
    }
}

bool PermissionDAO::addPermissionToRole(const std::string& app_code,
                                        const std::string& role_key,
                                        const std::string& perm_key) {
    ConnectionGuard conn(this); if (!conn.isValid()) return false;
    try {
        // SQL lookups for IDs might be complex, simplified by subqueries or separate lookups.
        // Let's do separate lookups for safety and clarity.
        
        // 1. Get Role ID
        int64_t role_id = -1;
        {
            std::unique_ptr<sql::PreparedStatement> estmt(conn->prepareStatement(
                "SELECT r.id FROM sys_roles r JOIN sys_apps a ON r.app_id = a.id WHERE a.app_code = ? AND r.role_key = ?"
            ));
            estmt->setString(1, app_code);
            estmt->setString(2, role_key);
            std::unique_ptr<sql::ResultSet> res(estmt->executeQuery());
            if (res->next()) role_id = res->getInt64("id");
            else { std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "角色不存在"; return false; }
        }

        // 2. Get Permission ID
        int64_t perm_id = -1;
        {
            std::unique_ptr<sql::PreparedStatement> estmt(conn->prepareStatement(
                "SELECT p.id FROM sys_permissions p JOIN sys_apps a ON p.app_id = a.id WHERE a.app_code = ? AND p.perm_key = ?"
            ));
            estmt->setString(1, app_code);
            estmt->setString(2, perm_key);
            std::unique_ptr<sql::ResultSet> res(estmt->executeQuery());
            if (res->next()) perm_id = res->getInt64("id");
            else { std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "权限不存在"; return false; }
        }

        // 3. Insert
        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement("INSERT INTO sys_role_permissions (role_id, perm_id) VALUES (?, ?)")
        );
        pstmt->setInt64(1, role_id);
        pstmt->setInt64(2, perm_id);
        pstmt->executeUpdate();
        return true;
    } catch (const sql::SQLException& e) {
        std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "添加角色权限失败: " + std::string(e.what());
        return false;
    }
}

bool PermissionDAO::removePermissionFromRole(const std::string& app_code,
                                             const std::string& role_key,
                                             const std::string& perm_key) {
    ConnectionGuard conn(this); if (!conn.isValid()) return false;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement(
                "DELETE rp FROM sys_role_permissions rp "
                "JOIN sys_roles r ON rp.role_id = r.id "
                "JOIN sys_permissions p ON rp.perm_id = p.id "
                "JOIN sys_apps a ON r.app_id = a.id "
                "WHERE a.app_code = ? AND r.role_key = ? AND p.perm_key = ?"
            )
        );
        pstmt->setString(1, app_code);
        pstmt->setString(2, role_key);
        pstmt->setString(3, perm_key);
        
        int rows = pstmt->executeUpdate();
        return rows > 0;
    } catch (const sql::SQLException& e) {
        std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "移除角色权限失败: " + std::string(e.what());
        return false;
    }
}

bool PermissionDAO::createAuditLog(int64_t operator_id, 
                                   const std::string& operator_name,
                                   const std::string& app_code,
                                   const std::string& action,
                                   const std::string& target_type,
                                   const std::string& target_id,
                                   const std::string& target_name,
                                   const std::string& object_type,
                                   const std::string& object_id,
                                   const std::string& object_name) {
    ConnectionGuard conn(this); if (!conn.isValid()) return false;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement(
                "INSERT INTO sys_audit_logs "
                "(operator_id, operator_name, app_code, action, target_type, target_id, target_name, object_type, object_id, object_name) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
            )
        );
        pstmt->setInt64(1, operator_id);
        pstmt->setString(2, operator_name);
        pstmt->setString(3, app_code);
        pstmt->setString(4, action);
        pstmt->setString(5, target_type);
        pstmt->setString(6, target_id);
        pstmt->setString(7, target_name);
        pstmt->setString(8, object_type);
        pstmt->setString(9, object_id);
        pstmt->setString(10, object_name);
        
        pstmt->executeUpdate();
        return true;
    } catch (const sql::SQLException& e) {
        // 审计日志失败最好记录到本地日志
        std::cerr << "审计日志记录失败: " << e.what() << std::endl;
        return false;
    }
}

bool PermissionDAO::deleteRole(const std::string& app_code, const std::string& role_key) {
    ConnectionGuard conn(this); if (!conn.isValid()) return false;
    try {
        int64_t app_id = getAppId(app_code);
        if (app_id == -1) {
            std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "应用不存在: " + app_code;
            return false;
        }

        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement(
                "DELETE FROM sys_roles WHERE app_id = ? AND role_key = ?"
            )
        );
        pstmt->setInt64(1, app_id);
        pstmt->setString(2, role_key);
        
        int rows = pstmt->executeUpdate();
        if (rows == 0) {
            std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "角色不存在或已删除";
            return false;
        }
        return true;
    } catch (const sql::SQLException& e) {
        std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "删除角色失败: " + std::string(e.what());
        return false;
    }
}

bool PermissionDAO::deletePermission(const std::string& app_code, const std::string& perm_key) {
    ConnectionGuard conn(this); if (!conn.isValid()) return false;
    try {
        int64_t app_id = getAppId(app_code);
        if (app_id == -1) {
            std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "应用不存在: " + app_code;
            return false;
        }

        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement(
                "DELETE FROM sys_permissions WHERE app_id = ? AND perm_key = ?"
            )
        );
        pstmt->setInt64(1, app_id);
        pstmt->setString(2, perm_key);
        
        int rows = pstmt->executeUpdate();
        if (rows == 0) {
            std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "权限不存在或已删除";
            return false;
        }
        return true;
    } catch (const sql::SQLException& e) {
        std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "删除权限失败: " + std::string(e.what());
        return false;
    }
}

std::vector<PermissionDAO::RoleInfo> PermissionDAO::listRoles(const std::string& app_code) {
    std::vector<RoleInfo> roles;
    ConnectionGuard conn(this); if (!conn.isValid()) return roles;
    try {
        int64_t app_id = getAppId(app_code);
        if (app_id == -1) return roles;

        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement(
                "SELECT id, role_name, role_key, description, is_default FROM sys_roles WHERE app_id = ?"
            )
        );
        pstmt->setInt64(1, app_id);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        
        while (res->next()) {
            RoleInfo role;
            role.id = res->getInt64("id");
            role.role_name = res->getString("role_name");
            role.role_key = res->getString("role_key");
            role.description = res->getString("description");
            role.is_default = res->getBoolean("is_default");
            roles.push_back(role);
        }
    } catch (const sql::SQLException& e) {
        std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "查询角色列表失败: " + std::string(e.what());
    }
    return roles;
}

std::vector<PermissionDAO::PermInfo> PermissionDAO::listPermissions(const std::string& app_code) {
    std::vector<PermInfo> perms;
    ConnectionGuard conn(this); if (!conn.isValid()) return perms;
    try {
        int64_t app_id = getAppId(app_code);
        if (app_id == -1) return perms;

        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement(
                "SELECT id, perm_name, perm_key, description FROM sys_permissions WHERE app_id = ?"
            )
        );
        pstmt->setInt64(1, app_id);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        
        while (res->next()) {
            PermInfo perm;
            perm.id = res->getInt64("id");
            perm.perm_name = res->getString("perm_name");
            perm.perm_key = res->getString("perm_key");
            perm.description = res->getString("description");
            perms.push_back(perm);
        }
    } catch (const sql::SQLException& e) {
        std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "查询权限列表失败: " + std::string(e.what());
    }
    return perms;
}

bool PermissionDAO::updateRole(const std::string& app_code,
                               const std::string& role_key,
                               const std::string* role_name,
                               const std::string* description,
                               const bool* is_default) {
    ConnectionGuard conn(this); if (!conn.isValid()) return false;
    try {
        int64_t app_id = getAppId(app_code);
        if (app_id == -1) {
            std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "应用不存在: " + app_code;
            return false;
        }

        std::string sql = "UPDATE sys_roles SET updated_at=NOW()";
        if (role_name) sql += ", role_name=?";
        if (description) sql += ", description=?";
        if (is_default) sql += ", is_default=?";
        sql += " WHERE app_id=? AND role_key=?";

        std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(sql));

        int idx = 1;
        if (role_name) pstmt->setString(idx++, *role_name);
        if (description) pstmt->setString(idx++, *description);
        if (is_default) pstmt->setInt(idx++, *is_default ? 1 : 0);
        pstmt->setInt64(idx++, app_id);
        pstmt->setString(idx++, role_key);

        int rows = pstmt->executeUpdate();
        // rows can be 0 if no change was made, but that's not strictly an error. 
        // We might want to check if the role actually exists though.
        // For simplicity, we assume success if no exception.
        
        return true;
    } catch (const sql::SQLException& e) {
        std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "更新角色失败: " + std::string(e.what());
        return false;
    }
}

bool PermissionDAO::updatePermission(const std::string& app_code,
                                     const std::string& perm_key,
                                     const std::string* perm_name,
                                     const std::string* description) {
    ConnectionGuard conn(this); if (!conn.isValid()) return false;
    try {
        int64_t app_id = getAppId(app_code);
        if (app_id == -1) {
            std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "应用不存在: " + app_code;
            return false;
        }

        std::string sql = "UPDATE sys_permissions SET updated_at=NOW()";
        if (perm_name) sql += ", perm_name=?";
        if (description) sql += ", description=?";
        sql += " WHERE app_id=? AND perm_key=?";

        std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(sql));

        int idx = 1;
        if (perm_name) pstmt->setString(idx++, *perm_name);
        if (description) pstmt->setString(idx++, *description);
        pstmt->setInt64(idx++, app_id);
        pstmt->setString(idx++, perm_key);

        pstmt->executeUpdate();
        return true;
    } catch (const sql::SQLException& e) {
        std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "更新权限失败: " + std::string(e.what());
        return false;
    }
}

PermissionDAO::ConsoleUser PermissionDAO::getConsoleUser(const std::string& username) {
    ConsoleUser user;
    ConnectionGuard conn(this); if (!conn.isValid()) return user;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement("SELECT id, username, password_hash, real_name FROM sys_console_users WHERE username = ?")
        );
        pstmt->setString(1, username);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res->next()) {
            user.id = res->getInt64("id");
            user.username = res->getString("username");
            user.password_hash = res->getString("password_hash");
            user.real_name = res->getString("real_name");
        }
    } catch (const sql::SQLException& e) {
        std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "查询用户失败: " + std::string(e.what());
    }
    return user;
}

std::string PermissionDAO::getConsoleUserHash(const std::string& username) {
    auto user = getConsoleUser(username);
    return user.password_hash;
}

std::vector<std::string> PermissionDAO::getRolePermissions(const std::string& app_code,
                                                           const std::string& role_key) {
    std::vector<std::string> perms;
    ConnectionGuard conn(this); if (!conn.isValid()) return perms;
    try {
        int64_t app_id = getAppId(app_code);
        if (app_id == -1) return perms;

        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement(
                "SELECT p.perm_key FROM sys_role_permissions rp "
                "JOIN sys_roles r ON rp.role_id = r.id "
                "JOIN sys_permissions p ON rp.perm_id = p.id "
                "WHERE r.app_id = ? AND r.role_key = ?"
            )
        );
        pstmt->setInt64(1, app_id);
        pstmt->setString(2, role_key);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        
        while (res->next()) {
            perms.push_back(res->getString("perm_key"));
        }
    } catch (const sql::SQLException& e) {
        std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "查询角色权限失败: " + std::string(e.what());
    }
    return perms;
}

std::vector<PermissionDAO::UserInfo> PermissionDAO::getRoleUsers(const std::string& app_code,
                                                                 const std::string& role_key,
                                                                 int32_t page, int32_t page_size,
                                                                 int64_t& out_total) {
    std::vector<UserInfo> users;
    out_total = 0;
    ConnectionGuard conn(this); if (!conn.isValid()) return users;
    try {
        int64_t app_id = getAppId(app_code);
        if (app_id == -1) return users;

        // Get total count
        std::unique_ptr<sql::PreparedStatement> count_stmt(
            conn->prepareStatement(
                "SELECT COUNT(*) as total FROM sys_user_roles ur "
                "JOIN sys_roles r ON ur.role_id = r.id "
                "WHERE r.app_id = ? AND r.role_key = ?"
            )
        );
        count_stmt->setInt64(1, app_id);
        count_stmt->setString(2, role_key);
        std::unique_ptr<sql::ResultSet> count_res(count_stmt->executeQuery());
        if (count_res->next()) {
            out_total = count_res->getInt64("total");
        }

        // Get paginated data
        int32_t offset = (page - 1) * page_size;
        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement(
                "SELECT ur.app_user_id, ur.created_at FROM sys_user_roles ur "
                "JOIN sys_roles r ON ur.role_id = r.id "
                "WHERE r.app_id = ? AND r.role_key = ? "
                "ORDER BY ur.created_at DESC LIMIT ? OFFSET ?"
            )
        );
        pstmt->setInt64(1, app_id);
        pstmt->setString(2, role_key);
        pstmt->setInt(3, page_size);
        pstmt->setInt(4, offset);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        
        while (res->next()) {
            UserInfo user;
            user.user_id = res->getString("app_user_id");
            user.created_at = res->getString("created_at");
            users.push_back(user);
        }
    } catch (const sql::SQLException& e) {
        std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "查询角色用户失败: " + std::string(e.what());
    }
    return users;
}

std::vector<PermissionDAO::UserRoleData> PermissionDAO::listUserRoles(const std::string& app_code,
                                                                      int32_t page, int32_t page_size,
                                                                      const std::string* user_id,
                                                                      int64_t& out_total) {
    std::vector<UserRoleData> users;
    out_total = 0;
    ConnectionGuard conn(this); if (!conn.isValid()) return users;
    try {
        int64_t app_id = getAppId(app_code);
        if (app_id == -1) return users;

        std::string base_sql = "FROM sys_user_roles WHERE app_id = ?";
        if (user_id && !user_id->empty()) {
            base_sql += " AND app_user_id = ?";
        }

        // Get total count (distinct users)
        std::unique_ptr<sql::PreparedStatement> count_stmt(
            conn->prepareStatement("SELECT COUNT(DISTINCT app_user_id) as total " + base_sql)
        );
        count_stmt->setInt64(1, app_id);
        if (user_id && !user_id->empty()) {
            count_stmt->setString(2, *user_id);
        }
        std::unique_ptr<sql::ResultSet> count_res(count_stmt->executeQuery());
        if (count_res->next()) {
            out_total = count_res->getInt64("total");
        }

        // Get paginated data
        int32_t offset = (page - 1) * page_size;
        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement(
                "SELECT ur.app_user_id, GROUP_CONCAT(r.role_key) as role_keys, MIN(ur.created_at) as created_at "
                "FROM sys_user_roles ur "
                "JOIN sys_roles r ON ur.role_id = r.id "
                "WHERE ur.app_id = ? " + 
                (user_id && !user_id->empty() ? std::string("AND ur.app_user_id = ? ") : std::string("")) +
                "GROUP BY ur.app_user_id "
                "ORDER BY created_at DESC LIMIT ? OFFSET ?"
            )
        );
        
        int idx = 1;
        pstmt->setInt64(idx++, app_id);
        if (user_id && !user_id->empty()) {
            pstmt->setString(idx++, *user_id);
        }
        pstmt->setInt(idx++, page_size);
        pstmt->setInt(idx++, offset);
        
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        
        while (res->next()) {
            UserRoleData data;
            data.user_id = res->getString("app_user_id");
            data.created_at = res->getString("created_at");
            
            std::string roles_str = res->getString("role_keys");
            std::stringstream ss(roles_str);
            std::string item;
            while (std::getline(ss, item, ',')) {
                data.role_keys.push_back(item);
            }
            users.push_back(data);
        }
    } catch (const sql::SQLException& e) {
        std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "查询用户角色列表失败: " + std::string(e.what());
    }
    return users;
}

std::vector<PermissionDAO::AuditLogInfo> PermissionDAO::listAuditLogs(int32_t page, int32_t page_size,
                                                                      const std::string* app_code,
                                                                      const std::string* action,
                                                                      const std::string* operator_id,
                                                                      const std::string* target_id,
                                                                      int64_t& out_total) {
    std::vector<AuditLogInfo> logs;
    out_total = 0;
    ConnectionGuard conn(this); if (!conn.isValid()) return logs;
    try {
        std::string base_sql = "FROM sys_audit_logs WHERE 1=1";
        if (app_code && !app_code->empty()) base_sql += " AND app_code = ?";
        if (action && !action->empty()) base_sql += " AND action = ?";
        if (operator_id && !operator_id->empty()) base_sql += " AND operator_id = ?";
        if (target_id && !target_id->empty()) base_sql += " AND target_id = ?";

        // Get total count
        std::unique_ptr<sql::PreparedStatement> count_stmt(
            conn->prepareStatement("SELECT COUNT(*) as total " + base_sql)
        );
        
        int idx = 1;
        if (app_code && !app_code->empty()) count_stmt->setString(idx++, *app_code);
        if (action && !action->empty()) count_stmt->setString(idx++, *action);
        if (operator_id && !operator_id->empty()) count_stmt->setString(idx++, *operator_id);
        if (target_id && !target_id->empty()) count_stmt->setString(idx++, *target_id);
        
        std::unique_ptr<sql::ResultSet> count_res(count_stmt->executeQuery());
        if (count_res->next()) {
            out_total = count_res->getInt64("total");
        }

        // Get paginated data
        int32_t offset = (page - 1) * page_size;
        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement(
                "SELECT id, operator_id, operator_name, app_code, action, "
                "target_type, target_id, target_name, object_type, object_id, object_name, created_at "
                + base_sql + " ORDER BY created_at DESC LIMIT ? OFFSET ?"
            )
        );
        
        idx = 1;
        if (app_code && !app_code->empty()) pstmt->setString(idx++, *app_code);
        if (action && !action->empty()) pstmt->setString(idx++, *action);
        if (operator_id && !operator_id->empty()) pstmt->setString(idx++, *operator_id);
        if (target_id && !target_id->empty()) pstmt->setString(idx++, *target_id);
        pstmt->setInt(idx++, page_size);
        pstmt->setInt(idx++, offset);
        
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        
        while (res->next()) {
            AuditLogInfo log;
            log.id = res->getInt64("id");
            log.operator_id = res->getInt64("operator_id");
            log.operator_name = res->getString("operator_name");
            log.app_code = res->getString("app_code");
            log.action = res->getString("action");
            log.target_type = res->getString("target_type");
            log.target_id = res->getString("target_id");
            log.target_name = res->getString("target_name");
            log.object_type = res->getString("object_type");
            log.object_id = res->getString("object_id");
            log.object_name = res->getString("object_name");
            log.created_at = res->getString("created_at");
            logs.push_back(log);
        }
    } catch (const sql::SQLException& e) {
        std::lock_guard<std::mutex> lock(error_mutex_); last_error_ = "查询审计日志失败: " + std::string(e.what());
    }
    return logs;
}
