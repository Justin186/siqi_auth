#!/bin/bash

# 确保在 build 目录中
PROJECT_ROOT=$(cd "$(dirname "$0")/.."; pwd)
BUILD_DIR="$PROJECT_ROOT/build"
cd "$PROJECT_ROOT"

# 1. 编译 (如果尚未编译，可选)
if [ ! -f "$BUILD_DIR/auth_server" ]; then
    echo "[-] 未检测到二进制文件，尝试编译..."
    mkdir -p build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make -j$(nproc) auth_server
    cd ..
fi

echo "[+] 启动 Auth Server..."

# 后台运行并重定向日志
# 注意：这里连接的是 127.0.0.1:3306，即 deploy_db.sh 启动的 Docker 内 Master 数据库
nohup $BUILD_DIR/auth_server \
    --port=8001 \
    --db_host=127.0.0.1 \
    --db_port=8002 \
    --db_user=siqi_dev \
    --db_password=siqi123 \
    --db_name=siqi_auth \
    > auth_server.log 2>&1 &

echo "[√] 服务已在后台启动，日志文件: auth_server.log"
echo "    查看日志命令: tail -f auth_server.log"
echo "    停止服务命令: pkill -f auth_server"
