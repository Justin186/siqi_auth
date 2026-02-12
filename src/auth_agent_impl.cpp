#include "auth_agent.h"
#include <butil/logging.h>

AuthAgentImpl::AuthAgentImpl(brpc::Channel* channel, int cache_ttl, int timeout_ms) 
    : stub_(new siqi::auth::AuthService_Stub(channel)),
      cache_ttl_(cache_ttl),
      timeout_ms_(timeout_ms) {
    cache_ = std::make_shared<LocalCache<bool>>();
}

void AuthAgentImpl::Check(google::protobuf::RpcController* cntl_base,
                   const siqi::auth::CheckRequest* request,
                   siqi::auth::CheckResponse* response,
                   google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);

    // 0. 提取参数 (优先读 PB Request，若为空则读 URL QueryString)
    std::string app_code = request->app_code();
    std::string user_id = request->user_id();
    std::string perm_key = request->perm_key();

    if (app_code.empty()) {
        const std::string* p = cntl->http_request().uri().GetQuery("app_code");
        if (p) app_code = *p;
    }
    if (user_id.empty()) {
        const std::string* p = cntl->http_request().uri().GetQuery("user_id");
        if (p) user_id = *p;
    }
    if (perm_key.empty()) {
        const std::string* p = cntl->http_request().uri().GetQuery("perm_key");
        if (p) perm_key = *p;
    }

    // 1. 参数校验
    if (app_code.empty() || user_id.empty() || perm_key.empty()) {
        LOG(WARNING) << "Agent 收到非法请求: " 
                     << " app_code=" << app_code
                     << " user_id=" << user_id
                     << " perm_key=" << perm_key
                     << " from=" << cntl->remote_side();
        
        response->set_allowed(false);
        response->set_reason("参数不完整 (Agent)");
        return;
    }

    // 2. 查本地缓存
    // Key 格式: "app:user:perm"
    std::string cache_key = app_code + ":" + user_id + ":" + perm_key;
    bool allowed = false;
    
    if (cache_->Get(cache_key, allowed)) {
        response->set_allowed(allowed);
        // 可以在 HTTP Header 里加个标记，方便调试
        cntl->http_response().SetHeader("X-Cache", "HIT");
        return;
    }

    // 3. 缓存未命中，发起远程 RPC
    brpc::Controller remote_cntl;
    remote_cntl.set_timeout_ms(timeout_ms_);
    
    // 构造实际的 RPC 请求（因为原始 request 可能为空，需要用我们解析出来的字段重组）
    siqi::auth::CheckRequest remote_request;
    remote_request.set_app_code(app_code);
    remote_request.set_user_id(user_id);
    remote_request.set_perm_key(perm_key);
    
    siqi::auth::CheckResponse remote_response;
    stub_->Check(&remote_cntl, &remote_request, &remote_response, NULL);
    
    if (remote_cntl.Failed()) {
        // RPC 失败 (网络问题/Server挂了)
        // 策略: 默认拒绝 (Fail-Secure)
        response->set_allowed(false);
        response->set_reason("IAM Server Unavailable: " + remote_cntl.ErrorText());
        cntl->http_response().SetHeader("X-Cache", "ERROR");
        return;
    }
    
    // 4. 写回缓存
    cache_->Put(cache_key, remote_response.allowed(), cache_ttl_);
    
    // 5. 返回结果
    response->CopyFrom(remote_response);
    cntl->http_response().SetHeader("X-Cache", "MISS");
}
