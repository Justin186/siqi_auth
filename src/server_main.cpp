#include <brpc/server.h>
#include <gflags/gflags.h>
#include "auth_service_impl.h"
#include "admin_service_impl.h"
#include "local_cache.h"
#include <unordered_set>

DEFINE_int32(port, 8888, "TCP Port of this server");
DEFINE_string(db_host, "localhost", "MySQL host");
DEFINE_int32(db_port, 3306, "MySQL port");
DEFINE_string(db_user, "siqi_dev", "MySQL user");
DEFINE_string(db_password, "siqi123", "MySQL password");
DEFINE_string(db_name, "siqi_auth", "MySQL database name");
DEFINE_int32(cache_ttl, 60, "Cache TTL in seconds");
DEFINE_int32(session_ttl, 3600, "Admin session TTL in seconds");

int main(int argc, char* argv[]) {
    // 解析命令行参数
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    // 0. 创建共享缓存 (Key: app:user, Value: Set<Perm>)
    auto cache = std::make_shared<LocalCache<std::unordered_set<std::string>>>();

    // 1. 创建服务实例
    AuthServiceImpl auth_service(cache, FLAGS_db_host, FLAGS_db_port, FLAGS_db_user, FLAGS_db_password, FLAGS_db_name, FLAGS_cache_ttl);
    AdminServiceImpl admin_service(cache, FLAGS_db_host, FLAGS_db_port, FLAGS_db_user, FLAGS_db_password, FLAGS_db_name, FLAGS_session_ttl);
    
    // 2. 创建brpc服务器
    brpc::Server server;
    
    // 3. 添加服务
    if (server.AddService(&auth_service, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "添加 AuthService 失败";
        return -1;
    }

    if (server.AddService(&admin_service, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "添加 AdminService 失败";
        return -1;
    }
    
    // 4. 启动服务器
    brpc::ServerOptions options;
    if (server.Start(FLAGS_port, &options) != 0) {
        LOG(ERROR) << "启动服务器失败";
        return -1;
    }
    
    LOG(INFO) << "司契权限系统启动成功，监听端口: " << FLAGS_port;
    LOG(INFO) << "其他系统可以通过 brpc://localhost:" << FLAGS_port << " 调用";
    
    // 5. 运行直到收到停止信号
    server.RunUntilAskedToQuit();
    
    return 0;
}