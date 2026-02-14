#include <brpc/server.h>
#include <gflags/gflags.h>
#include "auth_agent.h"
#include "permission_dao.h"

// Agent 监听端口
DEFINE_int32(port, 8881, "Agent 监听端口 (提供给本机应用调用)");

// 数据库配置 (默认为本地 Slave, 连接 127.0.0.1:3306)
DEFINE_string(db_host, "127.0.0.1", "MySQL Slave Addr");
DEFINE_int32(db_port, 3306, "MySQL Slave Port");
DEFINE_string(db_user, "root", "MySQL User");  // 假设使用 root 或专用读用户
DEFINE_string(db_password, "siqi123", "MySQL Password");
DEFINE_string(db_name, "siqi_auth", "MySQL DB Name");

int main(int argc, char* argv[]) {
    // 解析命令行参数
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    
    // 1. 初始化本地数据库连接 (替代原来的 RPC Channel)
    // PermissionDAO 内部维护连接池，适合高并发读取
    // 注意：这里我们连接的是 Local MySQL Slave，延迟极低 (<1ms)
    PermissionDAO dao(FLAGS_db_host, FLAGS_db_port, FLAGS_db_user, FLAGS_db_password, FLAGS_db_name);
    
    // 2. 启动 Agent Server
    brpc::Server server;
    AuthAgentImpl agent_service(&dao);
    
    if (server.AddService(&agent_service, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "添加 AgentService 失败";
        return -1;
    }
    
    brpc::ServerOptions options;
    if (server.Start(FLAGS_port, &options) != 0) {
        LOG(ERROR) << "启动 Agent 失败";
        return -1;
    }
    
    LOG(INFO) << "Siqi Auth Agent (Local DB Mode) 已启动!";
    LOG(INFO) << "  - 监听端口: " << FLAGS_port;
    LOG(INFO) << "  - 本地数据库: " << FLAGS_db_host << ":" << FLAGS_db_port;
    LOG(INFO) << "  - 模式: 直连数据库 (Master-Slave Replica)";

    server.RunUntilAskedToQuit();
    return 0;
}
