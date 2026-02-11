#include <brpc/channel.h>
#include <gflags/gflags.h>
#include "auth.pb.h"
#include <memory>
#include <iostream>

DEFINE_string(server, "127.0.0.1:8888", "Server Address");
DEFINE_string(app, "qq_bot", "App code");
DEFINE_string(user, "123456", "User ID");
DEFINE_string(perm, "member:kick", "Permission Key");

class AuthClient {
private:
    std::unique_ptr<brpc::Channel> channel_;
    std::unique_ptr<siqi::auth::AuthService_Stub> stub_;
    
public:
    AuthClient(const std::string& server_addr) {
        brpc::ChannelOptions options;
        options.timeout_ms = 1000;
        options.connect_timeout_ms = 3000;
        
        channel_ = std::make_unique<brpc::Channel>();
        if (channel_->Init(server_addr.c_str(), &options) != 0) {
            throw std::runtime_error("连接权限系统失败");
        }
        
        stub_ = std::make_unique<siqi::auth::AuthService_Stub>(channel_.get());
    }
    
    bool Check(const std::string& app_code, 
               const std::string& user_id, 
               const std::string& perm_key) {
        siqi::auth::CheckRequest request;
        request.set_app_code(app_code);
        request.set_user_id(user_id);
        request.set_perm_key(perm_key);
        
        siqi::auth::CheckResponse response;
        brpc::Controller cntl;
        
        stub_->Check(&cntl, &request, &response, NULL);
        
        if (cntl.Failed()) {
            std::cerr << "RPC Failed: " << cntl.ErrorText() << std::endl;
            return false;
        }
        
        if (!response.reason().empty()) {
            std::cout << "Reason: " << response.reason() << std::endl;
        }

        return response.allowed();
    }
};

int main(int argc, char* argv[]) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    
    try {
        AuthClient client(FLAGS_server);
        bool allowed = client.Check(FLAGS_app, FLAGS_user, FLAGS_perm);
        
        if (allowed) {
            std::cout << "✅ 允许访问 [ALLOWED]" << std::endl;
        } else {
            std::cout << "❌ 拒绝访问 [DENIED]" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}