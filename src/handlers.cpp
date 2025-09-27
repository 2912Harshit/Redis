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

using namespace std;

int handle_rpush(vector<string> &parsed_request, string &key)
{
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
  return (int)lists[key].size();
}
int handle_lpush(vector<string> &parsed_request, string &key)
{
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
  
  return (int)lists[key].size();
}
string handle_lpop(string &key)
{
  lock_guard<mutex> lock1(lists_mutex);
  string str = lists[key].front();
  lists[key].pop_front();
  return str;
}
void handle_multiple_lpop(int client_fd, string &key, int no_of_removals)
{
  lock_guard<mutex> lock1(lists_mutex);
  string resp_array = create_resp_array(lists[key], 0, no_of_removals - 1);
  for (int i = 0; i < no_of_removals; i++)
  {
    lists[key].pop_front();
  }
  send(client_fd, resp_array.c_str(), resp_array.size(), 0);
}

void handle_blpop(int client_fd, string &key, float time) {
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
                send_null_array(client_fd);
                return;
            }
        }
    }

    // At this point, we know lists[key] has at least one element

    string val = lists[key].front();
    lists[key].pop_front();

    deque<string> key_val = {key, val};
    // Avoid deadlock: release list lock before send_array (which also locks)
    send_array(client_fd, key_val);
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
    expiry_map.erase(key);
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

string handle_type_of(string key,std::shared_ptr<StreamHandler>&StreamHandler_ptr)
{
  {
    lock_guard<mutex>lock(kv_mutex);
    if(kv.count(key))return "string";
  }
  {
    lock_guard<mutex>lock(lists_mutex);
    if(lists.count(key))return "list";
  }
  {
    lock_guard<mutex>lock(m_stream_mutex);
    if(StreamHandler_ptr->m_streams.count(key))return "stream";
  }
  // set zset hash vectorset
  return "none";
}


