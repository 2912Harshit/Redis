#include <string>
#include<iostream>
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

using namespace std;

int handle_rpush(vector<string> &parsed_request, string &key)
{
  lock_guard<mutex> lock1(lists_mutex);
  lock_guard<mutex>lock2(blocked_clients_mutex);
  for (int i = 2; i < (int)parsed_request.size();i++)
  {
    cout<<"rpush added"<<endl;
    lists[key].push_back(parsed_request[i]);
  }
  while(!blocked_clients[key].empty() && !lists[key].empty()){
    cout<<"rpush blocked"<<endl;
    int client_fd=blocked_clients[key].front();
    blocked_clients[key].pop_front();
    clients_cvs[client_fd].notify_one();
  }
  cout<<"rpush size: "<<lists[key].size()<<endl;
  return (int)lists[key].size();
}
int handle_lpush(vector<string> &parsed_request, string &key)
{
  lock_guard<mutex> lock1(lists_mutex);
  lock_guard<mutex>lock2(blocked_clients_mutex);
  for (int i = 2; i < (int)parsed_request.size();i++)
  {
    lists[key].push_front(parsed_request[i]);
  }
  while(!blocked_clients[key].empty() && !lists[key].empty()){
    int client_fd=blocked_clients[key].front();
    blocked_clients[key].pop_front();
    clients_cvs[client_fd].notify_one();
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
  string resp_array = create_resp_array(client_fd, lists[key], 0, no_of_removals - 1);
  for (int i = 0; i < no_of_removals; i++)
  {
    lists[key].pop_front();
  }
  send(client_fd, resp_array.c_str(), resp_array.size(), 0);
}

void handle_blpop(int client_fd,string &key,int time)
{
  unique_lock<mutex>lock(lists_mutex);
  if(lists[key].empty()){
    clients_cvs[client_fd];
    blocked_clients[key].push_back(client_fd);
    if(time==0){
      cout<<"time 0"<<endl;

      clients_cvs[client_fd].wait(lock,[&](){return !lists[key].empty();});
      
    }
    else{ 
      auto deadline=chrono::steady_clock::now()+chrono::seconds(time);
      bool list_empty=clients_cvs[client_fd].wait_until(lock,deadline,[&](){return lists[key].empty();});
      if(list_empty){
        send_null_array(client_fd);
        return;
      }
    }
  }
  cout<<"yehe"<<endl;
  string val=lists[key].front();
  lists[key].pop_front();
  deque<string>key_val={key,val};
  send_array(client_fd,key_val);
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


