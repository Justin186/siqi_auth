#include "permission_dao.h"
#include <cppconn/prepared_statement.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <iostream>

PermissionDAO::PermissionDAO(const std::string& host,
                             int port,
                             const std::string& user,
                             const std::string& password,
                             const std::string& database) {
    try {
        sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
        std::string url = "tcp://" + host + ":" + std::to_string(port);
        connection_.reset(driver->connect(url, user, password));
        connection_->setSchema(database);
        last_error_.clear();
    } catch (const sql::SQLException& e) {
        last_error_ = "连接数据库失败: " + std::string(e.what());
        std::cerr << last_error_ << std::endl;
    }
}

PermissionDAO::~PermissionDAO() {
    if (connection_) {
        try {
            connection_->close();
        } catch (...) {}
    }
}

bool PermissionDAO::isConnected() const {
    try {
        return connection_ && !connection_->isClosed() && connection_->isValid();
    } catch (...) {
        return false;
    }
}

std::string PermissionDAO::getLastError() const {
    return last_error_;
}

bool PermissionDAO::reconnectIfNeeded() {
    // Basic check. In a real app, store credentials and reconnect here.
    return isConnected();   
}

bool PermissionDAO::checkPermission(const std::string& app_code,
                                   const std::string& user_id,
                                   const std::string& perm_key,
                                   const std::string& resource_id) {
    if (!reconnectIfNeeded()) {
        return false;
    }
    
    try {
        // SQL query to check permission
        // Using a simpler query structure for reliability
        std::unique_ptr<sql::PreparedStatement> pstmt(
            connection_->prepareStatement(
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
    if (!isConnected()) return roles;

    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            connection_->prepareStatement(
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
        last_error_ = "获取角色失败: " + std::string(e.what());
        std::cerr << last_error_ << std::endl;
    }
    return roles;
}

std::vector<std::pair<std::string, std::string>> 
PermissionDAO::getUserPermissions(const std::string& app_code,
                       const std::string& user_id) {
    std::vector<std::pair<std::string, std::string>> perms;
    if (!reconnectIfNeeded()) return perms;

    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            connection_->prepareStatement(
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
        last_error_ = "获取用户权限失败: " + std::string(e.what());
    }
    return perms;
}

int64_t PermissionDAO::getAppId(const std::string& app_code) {
    if (!reconnectIfNeeded()) return -1;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            connection_->prepareStatement("SELECT id FROM sys_apps WHERE app_code = ?")
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
    if (!reconnectIfNeeded()) return false;
    try {
        int64_t app_id = getAppId(app_code);
        if (app_id == -1) {
            last_error_ = "应用不存在: " + app_code;
            return false;
        }

        std::unique_ptr<sql::PreparedStatement> pstmt(
            connection_->prepareStatement(
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
        last_error_ = "创建角色失败: " + std::string(e.what());
        return false;
    }
}

bool PermissionDAO::createPermission(const std::string& app_code,
                                     const std::string& perm_name,
                                     const std::string& perm_key,
                                     const std::string& description) {
    if (!reconnectIfNeeded()) return false;
    try {
        int64_t app_id = getAppId(app_code);
        if (app_id == -1) {
            last_error_ = "应用不存在: " + app_code;
            return false;
        }

        std::unique_ptr<sql::PreparedStatement> pstmt(
            connection_->prepareStatement(
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
        last_error_ = "创建权限失败: " + std::string(e.what());
        return false;
    }
}

bool PermissionDAO::assignRoleToUser(const std::string& app_code,
                          const std::string& user_id,
                          const std::string& role_key) {
    if (!reconnectIfNeeded()) return false;
    try {
        // 1. Get role_id
        std::unique_ptr<sql::PreparedStatement> pstmt_role(
            connection_->prepareStatement(
                "SELECT r.id, r.app_id FROM sys_roles r JOIN sys_apps a ON r.app_id = a.id "
                "WHERE a.app_code = ? AND r.role_key = ?"
            )
        );
        pstmt_role->setString(1, app_code);
        pstmt_role->setString(2, role_key);
        std::unique_ptr<sql::ResultSet> res(pstmt_role->executeQuery());
        
        if (!res->next()) {
            last_error_ = "角色不存在";
            return false;
        }
        int64_t role_id = res->getInt64("id");
        int64_t app_id = res->getInt64("app_id");

        // 2. Insert mapping
        std::unique_ptr<sql::PreparedStatement> pstmt_insert(
            connection_->prepareStatement(
                "INSERT INTO sys_user_roles (app_id, app_user_id, role_id) VALUES (?, ?, ?)"
            )
        );
        pstmt_insert->setInt64(1, app_id);
        pstmt_insert->setString(2, user_id);
        pstmt_insert->setInt64(3, role_id);
        
        pstmt_insert->executeUpdate();
        return true;
    } catch (const sql::SQLException& e) {
        last_error_ = "授权失败: " + std::string(e.what());
        return false;
    }
}

bool PermissionDAO::removeRoleFromUser(const std::string& app_code,
                          const std::string& user_id,
                          const std::string& role_key) {
    if (!reconnectIfNeeded()) return false;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            connection_->prepareStatement(
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
        last_error_ = "移除权限失败: " + std::string(e.what());
        return false;
    }
}

bool PermissionDAO::addPermissionToRole(const std::string& app_code,
                                        const std::string& role_key,
                                        const std::string& perm_key) {
    if (!reconnectIfNeeded()) return false;
    try {
        // SQL lookups for IDs might be complex, simplified by subqueries or separate lookups.
        // Let's do separate lookups for safety and clarity.
        
        // 1. Get Role ID
        int64_t role_id = -1;
        {
            std::unique_ptr<sql::PreparedStatement> estmt(connection_->prepareStatement(
                "SELECT r.id FROM sys_roles r JOIN sys_apps a ON r.app_id = a.id WHERE a.app_code = ? AND r.role_key = ?"
            ));
            estmt->setString(1, app_code);
            estmt->setString(2, role_key);
            std::unique_ptr<sql::ResultSet> res(estmt->executeQuery());
            if (res->next()) role_id = res->getInt64("id");
            else { last_error_ = "角色不存在"; return false; }
        }

        // 2. Get Permission ID
        int64_t perm_id = -1;
        {
            std::unique_ptr<sql::PreparedStatement> estmt(connection_->prepareStatement(
                "SELECT p.id FROM sys_permissions p JOIN sys_apps a ON p.app_id = a.id WHERE a.app_code = ? AND p.perm_key = ?"
            ));
            estmt->setString(1, app_code);
            estmt->setString(2, perm_key);
            std::unique_ptr<sql::ResultSet> res(estmt->executeQuery());
            if (res->next()) perm_id = res->getInt64("id");
            else { last_error_ = "权限不存在"; return false; }
        }

        // 3. Insert
        std::unique_ptr<sql::PreparedStatement> pstmt(
            connection_->prepareStatement("INSERT INTO sys_role_permissions (role_id, perm_id) VALUES (?, ?)")
        );
        pstmt->setInt64(1, role_id);
        pstmt->setInt64(2, perm_id);
        pstmt->executeUpdate();
        return true;
    } catch (const sql::SQLException& e) {
        last_error_ = "添加角色权限失败: " + std::string(e.what());
        return false;
    }
}

bool PermissionDAO::removePermissionFromRole(const std::string& app_code,
                                             const std::string& role_key,
                                             const std::string& perm_key) {
    if (!reconnectIfNeeded()) return false;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            connection_->prepareStatement(
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
        last_error_ = "移除角色权限失败: " + std::string(e.what());
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
    if (!reconnectIfNeeded()) return false;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            connection_->prepareStatement(
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
    if (!reconnectIfNeeded()) return false;
    try {
        int64_t app_id = getAppId(app_code);
        if (app_id == -1) {
            last_error_ = "应用不存在: " + app_code;
            return false;
        }

        std::unique_ptr<sql::PreparedStatement> pstmt(
            connection_->prepareStatement(
                "DELETE FROM sys_roles WHERE app_id = ? AND role_key = ?"
            )
        );
        pstmt->setInt64(1, app_id);
        pstmt->setString(2, role_key);
        
        int rows = pstmt->executeUpdate();
        if (rows == 0) {
            last_error_ = "角色不存在或已删除";
            return false;
        }
        return true;
    } catch (const sql::SQLException& e) {
        last_error_ = "删除角色失败: " + std::string(e.what());
        return false;
    }
}

bool PermissionDAO::deletePermission(const std::string& app_code, const std::string& perm_key) {
    if (!reconnectIfNeeded()) return false;
    try {
        int64_t app_id = getAppId(app_code);
        if (app_id == -1) {
            last_error_ = "应用不存在: " + app_code;
            return false;
        }

        std::unique_ptr<sql::PreparedStatement> pstmt(
            connection_->prepareStatement(
                "DELETE FROM sys_permissions WHERE app_id = ? AND perm_key = ?"
            )
        );
        pstmt->setInt64(1, app_id);
        pstmt->setString(2, perm_key);
        
        int rows = pstmt->executeUpdate();
        if (rows == 0) {
            last_error_ = "权限不存在或已删除";
            return false;
        }
        return true;
    } catch (const sql::SQLException& e) {
        last_error_ = "删除权限失败: " + std::string(e.what());
        return false;
    }
}

std::vector<PermissionDAO::RoleInfo> PermissionDAO::listRoles(const std::string& app_code) {
    std::vector<RoleInfo> roles;
    if (!reconnectIfNeeded()) return roles;
    try {
        int64_t app_id = getAppId(app_code);
        if (app_id == -1) return roles;

        std::unique_ptr<sql::PreparedStatement> pstmt(
            connection_->prepareStatement(
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
        last_error_ = "查询角色列表失败: " + std::string(e.what());
    }
    return roles;
}

std::vector<PermissionDAO::PermInfo> PermissionDAO::listPermissions(const std::string& app_code) {
    std::vector<PermInfo> perms;
    if (!reconnectIfNeeded()) return perms;
    try {
        int64_t app_id = getAppId(app_code);
        if (app_id == -1) return perms;

        std::unique_ptr<sql::PreparedStatement> pstmt(
            connection_->prepareStatement(
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
        last_error_ = "查询权限列表失败: " + std::string(e.what());
    }
    return perms;
}

bool PermissionDAO::updateRole(const std::string& app_code,
                               const std::string& role_key,
                               const std::string* role_name,
                               const std::string* description,
                               const bool* is_default) {
    if (!reconnectIfNeeded()) return false;
    try {
        int64_t app_id = getAppId(app_code);
        if (app_id == -1) {
            last_error_ = "应用不存在: " + app_code;
            return false;
        }

        std::string sql = "UPDATE sys_roles SET updated_at=NOW()";
        if (role_name) sql += ", role_name=?";
        if (description) sql += ", description=?";
        if (is_default) sql += ", is_default=?";
        sql += " WHERE app_id=? AND role_key=?";

        std::unique_ptr<sql::PreparedStatement> pstmt(connection_->prepareStatement(sql));

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
        last_error_ = "更新角色失败: " + std::string(e.what());
        return false;
    }
}

bool PermissionDAO::updatePermission(const std::string& app_code,
                                     const std::string& perm_key,
                                     const std::string* perm_name,
                                     const std::string* description) {
    if (!reconnectIfNeeded()) return false;
    try {
        int64_t app_id = getAppId(app_code);
        if (app_id == -1) {
            last_error_ = "应用不存在: " + app_code;
            return false;
        }

        std::string sql = "UPDATE sys_permissions SET updated_at=NOW()";
        if (perm_name) sql += ", perm_name=?";
        if (description) sql += ", description=?";
        sql += " WHERE app_id=? AND perm_key=?";

        std::unique_ptr<sql::PreparedStatement> pstmt(connection_->prepareStatement(sql));

        int idx = 1;
        if (perm_name) pstmt->setString(idx++, *perm_name);
        if (description) pstmt->setString(idx++, *description);
        pstmt->setInt64(idx++, app_id);
        pstmt->setString(idx++, perm_key);

        pstmt->executeUpdate();
        return true;
    } catch (const sql::SQLException& e) {
        last_error_ = "更新权限失败: " + std::string(e.what());
        return false;
    }
}

PermissionDAO::ConsoleUser PermissionDAO::getConsoleUser(const std::string& username) {
    ConsoleUser user;
    if (!reconnectIfNeeded()) return user;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            connection_->prepareStatement("SELECT id, username, password_hash, real_name FROM sys_console_users WHERE username = ?")
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
        last_error_ = "查询用户失败: " + std::string(e.what());
    }
    return user;
}

std::string PermissionDAO::getConsoleUserHash(const std::string& username) {
    auto user = getConsoleUser(username);
    return user.password_hash;
}
