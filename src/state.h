#pragma once

#include "stdc++.h"
#include "StreamHandler.h"
#include "TransactionHandler.h"

using namespace std;

class StreamHandler;
class TransactionHandler;

extern std::unordered_map<std::string, std::string> kv;
extern std::unordered_map<std::string, std::chrono::steady_clock::time_point> expiry_map;
extern std::unordered_map<std::string, std::deque<std::string>> lists;
extern std::unordered_map<int, std::condition_variable> clients_cvs;
extern std::unordered_map<std::string, std::deque<int>> blocked_clients;
extern unordered_map<string,set<tuple<string,int>>>blocked_streams;
std::shared_ptr<StreamHandler>StreamHandler_ptr;
std::shared_ptr<TransactionHandler>TransactionHandler_ptr;

extern std::mutex kv_mutex;
extern std::mutex expiry_map_mutex;
extern std::mutex lists_mutex;
extern std::mutex clients_cvs_mutex;
extern std::mutex blocked_clients_mutex;
extern std::mutex m_stream_mutex;
extern mutex blocked_streams_mutex;


