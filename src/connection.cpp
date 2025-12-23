#include<iostream>
#include <string>
#include <vector>
#include <mutex>
#include <sys/socket.h>
#include <unistd.h>
#include "connection.h"
#include "parser.h"
#include "state.h"
// #include "handlers.h"
#include "resp_send.h"
#include "utils.h"
// #include "StreamHandler.h"
#include "resp_create.h"
// #include "TransactionHandler.h"

using namespace std;



void handleResponse(int client_fd)
{
  char buffer[1024];

  while (true)
  {
    int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0)
    {
      close(client_fd);
      return;
    }
    buffer[bytes_read] = '\0';
    deque<string> parsed_request = parse_redis_command(buffer);
    if(parsed_request.empty()) continue;
    cout<<"parsed_request: "<<parsed_request[0]<<" "<<client_fd<<endl;
    string command = parsed_request[0];
    to_lowercase(command);
    string response="";

    if(pubSubCommandMap.count(command) || PubSubHandler_ptr->checkClient(client_fd)){
      if(pubSubCommandMap.count(command)){
        pubSubCommandMap[command](client_fd,parsed_request);
        continue;
      }else{
        string msg="Can't execute '"+command+"': only (P|S)SUBSCRIBE / (P|S)UNSUBSCRIBE / PING / QUIT / RESET are allowed in this context";
        response=create_simple_error(msg);
      }
    }else{   
      if(command!="exec" && command!="discard" && TransactionHandler_ptr->checkClient(client_fd)){
        response=TransactionHandler_ptr->addRequest(client_fd,parsed_request);
      }
      else response=commandMap[command](client_fd,parsed_request);

      send(client_fd,response.c_str(),response.size(),0);
    }

    // if(TransactionHandler_ptr->checkClient(client_fd) && command!="exec")
    // {
    //   TransactionHandler_ptr->addRequest(client_fd,parsed_request);
    // }
    // else if(command=="exec")
    // {
    //   TransactionHandler_ptr->handleExec(client_fd);
    // }

    // else if (command == "ping")
    // {
    //   send_simple_string(client_fd, "PONG");
    // }
    // else if (command == "echo")
    // {
    //   send_bulk_string(client_fd, parsed_request[1]);
    // }
    // else if (command == "set")
    // {
    //   string key = parsed_request[1];
    //   string value = parsed_request[2];
    //   int delay_time = -1;
    //   if (parsed_request.size() > 3)
    //   {
    //     string optional_arg = parsed_request[3];
    //     to_lowercase(optional_arg);
    //     if (optional_arg == "px" || optional_arg == "ex")
    //     {
    //       delay_time = stoi(parsed_request[4]);
    //       if (optional_arg == "ex")
    //         delay_time *= 1000;
    //     }
    //   }
    //   set_key_value(key, value, delay_time);
    //   send_simple_string(client_fd, "OK");
    // }

    // else if (command == "get")
    // {
    //   string key = parsed_request[1];
    //   bool send_empty = false;
    //   string value;
    //   {
    //     lock_guard<mutex> lock1(kv_mutex);
    //     lock_guard<mutex> lock2(expiry_map_mutex);
    //     if (kv.count(key))
    //     {
    //       if (!expiry_map.count(key) || chrono::steady_clock::now() < expiry_map[key])
    //         value = kv[key];
    //       else
    //         send_empty = true;
    //     }
    //     else
    //       send_empty = true;
    //   }
    //   if (send_empty)
    //     remove_key(key);
    //   if (!value.empty())
    //     send_bulk_string(client_fd, value);
    //   else
    //     send_null_bulk_string(client_fd);
    // }
    // else if (command == "rpush")
    // {
    //   string list_key = parsed_request[1];
    //   send_integer(client_fd, handle_rpush(parsed_request));
    // }

    // else if (command == "lrange")
    // {
    //   string list_key = parsed_request[1];
    //   int start = stoi(parsed_request[2]);
    //   int end = stoi(parsed_request[3]);
    //   if (start < 0)
    //     start = (int)lists[list_key].size() + start;
    //   if (end < 0)
    //     end = (int)lists[list_key].size() + end;
    //   if (start < 0)
    //     start = 0;
    //   if (end < 0)
    //     end = 0;
    //   if (lists.count(list_key) && start <= end && start < (int)lists[list_key].size())
    //   {  
    //     lock_guard<mutex>lock(lists_mutex);
    //     send_array(client_fd, lists[list_key], start, end);
    //   }
    //   else{
    //     send_empty_array(client_fd);
    //   }


    // }
    // else if (command == "lpush")
    // {
    //   string list_key = parsed_request[1];
    //   send_integer(client_fd, handle_lpush(parsed_request));
    // }



    // else if (command == "llen")
    // {
    //   string key = parsed_request[1];
    //   bool key_exists = false;
    //   {
    //     lock_guard<mutex> lock(lists_mutex);
    //     if (lists.count(key))
    //       key_exists = true;
    //   }
    //   if (key_exists)
    //     send_integer(client_fd, (int)lists[key].size());
    //   else
    //     send_integer(client_fd, 0);
    // }



    // else if (command == "lpop")
    // {
    //   string key = parsed_request[1];
    //   int no_of_removals = 1;
    //   if (parsed_request.size() > 2)
    //     no_of_removals = stoi(parsed_request[2]);
    //   bool key_exists = false;
    //   {
    //     lock_guard<mutex> lock(lists_mutex);
    //     if (lists.count(key))
    //       key_exists = true;
    //   }
    //   if (key_exists && no_of_removals >= 1)
    //     send_bulk_string(client_fd, handle_lpop(parsed_request));
    //   else
    //     send_null_bulk_string(client_fd);
    // }

    // // continue from here
    // else if(command=="blpop"){
    //   // string key=parsed_request[1];
    //   // float time=0;
    //   // if(parsed_request.size()>2)
    //   //   time=stof(parsed_request[2]);
    //   string res=handle_blpop(client_fd,parsed_request);
    //   send(client_fd,res.c_str(),res.size(),0);
    // }else if(command == "type"){
    //   string key=parsed_request[1];
    //   send_simple_string(client_fd,handle_type_of(parsed_request));
    // }
    // else if(command=="xadd"){
    //   string resp=StreamHandler_ptr->xaddHandler(parsed_request);
    //   send(client_fd,resp.c_str(),resp.size(),0);
    // }
    // else if(command=="xrange"){
    //   string resp=StreamHandler_ptr->xrangeHandler(parsed_request);
    //   send(client_fd,resp.c_str(),resp.size(),0);
    // }
    // else if(command=="xread"){
    //   string type=parsed_request[1];
    //   to_lowercase(type);
    //   if(type=="streams"){
    //     deque<string> resp_keys=StreamHandler_ptr->xreadHandler(client_fd,parsed_request,false);
    //     send_array(client_fd,resp_keys,0,INT_MAX,true);
    //   }
    //   if(type=="block"){
    //     int time=stoi(parsed_request[2]);
    //     type=parsed_request[3];
    //     to_lowercase(type);
    //     if(type=="streams"){
    //       deque<string> resp_keys=StreamHandler_ptr->xreadBlockedHandler(client_fd,parsed_request);
    //       if(resp_keys.empty())send_null_array(client_fd);
    //       else send_array(client_fd,resp_keys,0,INT_MAX,true);
    //     }
    //   }

    // }
    // else if(command=="incr"){
    //   string key=parsed_request[1];
    //   TransactionHandler_ptr->handleIncr(client_fd,key);
    // }
    // else if(command=="multi"){
    //   TransactionHandler_ptr->handleMulti(client_fd);
    // }
    

  }
}


