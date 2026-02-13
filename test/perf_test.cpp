#include <brpc/channel.h>
#include <gflags/gflags.h>
#include <butil/time.h>
#include <butil/logging.h>
#include "auth.pb.h"
#include <vector>
#include <thread>
#include <atomic>
#include <numeric>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <random>

DEFINE_string(server, "127.0.0.1:8888", "Server address to connect");
DEFINE_int32(threads, 1, "Number of threads");
DEFINE_int32(duration, 30, "Duration in seconds to run");
DEFINE_bool(print_detail, false, "Print detailed latency for each thread");

// 简单的统计结构
struct ThreadStats {
    long count = 0;
    long success = 0;
    long fail = 0;
    long latency_sum_us = 0;
    std::vector<long> latencies; // 用于计算 P99等，注意内存使用
};

struct TestParam {
    std::string app_code;
    std::string perm_key;
};

const std::vector<TestParam> kTestParams = {
    {"qq_bot", "member:kick"},
    {"qq_bot", "member:mute"},
    {"qq_bot", "message:delete"},
    {"qq_bot", "message:pin"},
    {"admin_panel", "data:view"},
    {"admin_panel", "data:export"},
    {"admin_panel", "user:create"},
    {"admin_panel", "user:delete"},
    {"course_bot", "homework:assign"},
    {"course_bot", "homework:grade"}
};

void Worker(brpc::Channel* channel, int duration_s, ThreadStats* stats, int seed_offset) {
    siqi::auth::AuthService_Stub stub(channel);
    
    // 随机数生成器，用于随机 UserID，模拟一定的缓存穿透或随机访问
    std::mt19937 rng(std::random_device{}() + seed_offset);
    std::uniform_int_distribution<int> user_dist(100000, 310000); // 覆盖真实用户ID范围
    std::uniform_int_distribution<int> param_dist(0, kTestParams.size() - 1);

    long start_time_ms = butil::gettimeofday_ms();
    long end_time_ms = start_time_ms + duration_s * 1000;
    
    // 简单的 reporting
    long last_report_time = start_time_ms;
    long last_report_count = 0;

    stats->latencies.reserve(duration_s * 1000); // 预估每秒1000qps per thread

    while (true) {
        long current_ms = butil::gettimeofday_ms();
        if (current_ms >= end_time_ms) break;
        
        // 每秒打印一次进度 (仅第一个线程)
        if (seed_offset == 0 && (current_ms - last_report_time) >= 1000) {
            double qps = (stats->count - last_report_count) * 1000.0 / (current_ms - last_report_time);
            LOG(INFO) << "Progress: " << std::fixed << std::setprecision(1) 
                      << 100.0 * (current_ms - start_time_ms) / (duration_s * 1000) << "% "
                      << "Current QPS: " << qps;
            last_report_time = current_ms;
            last_report_count = stats->count;
        }

        const auto& param = kTestParams[param_dist(rng)];
        
        // 智能生成符合该应用范围的用户ID，提高命中率
        // qq_bot: 100000+, admin_panel: 200000+, course_bot: 300000+
        int base_id = 100000;
        if (param.app_code == "admin_panel") base_id = 200000;
        else if (param.app_code == "course_bot") base_id = 300000;
        
        // 生成范围在 base_id ~ base_id + 500 内，大概率命中真实存在的用户
        std::uniform_int_distribution<int> smart_user_dist(base_id, base_id + 500);

        siqi::auth::CheckRequest request;
        request.set_app_code(param.app_code);
        request.set_user_id(std::to_string(smart_user_dist(rng)));
        request.set_perm_key(param.perm_key);

        siqi::auth::CheckResponse response;
        brpc::Controller cntl;
        // 只有 100ms 超时，快速失败
        cntl.set_timeout_ms(500);

        long t1 = butil::gettimeofday_us();
        stub.Check(&cntl, &request, &response, NULL);
        long t2 = butil::gettimeofday_us();
        
        long latency = t2 - t1;
        stats->count++;
        stats->latencies.push_back(latency);
        stats->latency_sum_us += latency;
        
        if (!cntl.Failed()) {
            stats->success++;
        } else {
            // 失败时稍微sleep一下，避免疯狂报错
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            stats->fail++;
            if (FLAGS_print_detail && stats->fail <= 10) {
                 LOG(WARNING) << "RPC Failed: " << cntl.ErrorText();
            }
        }
    }
}

int main(int argc, char* argv[]) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    
    // 初始化 Channel，所有线程共享一个 Channel 对象通常是最佳实践（bRPC内部有连接池）
    brpc::Channel channel;
    brpc::ChannelOptions options;
    options.protocol = "baidu_std";
    options.connection_type = "pooled"; // 使用连接池
    options.timeout_ms = 1000;
    options.max_retry = 3;

    if (channel.Init(FLAGS_server.c_str(), &options) != 0) {
        LOG(ERROR) << "Fail to initialize channel";
        return -1;
    }

    LOG(INFO) << "Starting performance test on " << FLAGS_server 
              << " with " << FLAGS_threads << " threads for " << FLAGS_duration << " seconds...";

    std::vector<std::thread> threads;
    std::vector<ThreadStats> all_stats(FLAGS_threads);

    long start_time = butil::gettimeofday_ms();

    for (int i = 0; i < FLAGS_threads; ++i) {
        threads.emplace_back(Worker, &channel, FLAGS_duration, &all_stats[i], i);
    }

    for (auto& t : threads) {
        t.join();
    }

    long end_time = butil::gettimeofday_ms();
    double actual_duration_s = (end_time - start_time) / 1000.0;

    // 汇总数据
    long total_req = 0;
    long total_success = 0;
    long total_fail = 0;
    long total_latency_us = 0;
    std::vector<long> all_latencies;

    for (const auto& s : all_stats) {
        total_req += s.count;
        total_success += s.success;
        total_fail += s.fail;
        total_latency_us += s.latency_sum_us;
        all_latencies.insert(all_latencies.end(), s.latencies.begin(), s.latencies.end());
    }

    std::sort(all_latencies.begin(), all_latencies.end());
    
    long p50 = all_latencies.empty() ? 0 : all_latencies[all_latencies.size() * 0.50];
    long p90 = all_latencies.empty() ? 0 : all_latencies[all_latencies.size() * 0.90];
    long p99 = all_latencies.empty() ? 0 : all_latencies[all_latencies.size() * 0.99];
    long p999 = all_latencies.empty() ? 0 : all_latencies[all_latencies.size() * 0.999];
    double avg_latency = total_req == 0 ? 0.0 : (double)total_latency_us / total_req / 1000.0; // ms

    double qps = total_req / actual_duration_s;

    std::cout << "\n========================================================" << std::endl;
    std::cout << "Performance Test Result" << std::endl;
    std::cout << "========================================================" << std::endl;
    std::cout << "Server      : " << FLAGS_server << std::endl;
    std::cout << "Threads     : " << FLAGS_threads << std::endl;
    std::cout << "Duration    : " << actual_duration_s << " s" << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;
    std::cout << "QPS         : " << std::fixed << std::setprecision(2) << qps << " Req/s" << std::endl;
    std::cout << "Total Req   : " << total_req << std::endl;
    std::cout << "Success     : " << total_success << " (" << (total_req > 0 ? 100.0 * total_success / total_req : 0) << "%)" << std::endl;
    std::cout << "Failed      : " << total_fail << " (" << (total_req > 0 ? 100.0 * total_fail / total_req : 0) << "%)" << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;
    std::cout << "Avg Latency : " << avg_latency << " ms" << std::endl;
    std::cout << "P50 Latency : " << p50 / 1000.0 << " ms" << std::endl;
    std::cout << "P90 Latency : " << p90 / 1000.0 << " ms" << std::endl;
    std::cout << "P99 Latency : " << p99 / 1000.0 << " ms" << std::endl;
    std::cout << "P999 Latency: " << p999 / 1000.0 << " ms" << std::endl;
    std::cout << "========================================================" << std::endl;

    return 0;
}
