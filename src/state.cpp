#include "state.h"
// #include "TransactionHandler.h"
// #include "StreamHandler.h"
using namespace std;

std::unordered_map<std::string, std::string> kv;
std::unordered_map<std::string, std::chrono::steady_clock::time_point> expiry_map;
std::unordered_map<std::string, std::deque<std::string>> lists;
std::unordered_map<int, std::condition_variable> clients_cvs;
std::unordered_map<std::string, std::deque<int>> blocked_clients;
unordered_map<string,set<tuple<string,int>>>blocked_streams;
std::shared_ptr<StreamHandler>StreamHandler_ptr=std::make_shared<StreamHandler>();
std::shared_ptr<TransactionHandler>TransactionHandler_ptr=std::make_shared<TransactionHandler>();
std::shared_ptr<PubSubHandler>PubSubHandler_ptr=std::make_shared<PubSubHandler>();

std::unordered_map<std::string,string(*)(int,deque<string>&)>commandMap;
std::unordered_map<std::string,void(*)(int,deque<string>&)>pubSubCommandMap;



std::mutex kv_mutex, expiry_map_mutex, lists_mutex, clients_cvs_mutex, blocked_clients_mutex, m_stream_mutex, blocked_streams_mutex;



