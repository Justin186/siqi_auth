#include <brpc/channel.h>
#include "auth.pb.h"
#include <memory>

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
            // 降级策略：如果权限系统不可用，默认允许还是拒绝？
            // 根据业务决定，这里先记录日志
            LOG(WARNING) << "权限系统不可用: " << cntl.ErrorText();
            return false;  // 安全起见，拒绝访问
        }
        
        return response.allowed();
    }
};

// QQ Bot中这样使用：
int main() {
    // 初始化权限客户端
    AuthClient auth_client("localhost:8888");
    
    // 用户尝试踢人时
    if (auth_client.Check("qq_bot", "123456", "member:kick")) {
        std::cout << "允许踢人" << std::endl;
        // 执行踢人操作
    } else {
        std::cout << "没有踢人权限" << std::endl;
        // 提示用户没有权限
    }
    
    return 0;
}