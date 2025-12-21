#include <string>
#include<iostream>
#include<thread>
#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <sys/socket.h>
#include "handlers.h"
#include "state.h"
#include "resp_create.h"
#include "resp_send.h"
#include "StreamHandler.h"
#include "utils.h"
#include "stdc++.h"

using namespace std;

string handle_rpush(int client_fd,deque<string> &parsed_request)
{
  string key=parsed_request[1];
  lock_guard<mutex> lock1(lists_mutex);
  for (int i = 2; i < (int)parsed_request.size();i++)
  {
    lists[key].push_back(parsed_request[i]);
  }
  {
    lock_guard<mutex>lock2(blocked_clients_mutex);
    while(!blocked_clients[key].empty() && !lists[key].empty()){
      int client_fd=blocked_clients[key].front();
      blocked_clients[key].pop_front();
      clients_cvs[client_fd].notify_one();
      // this_thread::sleep_for(chrono::milliseconds(100));
    }
  }
  return create_integer((int)lists[key].size());
}
string handle_lpush(int client_fd,deque<string> &parsed_request)
{
  string key=parsed_request[1];
  lock_guard<mutex> lock1(lists_mutex);
  for (int i = 2; i < (int)parsed_request.size();i++)
  { 
    lists[key].push_front(parsed_request[i]);
  }
  {
    lock_guard<mutex>lock2(blocked_clients_mutex);
    while(!blocked_clients[key].empty() && !lists[key].empty()){
      int client_fd=blocked_clients[key].front();
      blocked_clients[key].pop_front();
      clients_cvs[client_fd].notify_one();
      // this_thread::sleep_for(chrono::milliseconds(100));
    }
  }
  
  return create_integer((int)lists[key].size());
}
string handle_lpop(int client_fd,std::deque<std::string> &parsed_request)
{
      string key = parsed_request[1];
      int no_of_removals = 1;
      if (parsed_request.size() > 2)
        no_of_removals = stoi(parsed_request[2]);
      bool key_exists = false;
      {
        lock_guard<mutex> lock(lists_mutex);
        if (lists.count(key))
          key_exists = true;
      }
      if (key_exists && no_of_removals == 1)
      {
          lock_guard<mutex> lock1(lists_mutex);
          string str = lists[key].front();
          lists[key].pop_front();
          return create_simple_string(str);
      }
      else if (key_exists && no_of_removals > 1)
      {
        lock_guard<mutex> lock1(lists_mutex);
        string resp_array = create_resp_array(lists[key], 0, no_of_removals - 1);
        for (int i = 0; i < no_of_removals; i++)
        {
          lists[key].pop_front();
        }
        return resp_array;
      }
      else
        return create_null_bulk_string();
}
void handle_multiple_lpop(int client_fd, string &key, int no_of_removals)
{
  // send(client_fd, resp_array.c_str(), resp_array.size(), 0);
}

string handle_blpop(int client_fd,deque<string>&parsed_request) {
    string key=parsed_request[1];
    float time=0;
    if(parsed_request.size()>2)
      time=stof(parsed_request[2]);

    
    unique_lock<mutex> lock(lists_mutex);
    if (lists[key].empty()) {
        {
            lock_guard<mutex> guard(blocked_clients_mutex);
            blocked_clients[key].push_back(client_fd);
        }

        auto &cv = clients_cvs[client_fd];

        if (time == 0) {
            cv.wait(lock, [&](){ return !lists[key].empty(); });
        } else {

            auto deadline = chrono::steady_clock::now() + chrono::milliseconds((int)(time*1000));
            bool has_data = cv.wait_until(lock, deadline, [&](){ return !lists[key].empty(); });

            if (!has_data) {
                // timeout: remove client from blocked list if still present
                {
                    lock_guard<mutex> guard(blocked_clients_mutex);
                    auto &dq = blocked_clients[key];
                    for (auto it = dq.begin(); it != dq.end(); ++it) {
                        if (*it == client_fd) { dq.erase(it); break; }
                    }
                }
                return create_null_array();
            }
        }
    }

    // At this point, we know lists[key] has at least one element

    string val = lists[key].front();
    lists[key].pop_front();

    deque<string> key_val = {key, val};
    // Avoid deadlock: release list lock before send_array (which also locks)
    return create_resp_array(key_val);
}


void set_key_value(string key, string value, int delay_time)
{
  lock_guard<mutex> lock1(kv_mutex);
  lock_guard<mutex> lock2(expiry_map_mutex);
  kv[key] = value;

  if (delay_time != -1)
  {
    expiry_map[key] = chrono::steady_clock::now() + chrono::milliseconds(delay_time);
  }
  else if (expiry_map.count(key))
  {
    expiry_map.erase(key);
  }
}

void remove_key(string key)
{
  lock_guard<mutex> lock1(kv_mutex);
  lock_guard<mutex> lock2(expiry_map_mutex);
  if (expiry_map.count(key) && chrono::steady_clock::now() >= expiry_map[key])
  {
    kv.erase(key);
    expiry_map.erase(key);
  }
}

string handle_type_of(int client_fd,deque<string>&parsed_request)
{
      string key=parsed_request[1];
  {
    lock_guard<mutex>lock(kv_mutex);
    if(kv.count(key))return create_simple_string("string");
  }
  {
    lock_guard<mutex>lock(lists_mutex);
    if(lists.count(key))return create_simple_string("list");
  }
  {
    lock_guard<mutex>lock(m_stream_mutex);
    if(StreamHandler_ptr->m_streams.count(key))return create_simple_string("stream");
  }
  // set zset hash vectorset
  return create_simple_string("none");
}
pair<deque<string>,deque<string>>get_stream_keys_ids(deque<string>&parsed_request){
  deque<string>streamKeys;
  deque<string>streamIds;
  to_lowercase(parsed_request[1]);
  if(parsed_request[1]!="block"){
    int n=(parsed_request.size()-2)/2+2;
    for(int i=2;i<(n);i++){
      streamKeys.push_back(parsed_request[i]);
      streamIds.push_back(parsed_request[n+(i-2)]);
    }
  }else{
    int n=(parsed_request.size()-4)/2+4;
    for(int i=4;i<(n);i++){
      streamKeys.push_back(parsed_request[i]);
      streamIds.push_back(parsed_request[n+(i-4)]);
    }
  }
  return {streamKeys,streamIds};
}
string get_stream_name(deque<string>&parsed_request){
  to_lowercase(parsed_request[1]);
  if(parsed_request[1]!="block"){
    return parsed_request[1];
  }else return parsed_request[3];
}


// std::unordered_map<std::string, std::function<string(std::deque<std::string>&)> >commandMap;

// std::unordered_map<std::string, std::function<std::string(const std::deque<std::string>&)>> commandMap;

// std::unordered_map<std::string, std::function<std::string(std::deque<std::string>)> > commandMap;



string handle_ping(int client_fd,deque<string>& parsed_request){
  return "PONG";
};


string handle_echo(int client_fd,deque<string>& parsed_request){
  string resp_bulk;
  resp_bulk = create_bulk_string(parsed_request[1]);
  return resp_bulk;
};

string handle_set(int client_fd,deque<string>&parsed_request){
  string key = parsed_request[1];
  string value = parsed_request[2];
  int delay_time = -1;
  if (parsed_request.size() > 3)
  {
    string optional_arg = parsed_request[3];
    to_lowercase(optional_arg);
    if (optional_arg == "px" || optional_arg == "ex")
    {
      delay_time = stoi(parsed_request[4]);
      if (optional_arg == "ex")
        delay_time *= 1000;
    }
  }
  set_key_value(key, value, delay_time);
  return create_simple_string("OK");
}

string handle_get(int client_fd,deque<string>&parsed_request){
  string key = parsed_request[1];
  bool send_empty = false;
  string value;
  {
    lock_guard<mutex> lock1(kv_mutex);
    lock_guard<mutex> lock2(expiry_map_mutex);
    if (kv.count(key))
    {
      if (!expiry_map.count(key) || chrono::steady_clock::now() < expiry_map[key])
        value = kv[key];
      else
        send_empty = true;
    }
    else
      send_empty = true;
  }
  if (send_empty)
    remove_key(key);
  if (!value.empty())
    return create_bulk_string(value);
  else
    return create_null_bulk_string();
}

  string handle_lrange(int client_fd,deque<string>&parsed_request){
    string list_key = parsed_request[1];
    int start = stoi(parsed_request[2]);
    int end = stoi(parsed_request[3]);
    if (start < 0)
      start = (int)lists[list_key].size() + start;
    if (end < 0)
      end = (int)lists[list_key].size() + end;
    if (start < 0)
      start = 0;
    if (end < 0)
      end = 0;
    if (lists.count(list_key) && start <= end && start < (int)lists[list_key].size())
    {  
      lock_guard<mutex>lock(lists_mutex);
      return create_resp_array(lists[list_key], start, end);
    }
    else{
      return create_empty_array();
    }
  }


  string handle_llen(int client_fd,deque<string>&parsed_request){
  string key = parsed_request[1];
  bool key_exists = false;
  {
    lock_guard<mutex> lock(lists_mutex);
    if (lists.count(key))
      key_exists = true;
  }
  if (key_exists)
    return create_integer((int)lists[key].size());
  else
    return create_integer(0);
}


string handle_xadd(int client_fd,deque<string>&parsed_request){
  return StreamHandler_ptr->xaddHandler(parsed_request);
}

string handle_xrange(int client_fd,deque<string>&parsed_request){
  return StreamHandler_ptr->xrangeHandler(parsed_request);
}

string handle_xread(int client_fd,deque<string>&parsed_request){
      string type=parsed_request[1];
      to_lowercase(type);
      if(type=="streams"){
        deque<string> resp_keys=StreamHandler_ptr->xreadHandler(client_fd,parsed_request,false);
        return create_resp_array(resp_keys,0,INT_MAX,true);
      }
      if(type=="block"){
        int time=stoi(parsed_request[2]);
        type=parsed_request[3];
        to_lowercase(type);
        if(type=="streams"){
          deque<string> resp_keys=StreamHandler_ptr->xreadBlockedHandler(client_fd,parsed_request);
          if(resp_keys.empty())return create_null_array();
          else return create_resp_array(resp_keys,0,INT_MAX,true);
        }
      }
}

string handle_incr(int client_fd,deque<string>&parsed_request){
      string key=parsed_request[1];
      return TransactionHandler_ptr->handleIncr(key);
}

string handle_multi(int client_fd,deque<string>&parsed_request){
      return TransactionHandler_ptr->handleMulti(client_fd);
}




