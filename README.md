# 司契权限系统

司契权限系统是一个基于 C++ 和 bRPC 开发的高性能、中心化权限控制服务。它采用 RBAC（基于角色的访问控制）模型，为微服务架构中的其他应用（如 QQ 机器人、青鸾系统等）提供统一的权限校验能力。

## 核心特性

- **高性能 RPC**：基于百度 bRPC 框架，支持高并发低延迟调用。
- **RBAC 模型**：支持 用户 -> 角色 -> 权限 的标准模型。
- **Protobuf 接口**：使用 Protocol Buffers 定义接口，跨语言支持友好。
- **持久化存储**：使用 MySQL 存储权限数据。
- **易于集成**：提供简单的客户端 SDK 示例。

## 技术栈

- **RPC 框架**: [Apache bRPC](https://github.com/apache/brpc)
- **序列化**: Protocol Buffers 3
- **数据库**: MySQL (使用 MySQL Connector/C++)
- **构建工具**: CMake 3.10+

## 项目结构

```
siqi_auth/                   # 项目根目录
├── build/                   # 编译输出目录
├── conf/                    # 配置文件目录
├── include/                 # 头文件目录
│   ├── auth.pb.h            # [自动生成] Protobuf 生成的 C++ 头文件
│   ├── auth_service_impl.h  # 鉴权服务接口实现类定义
│   └── permission_dao.h     # 数据访问层（DAO）接口定义，负责数据库交互
├── proto/                   # RPC 接口定义目录
│   └── auth.proto           # Protobuf 文件，定义服务接口（Check, BatchCheck）与消息结构
├── src/                     # 源代码目录
│   ├── auth.pb.cc           # [自动生成] Protobuf 生成的 C++ 源文件
│   ├── auth_service_impl.cpp # 鉴权服务具体逻辑实现
│   ├── client_example.cpp   # 客户端 SDK 调用示例代码
│   ├── permission_dao.cpp   # 数据库操作具体实现（CRUD）
│   └── server_main.cpp      # 服务端主入口，负责初始化与启动 bRPC 服务
├── CMakeLists.txt           # CMake 构建脚本，定义依赖与编译规则
└── README.md                # 项目说明文档
```

## 环境准备 (Ubuntu 22.04)

确保系统安装了必要的构建工具和依赖库：

```bash
# 基础构建工具
sudo apt install -y git g++ cmake make

# 依赖库
sudo apt install -y libssl-dev libgflags-dev libprotobuf-dev protobuf-compiler libleveldb-dev

# MySQL Connector/C++
sudo apt install -y libmysqlcppconn-dev
```

bRPC 安装（参考[brpc的安装与使用介绍以及channel的封装](https://blog.csdn.net/m0_74189279/article/details/149484082)）：
```bash
git clone
cd brpc/
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ... && cmake --build . -j8
sudo make install
```

## 编译构建

```bash
cmake -B build
make -C build/ -j`nproc`
```

构建完成后，`build/` 目录下将生成以下可执行文件：
- `auth_server`: 权限系统服务端
- `test_client`: 测试用客户端

## 数据库配置

项目默认使用 MySQL 数据库，默认配置如下（硬编码在代码中，后期修改为配置文件）：
- **Host**: `localhost`
- **Port**: `3306`
- **User**: `siqi_dev`
- **Password**: `siqi123`
- **Database**: `siqi_auth`

### 初始化数据库

```bash
mysql -u siqi_dev -p siqi_auth < init.sql
```

## 运行服务

1. **启动 MySQL** 并确保数据库已初始化。
2. **启动权限服务器**：

```bash
./build/auth_server
```

服务器默认监听端口 **8888**。

日志输出示例：
```
司契权限系统启动成功，监听端口: 8888
其他系统可以通过 brpc://localhost:8888 调用
```

## 测试客户端

运行测试客户端以验证服务是否正常工作：

```bash
./build/test_client
```

客户端会尝试连接本地服务器并进行一次模拟的权限检查。

## 接口定义

服务接口定义在 `proto/auth.proto` 中：

- `CheckRequest`: 包含 `app_code` (应用标识), `user_id` (用户ID), `perm_key` (权限标识)。
- `CheckResponse`: 返回 `allowed` (布尔值) 表示是否拥有权限。

当接口文件发生变更时，请重新生成对应的 C++ 代码：

```bash
protoc --proto_path=proto --cpp_out=src proto/auth.proto && mv src/auth.pb.h include/
```


## 未来优化方向
**项目接入采用计划中的方案二：独立Client**

| 功能 | 当前 | 完整方案二需要 |
|------|-----------|---------------|
| **多语言SDK** | C++版 | C++、Java、Python、Go... |
| **客户端缓存** | ❌ 无 | ✅ 本地缓存策略 |
| **连接池** | ❌ 简单连接 | ✅ 连接池管理 |
| **熔断降级** | ❌ 无 | ✅ 熔断器机制 |
| **配置管理** | ❌ 硬编码 | ✅ 配置文件支持 |
| **监控指标** | ❌ 基础日志 | ✅ 详细指标收集 |

