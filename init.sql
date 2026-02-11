-- 创建应用表
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

-- 创建权限表
CREATE TABLE `sys_permissions` (
    `id` BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '权限ID',
    `app_id` BIGINT NOT NULL COMMENT '所属应用ID',
    `perm_name` VARCHAR(64) NOT NULL COMMENT '权限名称 例:踢人',
    `perm_key` VARCHAR(64) NOT NULL COMMENT '权限代码标识符 例:member:kick',
    `description` VARCHAR(255) COMMENT '描述',
    `created_at` DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间戳',
    INDEX `idx_app_id` (`app_id`),
    UNIQUE KEY `uk_app_perm` (`app_id`, `perm_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='权限定义表';

-- 创建角色表
CREATE TABLE `sys_roles` (
    `id` BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '角色ID',
    `app_id` BIGINT NOT NULL COMMENT '所属应用ID',
    `role_name` VARCHAR(32) NOT NULL COMMENT '角色名称',
    `role_key` VARCHAR(32) NOT NULL COMMENT '角色代码',
    `description` VARCHAR(255) COMMENT '描述',
    `is_default` TINYINT(1) DEFAULT 0 COMMENT '是否默认角色',
    `created_at` DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    INDEX `idx_app_id` (`app_id`),
    UNIQUE KEY `uk_app_role` (`app_id`, `role_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='角色表';

-- 创建角色权限关联表
CREATE TABLE `sys_role_permissions` (
    `id` BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '关联ID',
    `app_id` BIGINT NOT NULL COMMENT '应用ID',
    `role_id` BIGINT NOT NULL COMMENT '角色ID',
    `perm_id` BIGINT NOT NULL COMMENT '权限ID',
    INDEX `idx_role_id` (`role_id`),
    INDEX `idx_perm_id` (`perm_id`),
    UNIQUE KEY `uk_role_perm` (`role_id`, `perm_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='角色权限关联表';

-- 创建用户角色关联表
CREATE TABLE `sys_user_roles` (
    `id` BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '关联ID',
    `app_id` BIGINT NOT NULL COMMENT '应用ID',
    `app_user_id` VARCHAR(128) NOT NULL COMMENT '业务系统用户ID',
    `role_id` BIGINT NOT NULL COMMENT '角色ID',
    `created_at` DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '授权时间',
    INDEX `idx_app_user` (`app_id`, `app_user_id`),
    INDEX `idx_role_id` (`role_id`),
    UNIQUE KEY `uk_user_role` (`app_id`, `app_user_id`, `role_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户角色关联表';

-- 插入测试数据示例
INSERT INTO sys_apps (app_name, app_code, app_secret, description) VALUES
('QQ机器人', 'qq_bot', 'secret_qq_123', 'QQ群管理机器人');

-- 获取应用ID
SET @app_id = LAST_INSERT_ID();

-- 插入权限
INSERT INTO sys_permissions (app_id, perm_name, perm_key, description) VALUES
(@app_id, '踢出成员', 'member:kick', '将成员踢出群聊'),
(@app_id, '禁言成员', 'member:mute', '禁言群成员');

-- 插入角色
INSERT INTO sys_roles (app_id, role_name, role_key, description, is_default) VALUES
(@app_id, '管理员', 'admin', '系统管理员', 0),
(@app_id, '普通用户', 'user', '普通用户', 1);

-- 给管理员分配踢人权限
INSERT INTO sys_role_permissions (app_id, role_id, perm_id)
SELECT @app_id, 1, id FROM sys_permissions WHERE perm_key = 'member:kick';

-- 添加测试用户
INSERT INTO sys_user_roles (app_id, app_user_id, role_id) VALUES
(@app_id, '123456', 1);  -- 用户123456是管理员