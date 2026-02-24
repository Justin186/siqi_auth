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
-- 3. 权限定义表（优化版）
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
    UNIQUE KEY `uk_app_perm` (`app_id`, `perm_key`),
    KEY `idx_app_id` (`app_id`)  -- ⚡ 新增：按应用查权限
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='权限定义表';

-- ----------------------------
-- 4. 角色定义表（优化版）
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
    UNIQUE KEY `uk_app_role` (`app_id`, `role_key`),
    KEY `idx_app_id` (`app_id`)  -- ⚡ 新增：按应用查角色
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
-- 6. 用户角色授权表（预留临时权限字段）
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
VALUES (1, 'admin', '$6$saltsalt$R/al3WpzpwnOAjyk4icGxW6fCRaUtE6kNJnAakHs220z43YXmDnXm0n3.v1ycAmkvbT3al2H8qMPIy0nP8jVI/', '系统管理员');
-- 初始密码: admin123 (实际使用时必须修改)

-- ----------------------------
-- 清空并重置测试数据
-- ----------------------------
TRUNCATE TABLE `sys_audit_logs`;
TRUNCATE TABLE `sys_user_roles`;
TRUNCATE TABLE `sys_role_permissions`;
TRUNCATE TABLE `sys_roles`;
TRUNCATE TABLE `sys_permissions`;
TRUNCATE TABLE `sys_apps`;

-- ----------------------------
-- A. 注册3个应用
-- ----------------------------
INSERT INTO sys_apps (app_name, app_code, app_secret, description) VALUES
('QQ机器人', 'qq_bot', 'secret_qq_123', 'QQ群管理机器人'),
('内部管理系统', 'admin_panel', 'secret_admin_456', '公司内部后台'),
('课程群助手', 'course_bot', 'secret_course_789', '课程答疑机器人');

SET @app_qq = (SELECT id FROM sys_apps WHERE app_code = 'qq_bot');
SET @app_admin = (SELECT id FROM sys_apps WHERE app_code = 'admin_panel');
SET @app_course = (SELECT id FROM sys_apps WHERE app_code = 'course_bot');

-- ----------------------------
-- B. 定义10个权限
-- ----------------------------
INSERT INTO sys_permissions (app_id, perm_name, perm_key, description) VALUES
-- QQ机器人权限 (4个)
(@app_qq, '踢出成员', 'member:kick', '允许踢出群成员'),
(@app_qq, '禁言成员', 'member:mute', '允许禁言群成员'),
(@app_qq, '删除消息', 'message:delete', '允许删除群消息'),
(@app_qq, '置顶消息', 'message:pin', '允许置顶消息'),

-- 内部管理系统权限 (4个)
(@app_admin, '数据查看', 'data:view', '查看报表数据'),
(@app_admin, '数据导出', 'data:export', '导出数据'),
(@app_admin, '用户创建', 'user:create', '创建用户'),
(@app_admin, '用户删除', 'user:delete', '删除用户'),

-- 课程群助手权限 (2个)
(@app_course, '布置作业', 'homework:assign', '布置作业'),
(@app_course, '批改作业', 'homework:grade', '批改作业');

-- ----------------------------
-- C. 定义角色
-- ----------------------------
INSERT INTO sys_roles (app_id, role_name, role_key, description, is_default) VALUES
-- QQ机器人角色
(@app_qq, '超级管理员', 'admin', '拥有所有权限', 0),
(@app_qq, '群主', 'owner', '群拥有者', 0),
(@app_qq, '管理员', 'manager', '协助管理', 0),
(@app_qq, '普通成员', 'member', '普通群员', 1),

-- 内部管理系统角色
(@app_admin, '超级管理员', 'admin', '系统管理员', 0),
(@app_admin, '部门经理', 'manager', '部门负责人', 0),
(@app_admin, '普通员工', 'staff', '普通员工', 1),

-- 课程群助手角色
(@app_course, '教师', 'teacher', '授课教师', 0),
(@app_course, '助教', 'assistant', '课程助教', 0),
(@app_course, '学生', 'student', '选课学生', 1);

-- ----------------------------
-- D. 角色分配权限
-- ----------------------------
-- QQ机器人角色权限
INSERT INTO sys_role_permissions (role_id, perm_id) VALUES
-- admin角色拥有全部4个权限
((SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'admin'), 
 (SELECT id FROM sys_permissions WHERE app_id = @app_qq AND perm_key = 'member:kick')),
((SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'admin'), 
 (SELECT id FROM sys_permissions WHERE app_id = @app_qq AND perm_key = 'member:mute')),
((SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'admin'), 
 (SELECT id FROM sys_permissions WHERE app_id = @app_qq AND perm_key = 'message:delete')),
((SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'admin'), 
 (SELECT id FROM sys_permissions WHERE app_id = @app_qq AND perm_key = 'message:pin')),

-- owner角色拥有3个权限（除置顶消息）
((SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'owner'), 
 (SELECT id FROM sys_permissions WHERE app_id = @app_qq AND perm_key = 'member:kick')),
((SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'owner'), 
 (SELECT id FROM sys_permissions WHERE app_id = @app_qq AND perm_key = 'member:mute')),
((SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'owner'), 
 (SELECT id FROM sys_permissions WHERE app_id = @app_qq AND perm_key = 'message:delete')),

-- manager角色拥有2个权限（禁言和删消息）
((SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'manager'), 
 (SELECT id FROM sys_permissions WHERE app_id = @app_qq AND perm_key = 'member:mute')),
((SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'manager'), 
 (SELECT id FROM sys_permissions WHERE app_id = @app_qq AND perm_key = 'message:delete')),

-- member角色无权限（不插入）

-- 内部管理系统角色权限
((SELECT id FROM sys_roles WHERE app_id = @app_admin AND role_key = 'admin'), 
 (SELECT id FROM sys_permissions WHERE app_id = @app_admin AND perm_key = 'data:view')),
((SELECT id FROM sys_roles WHERE app_id = @app_admin AND role_key = 'admin'), 
 (SELECT id FROM sys_permissions WHERE app_id = @app_admin AND perm_key = 'data:export')),
((SELECT id FROM sys_roles WHERE app_id = @app_admin AND role_key = 'admin'), 
 (SELECT id FROM sys_permissions WHERE app_id = @app_admin AND perm_key = 'user:create')),
((SELECT id FROM sys_roles WHERE app_id = @app_admin AND role_key = 'admin'), 
 (SELECT id FROM sys_permissions WHERE app_id = @app_admin AND perm_key = 'user:delete')),

((SELECT id FROM sys_roles WHERE app_id = @app_admin AND role_key = 'manager'), 
 (SELECT id FROM sys_permissions WHERE app_id = @app_admin AND perm_key = 'data:view')),
((SELECT id FROM sys_roles WHERE app_id = @app_admin AND role_key = 'manager'), 
 (SELECT id FROM sys_permissions WHERE app_id = @app_admin AND perm_key = 'data:export')),
((SELECT id FROM sys_roles WHERE app_id = @app_admin AND role_key = 'manager'), 
 (SELECT id FROM sys_permissions WHERE app_id = @app_admin AND perm_key = 'user:create')),

((SELECT id FROM sys_roles WHERE app_id = @app_admin AND role_key = 'staff'), 
 (SELECT id FROM sys_permissions WHERE app_id = @app_admin AND perm_key = 'data:view')),

-- 课程群助手角色权限
((SELECT id FROM sys_roles WHERE app_id = @app_course AND role_key = 'teacher'), 
 (SELECT id FROM sys_permissions WHERE app_id = @app_course AND perm_key = 'homework:assign')),
((SELECT id FROM sys_roles WHERE app_id = @app_course AND role_key = 'teacher'), 
 (SELECT id FROM sys_permissions WHERE app_id = @app_course AND perm_key = 'homework:grade')),

((SELECT id FROM sys_roles WHERE app_id = @app_course AND role_key = 'assistant'), 
 (SELECT id FROM sys_permissions WHERE app_id = @app_course AND perm_key = 'homework:grade'));

-- ----------------------------
-- E. 给40个用户分配角色
-- ----------------------------
-- QQ机器人用户 (20个)
INSERT INTO sys_user_roles (app_id, app_user_id, role_id) VALUES
-- admin角色 (2个)
(@app_qq, '100001', (SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'admin')),
(@app_qq, '100002', (SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'admin')),

-- owner角色 (3个)
(@app_qq, '100101', (SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'owner')),
(@app_qq, '100102', (SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'owner')),
(@app_qq, '100103', (SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'owner')),

-- manager角色 (5个)
(@app_qq, '100201', (SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'manager')),
(@app_qq, '100202', (SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'manager')),
(@app_qq, '100203', (SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'manager')),
(@app_qq, '100204', (SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'manager')),
(@app_qq, '100205', (SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'manager')),

-- member角色 (10个)
(@app_qq, '100301', (SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'member')),
(@app_qq, '100302', (SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'member')),
(@app_qq, '100303', (SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'member')),
(@app_qq, '100304', (SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'member')),
(@app_qq, '100305', (SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'member')),
(@app_qq, '100306', (SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'member')),
(@app_qq, '100307', (SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'member')),
(@app_qq, '100308', (SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'member')),
(@app_qq, '100309', (SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'member')),
(@app_qq, '100310', (SELECT id FROM sys_roles WHERE app_id = @app_qq AND role_key = 'member'));

-- 内部管理系统用户 (12个)
INSERT INTO sys_user_roles (app_id, app_user_id, role_id) VALUES
-- admin角色 (2个)
(@app_admin, '200001', (SELECT id FROM sys_roles WHERE app_id = @app_admin AND role_key = 'admin')),
(@app_admin, '200002', (SELECT id FROM sys_roles WHERE app_id = @app_admin AND role_key = 'admin')),

-- manager角色 (3个)
(@app_admin, '200101', (SELECT id FROM sys_roles WHERE app_id = @app_admin AND role_key = 'manager')),
(@app_admin, '200102', (SELECT id FROM sys_roles WHERE app_id = @app_admin AND role_key = 'manager')),
(@app_admin, '200103', (SELECT id FROM sys_roles WHERE app_id = @app_admin AND role_key = 'manager')),

-- staff角色 (7个)
(@app_admin, '200201', (SELECT id FROM sys_roles WHERE app_id = @app_admin AND role_key = 'staff')),
(@app_admin, '200202', (SELECT id FROM sys_roles WHERE app_id = @app_admin AND role_key = 'staff')),
(@app_admin, '200203', (SELECT id FROM sys_roles WHERE app_id = @app_admin AND role_key = 'staff')),
(@app_admin, '200204', (SELECT id FROM sys_roles WHERE app_id = @app_admin AND role_key = 'staff')),
(@app_admin, '200205', (SELECT id FROM sys_roles WHERE app_id = @app_admin AND role_key = 'staff')),
(@app_admin, '200206', (SELECT id FROM sys_roles WHERE app_id = @app_admin AND role_key = 'staff')),
(@app_admin, '200207', (SELECT id FROM sys_roles WHERE app_id = @app_admin AND role_key = 'staff'));

-- 课程群助手用户 (8个)
INSERT INTO sys_user_roles (app_id, app_user_id, role_id) VALUES
-- teacher角色 (2个)
(@app_course, '300001', (SELECT id FROM sys_roles WHERE app_id = @app_course AND role_key = 'teacher')),
(@app_course, '300002', (SELECT id FROM sys_roles WHERE app_id = @app_course AND role_key = 'teacher')),

-- assistant角色 (2个)
(@app_course, '300101', (SELECT id FROM sys_roles WHERE app_id = @app_course AND role_key = 'assistant')),
(@app_course, '300102', (SELECT id FROM sys_roles WHERE app_id = @app_course AND role_key = 'assistant')),

-- student角色 (4个)
(@app_course, '300201', (SELECT id FROM sys_roles WHERE app_id = @app_course AND role_key = 'student')),
(@app_course, '300202', (SELECT id FROM sys_roles WHERE app_id = @app_course AND role_key = 'student')),
(@app_course, '300203', (SELECT id FROM sys_roles WHERE app_id = @app_course AND role_key = 'student')),
(@app_course, '300204', (SELECT id FROM sys_roles WHERE app_id = @app_course AND role_key = 'student'));

-- 总计: 20 + 12 + 8 = 40个用户

-- ----------------------------
-- F. 审计日志示例
-- ----------------------------
INSERT INTO sys_audit_logs (operator_id, operator_name, app_code, action, target_type, target_id, target_name, object_type, object_id, object_name) VALUES
(1, '系统管理员', 'qq_bot', 'USER_GRANT_ROLE', 'USER', '100001', '测试用户1', 'ROLE', 'admin', '超级管理员'),
(1, '系统管理员', 'admin_panel', 'CREATE_PERM', 'ROLE', 'manager', '部门经理', 'PERM', 'user:create', '用户创建'),
(1, '系统管理员', 'course_bot', 'USER_GRANT_ROLE', 'USER', '300001', '张老师', 'ROLE', 'teacher', '教师');

SET FOREIGN_KEY_CHECKS = 1;

-- ----------------------------
-- 统计信息查询
-- ----------------------------
-- SELECT 'sys_apps' AS table_name, COUNT(*) AS count FROM sys_apps
-- UNION ALL SELECT 'sys_permissions', COUNT(*) FROM sys_permissions
-- UNION ALL SELECT 'sys_roles', COUNT(*) FROM sys_roles
-- UNION ALL SELECT 'sys_role_permissions', COUNT(*) FROM sys_role_permissions
-- UNION ALL SELECT 'sys_user_roles', COUNT(*) FROM sys_user_roles;
-- 
-- 预期结果:
-- sys_apps: 3
-- sys_permissions: 10
-- sys_roles: 10
-- sys_role_permissions: 18
-- sys_user_roles: 40

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