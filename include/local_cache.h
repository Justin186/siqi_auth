#ifndef LOCAL_CACHE_H
#define LOCAL_CACHE_H

#include <unordered_map>
#include <mutex>
#include <chrono>
#include <string>

// 一个简单的线程安全 TTL 缓存
// T 是存储的值类型
template <typename T>
class LocalCache {
public:
    struct Entry {
        T value;
        std::chrono::steady_clock::time_point expire_at;
    };

    // 写入缓存
    void Put(const std::string& key, const T& value, int ttl_seconds) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        cache_[key] = {value, now + std::chrono::seconds(ttl_seconds)};
    }

    // 读取缓存
    bool Get(const std::string& key, T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            return false;
        }
        if (std::chrono::steady_clock::now() > it->second.expire_at) {
            cache_.erase(it);
            return false;
        }
        value = it->second.value;
        return true;
    }

    // 移除单个 Key
    void Invalidate(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.erase(key);
    }

    // 移除匹配前缀的 Key (需要遍历，O(N)复杂度)
    // 适用于管理操作，例如：清除某个 App 下的所有用户缓存
    void InvalidatePrefix(const std::string& prefix) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = cache_.begin(); it != cache_.end(); ) {
            if (it->first.compare(0, prefix.size(), prefix) == 0) {
                 it = cache_.erase(it);
            } else {
                 ++it;
            }
        }
    }
    
    // 移除所有 Key (清空)
    void Clear() {
         std::lock_guard<std::mutex> lock(mutex_);
         cache_.clear();
    }

private:
    std::unordered_map<std::string, Entry> cache_;
    std::mutex mutex_;
};

#endif // LOCAL_CACHE_H
