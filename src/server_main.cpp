#include <brpc/server.h>
#include "auth_service_impl.h"

int main(int argc, char* argv[]) {
    // 1. 创建服务实例
    AuthServiceImpl auth_service;
    
    // 2. 创建brpc服务器
    brpc::Server server;
    
    // 3. 添加服务
    if (server.AddService(&auth_service, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "添加服务失败";
        return -1;
    }
    
    // 4. 启动服务器
    brpc::ServerOptions options;
    if (server.Start(8888, &options) != 0) {
        LOG(ERROR) << "启动服务器失败";
        return -1;
    }
    
    LOG(INFO) << "司契权限系统启动成功，监听端口: 8888";
    LOG(INFO) << "其他系统可以通过 brpc://localhost:8888 调用";
    
    // 5. 运行直到收到停止信号
    server.RunUntilAskedToQuit();
    
    return 0;
}