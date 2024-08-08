#pragma once
#include "redis_pool.h"
#include <string>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

class DistributedLock {
public:
    DistributedLock(const std::string& key, int timeout){
        key_ = key;
        boost::uuids::uuid uuid = boost::uuids::random_generator()();
        value_ = boost::uuids::to_string(uuid);
        timeout_ = timeout;
        locked_ = false;
    }
    ~DistributedLock(){
        if (locked_) {
            unlock();
        }
    }
    bool try_lock(){

        auto reply = redis_conn_.setnx(key_, value_, timeout_);

        if (reply == nullptr || reply->type == REDIS_REPLY_NIL) {
            return false;
        }
        locked_ = true;
        return true;
    }
    void unlock(){
        auto reply = redis_conn_.unlock(key_, value_);
        return;
    }
private:
    RedisConnRAII redis_conn_;
    std::string key_;
    std::string value_;
    bool locked_;
    int timeout_;
};