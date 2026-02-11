#include "permission_dao.h"
#include <cppconn/prepared_statement.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <iostream>

PermissionDAO::PermissionDAO(const std::string& host,
                             const std::string& user,
                             const std::string& password,
                             const std::string& database) {
    try {
        sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
        connection_.reset(driver->connect("tcp://" + host + ":3306", user, password));
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
                "JOIN sys_role_permissions rp ON ur.role_id = rp.role_id AND ur.app_id = rp.app_id "
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
     return {}; // Stub
}

int64_t PermissionDAO::getAppId(const std::string& app_code) {
    return 0; // Stub
}

// Implement other methods if they exist in header
bool PermissionDAO::assignRoleToUser(const std::string& app_code,
                          const std::string& user_id,
                          const std::string& role_key) {
    return false;
}

bool PermissionDAO::removeRoleFromUser(const std::string& app_code,
                          const std::string& user_id,
                          const std::string& role_key) {
    return false;
}
