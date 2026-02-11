-- 司契权限系统数据库设计（优化版）
-- 创建时间: 2025-02-11
-- 说明: 采用RBAC模型，包含应用注册、权限定义、角色管理、用户授权、审计日志等核心表
-- 优化点: 1.全表使用自增主键 2.统一注释风格 3.移除冗余字段 4.统一时间戳格式

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

-- 创建数据库（如果不存在）
CREATE DATABASE IF NOT EXISTS `siqi_auth` 
CHARACTER SET utf8mb4 
COLLATE utf8mb4_unicode_ci;

-- 创建应用用户并授权（如果不存在）
CREATE USER IF NOT EXISTS 'siqi_dev'@'localhost' IDENTIFIED BY 'siqi123';
GRANT ALL PRIVILEGES ON `siqi_auth`.* TO 'siqi_dev'@'localhost';
FLUSH PRIVILEGES;

-- 切换到 siqi_auth 数据库
USE `siqi_auth`;

-- ----------------------------
-- 1. 应用注册表
-- ----------------------------
DROP TABLE IF EXISTS `sys_apps`;
CREATE TABLE `sys_apps` (
    `id` BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '系统ID',
    `app_name` VARCHAR(50) NOT NULL COMMENT '系统名称 例:课程群bot',
    `app_code` VARCHAR(32) NOT NULL UNIQUE COMMENT '应用代号 例:course_bot',
    `app_secret` VARCHAR(64) NOT NULL COMMENT 'RPC通信签名密钥',
    `description` VARCHAR(255) COMMENT '描述',
    `status` TINYINT DEFAULT 1 COMMENT '1启用 0禁用',
    `created_at` DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间戳',
    `updated_at` DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间戳'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='应用注册表';

-- ----------------------------
-- 2. 管理员表
-- ----------------------------
DROP TABLE IF EXISTS `sys_console_users`;
CREATE TABLE `sys_console_users` (
    `id` BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '管理员ID',
    `username` VARCHAR(32) NOT NULL UNIQUE COMMENT '用户名',
    `password_hash` VARCHAR(128) NOT NULL COMMENT '密码哈希',
    `real_name` VARCHAR(32) NOT NULL COMMENT '姓名',
    `created_at` DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间戳',
    `updated_at` DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间戳'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='控制台管理员表';

-- ----------------------------
-- 3. 权限定义表
-- ----------------------------
DROP TABLE IF EXISTS `sys_permissions`;
CREATE TABLE `sys_permissions` (
    `id` BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '权限ID',
    `app_id` BIGINT NOT NULL COMMENT '所属应用ID 关联sys_apps.id',
    `perm_name` VARCHAR(64) NOT NULL COMMENT '权限名称 例:踢人',
    `perm_key` VARCHAR(64) NOT NULL COMMENT '权限标识 例:member:kick',
    `description` VARCHAR(255) COMMENT '描述',
    `created_at` DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间戳',
    `updated_at` DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间戳',
    UNIQUE KEY `uk_app_perm` (`app_id`, `perm_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='权限定义表';

-- ----------------------------
-- 4. 角色定义表
-- ----------------------------
DROP TABLE IF EXISTS `sys_roles`;
CREATE TABLE `sys_roles` (
    `id` BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '角色ID',
    `app_id` BIGINT NOT NULL COMMENT '所属应用ID 关联sys_apps.id',
    `role_name` VARCHAR(32) NOT NULL COMMENT '角色名称 例:管理员',
    `role_key` VARCHAR(32) NOT NULL COMMENT '角色标识 例:admin',
    `description` VARCHAR(255) COMMENT '描述',
    `is_default` TINYINT DEFAULT 0 COMMENT '是否默认角色 1是 0否',
    `created_at` DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间戳',
    `updated_at` DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间戳',
    UNIQUE KEY `uk_app_role` (`app_id`, `role_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='角色定义表';

-- ----------------------------
-- 5. 角色权限关联表
-- ----------------------------
DROP TABLE IF EXISTS `sys_role_permissions`;
CREATE TABLE `sys_role_permissions` (
    `id` BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '关联ID',
    `role_id` BIGINT NOT NULL COMMENT '角色ID 关联sys_roles.id',
    `perm_id` BIGINT NOT NULL COMMENT '权限ID 关联sys_permissions.id',
    `created_at` DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间戳',
    UNIQUE KEY `uk_role_perm` (`role_id`, `perm_id`),
    KEY `idx_perm_id` (`perm_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='角色权限绑定表';

-- ----------------------------
-- 6. 用户角色授权表
-- ----------------------------
DROP TABLE IF EXISTS `sys_user_roles`;
CREATE TABLE `sys_user_roles` (
    `id` BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '授权ID',
    `app_id` BIGINT NOT NULL COMMENT '业务系统ID 关联sys_apps.id',
    `app_user_id` VARCHAR(128) NOT NULL COMMENT '业务系统用户ID 例:QQ号/工号',
    `role_id` BIGINT NOT NULL COMMENT '角色ID 关联sys_roles.id',
    `created_at` DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间戳',
    `updated_at` DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间戳',
    UNIQUE KEY `uk_app_user_role` (`app_id`, `app_user_id`, `role_id`),
    KEY `idx_user_query` (`app_id`, `app_user_id`),
    KEY `idx_role_query` (`role_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户角色授权表';

-- ----------------------------
-- 7. 操作审计日志表
-- ----------------------------
DROP TABLE IF EXISTS `sys_audit_logs`;
CREATE TABLE `sys_audit_logs` (
    `id` BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '日志ID',
    
    -- 操作者 (who)
    `operator_id` BIGINT NOT NULL COMMENT '管理员ID 关联sys_console_users.id',
    `operator_name` VARCHAR(32) NOT NULL COMMENT '管理员姓名（冗余）',
    
    -- 操作范围 (where)
    `app_code` VARCHAR(32) NOT NULL COMMENT '系统代号 例:course_bot',
    
    -- 动作类型 (what)
    `action` VARCHAR(32) NOT NULL COMMENT '动作类型 例:USER_GRANT_ROLE,ROLE_ADD_PERM',
    
    -- 主对象 (to whom)
    `target_type` VARCHAR(16) NOT NULL COMMENT '主对象类型 USER/ROLE/PERM',
    `target_id` VARCHAR(128) NOT NULL COMMENT '主对象ID 用户ID/角色ID/权限ID',
    `target_name` VARCHAR(64) COMMENT '主对象名称（冗余）',
    
    -- 关联对象 (with what)
    `object_type` VARCHAR(16) COMMENT '关联对象类型 ROLE/PERM',
    `object_id` VARCHAR(128) COMMENT '关联对象ID 角色ID/权限ID',
    `object_name` VARCHAR(64) COMMENT '关联对象名称（冗余）',
    
    `created_at` DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间戳',
    
    KEY `idx_operator` (`operator_id`),
    KEY `idx_target` (`target_id`),
    KEY `idx_app_code` (`app_code`),
    KEY `idx_action` (`action`),
    KEY `idx_created_at` (`created_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='操作审计日志表';

-- ----------------------------
-- 初始化数据：内置超级管理员（密码需后续修改）
-- ----------------------------
INSERT IGNORE INTO `sys_console_users` (`id`, `username`, `password_hash`, `real_name`) 
VALUES (1, 'admin', '$2a$10$N.zmdr9k7uOCQb376NoUnuTJ8iAt6Z5EHsM8lE4lBoTWqjpNVEc2G', '系统管理员');
-- 初始密码: admin123 (实际使用时必须修改)

-- ----------------------------
-- 测试数据
-- ----------------------------
-- A. 注册应用
INSERT INTO sys_apps (app_name, app_code, app_secret, description) VALUES
('QQ机器人', 'qq_bot', 'secret_qq_123', 'QQ群管理机器人'),
('内部管理系统', 'admin_panel', 'secret_admin_456', '公司内部后台');

-- 获取应用ID变量 (假设是第一个插入的)
SET @app_qq = (SELECT id FROM sys_apps WHERE app_code = 'qq_bot');
SET @app_admin = (SELECT id FROM sys_apps WHERE app_code = 'admin_panel');

-- B. 定义权限
INSERT INTO sys_permissions (app_id, perm_name, perm_key, description) VALUES
(@app_qq, '踢出成员', 'member:kick', '允许踢出群成员'),
(@app_qq, '禁言成员', 'member:mute', '允许禁言群成员'),
(@app_admin, '数据查看', 'data:view', '查看报表数据');

-- C. 定义角色
INSERT INTO sys_roles (app_id, role_name, role_key, description, is_default) VALUES
(@app_qq, '超级管理员', 'admin', '拥有所有权限', 0),
(@app_qq, '群主', 'owner', '群拥有者', 0),
(@app_qq, '普通成员', 'member', '普通群员', 1);

-- 获取ID变量
SET @perm_kick = (SELECT id FROM sys_permissions WHERE app_id = @app_qq AND perm_key = 'member:kick');
SET @perm_mute = (SELECT id FROM sys_permissions WHERE app_id = @app_qq AND perm_key = 'member:mute');
SET @role_admin = (SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'admin');

-- D. 给角色分配权限 (注意：V2版本表结构移除了 app_id 字段)
INSERT INTO sys_role_permissions (role_id, perm_id) VALUES
(@role_admin, @perm_kick),
(@role_admin, @perm_mute);

-- E. 给用户分配角色
-- 给用户 '123456' 分配 'admin' 角色
INSERT INTO sys_user_roles (app_id, app_user_id, role_id) VALUES
(@app_qq, '123456', @role_admin);

-- F. 审计日志示例
INSERT INTO sys_audit_logs (operator_id, operator_name, app_code, action, target_type, target_id, target_name) VALUES
(1, '系统管理员', 'qq_bot', 'USER_GRANT_ROLE', 'USER', '123456', '测试用户');

SET FOREIGN_KEY_CHECKS = 1;

-- ----------------------------
-- 建表检查说明:
-- 1. 所有表均使用自增BIGINT主键
-- 2. 时间戳统一为DATETIME + DEFAULT CURRENT_TIMESTAMP
-- 3. 外键约束未物理创建，通过注释体现逻辑关联
-- 4. sys_role_permissions 已移除冗余的app_id字段
-- 5. sys_user_roles 增加联合唯一约束防止重复授权
-- 6. 审计日志增加更多索引覆盖常见查询场景
-- ----------------------------