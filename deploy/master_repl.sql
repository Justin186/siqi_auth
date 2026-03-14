-- MySQL 主从复制专用账号初始化脚本
-- 该脚本会在 MySQL 容器首次初始化时自动执行（挂载到 /docker-entrypoint-initdb.d）

-- 1) 创建复制账号 repl（允许任意来源 IP 连接）
--    mysql_native_password 在一些客户端/版本组合下兼容性更好
CREATE USER IF NOT EXISTS 'repl'@'%' IDENTIFIED WITH mysql_native_password BY 'slave123';

-- 2) 仅授予复制所需权限（最小权限原则）
GRANT REPLICATION SLAVE ON *.* TO 'repl'@'%';

-- 3) 刷新权限使配置立即生效
FLUSH PRIVILEGES;
