#include <brpc/server.h>
#include <brpc/channel.h>
#include <gflags/gflags.h>
#include "auth_agent.h"

DEFINE_int32(port, 8881, "Agent 监听端口 (提供给本机应用调用)");
DEFINE_string(server, "127.0.0.1:8888", "远程 Auth Server 地址");
DEFINE_int32(cache_ttl, 60, "本地缓存有效期 (秒)");
DEFINE_int32(timeout_ms, 500, "RPC 调用超时 (毫秒)");

int main(int argc, char* argv[]) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    
    // 1. 初始化连向 Auth Server 的 Channel
    brpc::Channel channel;
    brpc::ChannelOptions ch_options;
    ch_options.protocol = "baidu_std"; // 与 Server 之间推荐使用二进制协议，最高效
    ch_options.timeout_ms = FLAGS_timeout_ms;
    ch_options.max_retry = 3;
    
    if (channel.Init(FLAGS_server.c_str(), &ch_options) != 0) {
        LOG(ERROR) << "无法初始化 Channel, Server 地址: " << FLAGS_server;
        return -1;
    }
    
    // 2. 启动 Agent Server
    brpc::Server server;
    AuthAgentImpl agent_service(&channel, FLAGS_cache_ttl, FLAGS_timeout_ms);
    
    if (server.AddService(&agent_service, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "添加 AgentService 失败";
        return -1;
    }
    
    brpc::ServerOptions options;
    // Agent 通常只允许本机访问，所以这里可以只监听 127.0.0.1 提高安全性
    // 但为了演示方便，这里还是监听了所有
    if (server.Start(FLAGS_port, &options) != 0) {
        LOG(ERROR) << "启动 Agent 失败";
        return -1;
    }
    
    LOG(INFO) << "Siqi Auth Agent 已启动!";
    LOG(INFO) << "  - 监听端口 (Local): " << FLAGS_port;
    LOG(INFO) << "  - 上游 Server: " << FLAGS_server;
    LOG(INFO) << "本机应用可通过 HTTP 接口调用: ";
    LOG(INFO) << "  curl 'http://127.0.0.1:" << FLAGS_port << "/AuthService/Check?app_code=qq_bot&user_id=123&perm_key=member:kick'";

    server.RunUntilAskedToQuit();
    return 0;
}
