#!/bin/bash

# 配置部分 (可以根据实际情况修改)
CONTAINER_NAME="siqi_mysql_prod"
DB_ROOT_PASS="root123"
DB_USER="siqi_dev"
DB_PASS="siqi123"
DB_NAME="siqi_auth"
HOST_PORT=8002

# 获取脚本所在目录的上一级目录 (项目根目录)
PROJECT_ROOT=$(cd "$(dirname "$0")/.."; pwd)

echo "[-] 正在停止并删除旧容器 (如果存在)..."
docker stop $CONTAINER_NAME >/dev/null 2>&1
docker rm $CONTAINER_NAME >/dev/null 2>&1

echo "[+] 正在启动 MySQL Docker 容器..."
# -v 挂载 init.sql 到 /docker-entrypoint-initdb.d/ 目录下，容器首次启动会自动执行其中的 SQL
docker run -d \
    --name $CONTAINER_NAME \
    --restart always \
    -p $HOST_PORT:3306 \
    -e MYSQL_ROOT_PASSWORD=$DB_ROOT_PASS \
    -e MYSQL_DATABASE=$DB_NAME \
    -e MYSQL_USER=$DB_USER \
    -e MYSQL_PASSWORD=$DB_PASS \
    -v "$PROJECT_ROOT/mysql_data":/var/lib/mysql \
    -v "$PROJECT_ROOT/scripts/init.sql":/docker-entrypoint-initdb.d/init.sql \
    mysql:8.0 \
    --server-id=1 \
    --log-bin=mysql-bin \
    --binlog-format=ROW

echo "[*] 等待数据库初始化 (约 10-15 秒)..."
sleep 15

# 创建复制账号 (repl/slave123)
echo "[+] 正在创建 Replication账号..."
docker exec $CONTAINER_NAME mysql -uroot -p$DB_ROOT_PASS -e "CREATE USER IF NOT EXISTS 'repl'@'%' IDENTIFIED WITH mysql_native_password BY 'slave123'; GRANT REPLICATION SLAVE ON *.* TO 'repl'@'%'; FLUSH PRIVILEGES;"

# 检查是否启动成功
if docker ps | grep -q $CONTAINER_NAME; then
    echo "[√] 数据库部署成功！"
    echo "    Host: 127.0.0.1 (宿主机)"
    echo "    Port: $HOST_PORT"
    echo "    User: $DB_USER"
    echo "    Pass: $DB_PASS"
    echo "    DB  : $DB_NAME"
else
    echo "[x] 数据库启动失败，请检查 'docker logs $CONTAINER_NAME'"
fi
