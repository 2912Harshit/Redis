#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <thread>
#include <algorithm>
#include <cctype>
#include <vector>
#include <ranges>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <algorithm>
#include <deque>
#include <condition_variable>
using namespace std;

unordered_map<string, string> kv;
unordered_map<string, chrono::steady_clock::time_point> expiry_map;
unordered_map<string, deque<string>> lists;
unordered_map<int,condition_variable>clients_cvs;
unordered_map<string,deque<int>>blocked_clients;
mutex kv_mutex, expiry_map_mutex, lists_mutex, clients_cvs_mutex, blocked_clients_mutex;

string create_simple_string(string &msg)
{
  return "+" + msg + "\r\n";
}
string create_bulk_string(string &msg)
{
  return "$" + to_string(msg.size()) + "\r\n" + msg + "\r\n";
}
string create_integer(int &msg)
{
  return ":" + to_string(msg) + "\r\n";
}
string create_resp_array(int client_fd, deque<string> &list, int start = 0, int end = INT_MAX)
{
  if (end > list.size() - 1)
    end = list.size() - 1;
  string resp_array = "*" + to_string(end - start + 1) + "\r\n";
  for (int i = start; i <= end; i++)
  {
    cout << list[i] << " ";
    resp_array.append(create_bulk_string(list[i]));
  }
  cout << endl;
  return resp_array;
}
void start_expiry_cleaner()
{
  thread([]()
         {
    while(true){
      {
        lock_guard<mutex>lock1(kv_mutex);
        lock_guard<mutex>lock2(expiry_map_mutex);
        auto now=chrono::steady_clock::now();
        for(auto it=expiry_map.begin();it!=expiry_map.end();){
          if(now>=it->second){
            kv.erase(it->first);
            it=expiry_map.erase(it);
          }else ++it;
        }
      }
      this_thread::sleep_for(chrono::milliseconds(100));
    } })
      .detach();
}

vector<string> parse_redis_command(const string &buffer)
{
  vector<string> tokens;
  size_t pos = 0;

  if (buffer.empty() || buffer[pos] != '*')
    return tokens;

  // Read number of arguments
  size_t end_line = buffer.find("\r\n", pos);
  int num_args = stoi(buffer.substr(pos + 1, end_line - pos - 1));
  pos = end_line + 2;

  for (int i = 0; i < num_args; ++i)
  {
    if (buffer[pos] != '$')
      break; // Invalid format
    end_line = buffer.find("\r\n", pos);
    int len = stoi(buffer.substr(pos + 1, end_line - pos - 1));
    pos = end_line + 2;

    tokens.push_back(buffer.substr(pos, len));
    pos += len + 2; // Skip string + \r\n
  }

  return tokens;
}

int handle_rpush(vector<string> &parsed_request, string &key)
{
  lock_guard<mutex> lock1(lists_mutex);
  for (int i = 2; i < parsed_request.size();)
  {
    if(!blocked_clients[key].empty() && !lists[key].empty()){
      int client_fd=blocked_clients[key].front();
      blocked_clients[key].pop_front();
      clients_cvs[client_fd].notify_one();
    }
    else lists[key].push_back(parsed_request[i++]);
  }
  return lists[key].size();
}
int handle_lpush(vector<string> &parsed_request, string &key)
{
  lock_guard<mutex> lock1(lists_mutex);
  lock_guard<mutex>lock2(blocked_clients_mutex);
  for (int i = 2; i < parsed_request.size();)
  {
    if(!blocked_clients[key].empty() && !lists[key].empty()){
      int client_fd=blocked_clients[key].front();
      blocked_clients[key].pop_front();
      clients_cvs[client_fd].notify_one();
    }
    else lists[key].push_front(parsed_request[i++]);
  }
  return lists[key].size();
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

void handle_blpop(int client_fd,string &key,int time){
  unique_lock<mutex>lock(lists_mutex);
  if(lists[key].empty()){
    condition_variable cv;
    clients_cvs[client_fd] = &cv;
    blocked_clients[key].push_back(client_fd);
    auto deadline=chrono::steady_clock::now()+chrono::seconds(time);
    bool list_empty=cv.wait_until(lock,deadline,[&](){return lists[key].empty();});
    if(list_empty){
      send_null_array(client_fd);
      return;
    }
  }
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

// void set_timeout(int delayMs,string key){
//   thread([delayMs,key](){
//     this_thread::sleep_for(chrono::milliseconds(delayMs));
//     remove_key(key);
//   }).detach();
// }

void to_lowercase(string &str)
{
  transform(str.begin(), str.end(), str.begin(),
            [](unsigned char c)
            { return tolower(c); });
}
void send_simple_string(int client_fd, string msg)
{
  string resp_simple;
  resp_simple = create_simple_string(msg);
  send(client_fd, resp_simple.c_str(), resp_simple.size(), 0);
}
void send_bulk_string(int client_fd, string msg)
{
  string resp_simple;
  resp_simple = create_bulk_string(msg);
  send(client_fd, resp_simple.c_str(), resp_simple.size(), 0);
}
void send_integer(int client_fd, int msg)
{
  string resp_simple;
  resp_simple = create_integer(msg);
  send(client_fd, resp_simple.c_str(), resp_simple.size(), 0);
}

void send_null_string(int client_fd)
{
  string resp_simple = "$-1\r\n";
  send(client_fd, resp_simple.c_str(), resp_simple.size(), 0);
}
void send_null_array(int client_fd)
{
  string resp_null_array = "*0\r\n";
  send(client_fd, resp_null_array.c_str(), resp_null_array.size(), 0);
}
void send_array(int client_fd, deque<string> &list, int start = 0, int end = INT_MAX)
{
  lock_guard<mutex> lock1(lists_mutex);
  string resp_array = create_resp_array(client_fd, list, start, end);
  send(client_fd, resp_array.c_str(), resp_array.size(), 0);
}

void handleResponse(int client_fd)
{
  char buffer[1024];
  while (true)
  {
    int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0)
    {
      cout << "failed to read payload." << endl;
      close(client_fd);
      return;
    }
    buffer[bytes_read] = '\0';
    vector<string> parsed_request = parse_redis_command(buffer);
    string command = parsed_request[0];
    to_lowercase(command);
    string response = "";
    if (command == "ping")
    {
      send_simple_string(client_fd, "PONG");
    }
    else if (command == "echo")
    {
      send_bulk_string(client_fd, parsed_request[1]);
    }
    else if (command == "set")
    {
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
      send_simple_string(client_fd, "OK");
    }
    else if (command == "get")
    {
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
            send_empty = true; // mark to remove later
        }
        else
          send_empty = true;
      }
      if (send_empty)
        remove_key(key); // lock again inside remove_key is fine
      if (!value.empty())
        send_bulk_string(client_fd, value);
      else
        send_null_string(client_fd);
    }
    else if (command == "rpush")
    {
      string list_key = parsed_request[1];
      send_integer(client_fd, handle_rpush(parsed_request, list_key));
    }
    else if (command == "lrange")
    {
      string list_key = parsed_request[1];
      int start = stoi(parsed_request[2]);
      int end = stoi(parsed_request[3]);
      if (start < 0)
        start = lists[list_key].size() + start;
      if (end < 0)
        end = lists[list_key].size() + end;
      if (start < 0)
        start = 0;
      if (end < 0)
        end = 0;
      if (lists.count(list_key) && start <= end && start < lists[list_key].size())
      {
        send_array(client_fd, lists[list_key], start, end);
      }
      else
        send_null_array(client_fd);
    }
    else if (command == "lpush")
    {
      string list_key = parsed_request[1];
      send_integer(client_fd, handle_lpush(parsed_request, list_key));
    }
    else if (command == "llen")
    {
      string key = parsed_request[1];
      bool key_exists = false;
      {
        lock_guard<mutex> lock(lists_mutex);
        if (lists.count(key))
          key_exists = true;
      }
      if (key_exists)
        send_integer(client_fd, lists[key].size());
      else
        send_integer(client_fd, 0);
    }
    else if (command == "lpop")
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
        send_bulk_string(client_fd, handle_lpop(key));
      else if (key_exists && no_of_removals > 1)
        handle_multiple_lpop(client_fd, key, no_of_removals);
      else
        send_null_string(client_fd);
    }
    else if(command=="blpop"){
      string key=parsed_request[1];
      handle_blpop(client_fd,key);
    }
  }
}

int main(int argc, char **argv)
{
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0)
  {
    std::cerr << "Failed to create server socket\n";
    return 1;
  }

  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
  {
    std::cerr << "setsockopt failed\n";
    return 1;
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(6379);

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
  {
    std::cerr << "Failed to bind to port 6379\n";
    return 1;
  }

  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0)
  {
    std::cerr << "listen failed\n";
    return 1;
  }

  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);
  std::cout << "Waiting for a client to connect...\n";

  // You can use print statements as follows for debugging, they'll be visible when running tests.
  std::cout << "Logs from your program will appear here!\n";

  // Uncomment this block to pass the first stage
  //
  start_expiry_cleaner();
  while (true)
  {
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
    std::cout << "Client connected : " << client_fd << "\n";
    thread client_thread(handleResponse, client_fd);
    client_thread.detach();
  }
  close(server_fd);

  return 0;
}
