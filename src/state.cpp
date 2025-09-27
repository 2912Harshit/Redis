#include "state.h"

std::unordered_map<std::string, std::string> kv;
std::unordered_map<std::string, std::chrono::steady_clock::time_point> expiry_map;
std::unordered_map<std::string, std::deque<std::string>> lists;
std::unordered_map<int, std::condition_variable> clients_cvs;
std::unordered_map<std::string, std::deque<int>> blocked_clients;


std::mutex kv_mutex, expiry_map_mutex, lists_mutex, clients_cvs_mutex, blocked_clients_mutex;
std::recursive_mutex m_stream_recursive_mutex;


