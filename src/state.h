#pragma once

#include <string>
#include <unordered_map>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <queue>
#include <set>
#include "StreamHandler.h"
#include "TransactionHandler.h"

using namespace std;
extern std::unordered_map<std::string, std::string> kv;
extern std::unordered_map<std::string, std::chrono::steady_clock::time_point> expiry_map;
extern std::unordered_map<std::string, std::deque<std::string>> lists;
extern std::unordered_map<int, std::condition_variable> clients_cvs;
extern std::unordered_map<std::string, std::deque<int>> blocked_clients;
extern unordered_map<string,set<tuple<string,int>>>blocked_streams;
std::shared_ptr<StreamHandler>StreamHandler_ptr=std::make_shared<StreamHandler>();
std::shared_ptr<TransactionHandler>TransactionHandler_ptr=std::make_shared<TransactionHandler>();

extern std::mutex kv_mutex;
extern std::mutex expiry_map_mutex;
extern std::mutex lists_mutex;
extern std::mutex clients_cvs_mutex;
extern std::mutex blocked_clients_mutex;
extern std::mutex m_stream_mutex;
extern mutex blocked_streams_mutex;


