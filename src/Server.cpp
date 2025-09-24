#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include<thread>
#include <algorithm>
#include <cctype>
#include<vector>
#include<ranges>
#include<unordered_map>
#include<mutex>
#include<functional>
using namespace std;

unordered_map<string,string>kv;
unordered_map<string,chrono::steady_clock::time_point>expiry_map;
unordered_map<string,vector<string>>lists;
mutex kv_mutex,expiry_map_mutex,lists_mutex;
void start_expiry_cleaner(){
  thread([](){
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
    }
  }).detach();
}

int handle_rpush(vector<string>&parsed_request,string key){
  // lock_guard<mutex>lock1(lists_mutex);
  for(int i=2;i<parsed_request.size();i++){
    lists[key].push_back(parsed_request[i]);
  }
  for(string str:lists[key])cout<<str<<" ";
  cout<<lists[key].size()<<endl;
  return lists[key].size();
}

void set_key_value(string key,string value,int delay_time){
    lock_guard<mutex>lock1(kv_mutex);
    lock_guard<mutex>lock2(expiry_map_mutex);
    kv[key]=value;
    
    if(delay_time!=-1){
      expiry_map[key]=chrono::steady_clock::now()+chrono::milliseconds(delay_time);
    }else if(expiry_map.count(key))expiry_map.erase(key);
}

void remove_key(string key){
    lock_guard<mutex>lock1(kv_mutex);
    lock_guard<mutex>lock2(expiry_map_mutex);
    if(expiry_map.count(key) && chrono::steady_clock::now()>=expiry_map[key]){
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

// string token_to_resp_bulk(string token){
//   string res="";
//   if(token.empty()){
//     res= "$-1\r\n";
//   }else if(token=="OK" || token=="PONG"){
//     res="+"+token+"\r\n";
//   }
//   else res= "$"+to_string(token.size())+"\r\n"+token+"\r\n";
//   return res;
// }

void to_lowercase(string &str){
    transform(str.begin(),str.end(),str.begin(),
                  [](unsigned char c){return tolower(c);});
    
}
void send_simple_string(int client_fd,string msg){
  string resp_simple;
  resp_simple="+"+msg+"\r\n";
  send(client_fd,resp_simple.c_str(),resp_simple.size(),0);
}
void send_bulk_string(int client_fd,string msg){
  string resp_simple;
  resp_simple="$"+to_string(msg.size())+"\r\n"+msg+"\r\n";
  send(client_fd,resp_simple.c_str(),resp_simple.size(),0);
}
void send_integer(int client_fd,int msg){
  string resp_simple;
  resp_simple=":"+to_string(msg)+"\r\n";
  send(client_fd,resp_simple.c_str(),resp_simple.size(),0);
}

void send_null_string(int client_fd){
  string resp_simple="$-1\r\n";
  send(client_fd,resp_simple.c_str(),resp_simple.size(),0);
}



// void send_string_wrap(int client_fd,string msg){
//   string resp_bulk=token_to_resp_bulk(msg);
//   send(client_fd,resp_bulk.c_str(),resp_bulk.size(),0);
// }
void handleResponse(int client_fd){
  char buffer[1024];
  while(true){
    int bytes_read=recv(client_fd,buffer,sizeof(buffer)-1,0);
    if(bytes_read<=0){
      cout<<"failed to read payload."<<endl;
      close(client_fd);
      return;
    }
    string request(buffer);
    vector<string>parsed_request;
    size_t start = 0;
    size_t end = request.find("\r\n");

    while (end != std::string::npos) {
        if(request[start]!='*' && request[start]!='$')parsed_request.push_back(request.substr(start, end - start));
        start = end + 2;
        end = request.find("\r\n", start);
    }
    string command=parsed_request[0];
    to_lowercase(command);
    string response="";
    if(command=="ping"){
      send_simple_string(client_fd,"PONG");
    }else if(command=="echo"){
        send_bulk_string(client_fd,parsed_request[1]);
    }else if(command=="set"){
      string key=parsed_request[1];
      string value=parsed_request[2];
      int delay_time=-1;
      if(parsed_request.size()>3){
        string optional_arg=parsed_request[3];
        to_lowercase(optional_arg);
        if(optional_arg=="px" || optional_arg=="ex"){
          delay_time=stoi(parsed_request[4]);
          if(optional_arg=="ex")delay_time*=1000;
        }
      }
      set_key_value(key,value,delay_time);
      send_simple_string(client_fd,"OK");
    } else if(command=="get") {
        string key = parsed_request[1];
        bool send_empty = false;
        string value;
        {
            lock_guard<mutex> lock1(kv_mutex);
            lock_guard<mutex> lock2(expiry_map_mutex);
            if(kv.count(key)){
                if(!expiry_map.count(key) || chrono::steady_clock::now() < expiry_map[key])
                    value = kv[key];
                else
                    send_empty = true; // mark to remove later
            } else
                send_empty = true;
        }
        if(send_empty) remove_key(key); // lock again inside remove_key is fine
        if(!value.empty()) send_bulk_string(client_fd, value);
        else send_null_string(client_fd);
    }else if(command=="rpush"){
      string list_key=parsed_request[1];
      send_integer(client_fd,handle_rpush(parsed_request,list_key));
    }
  }
}

int main(int argc, char **argv) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }
  
  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(6379);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 6379\n";
    return 1;
  }
  
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
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
  while(true){
    int client_fd=accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
    std::cout << "Client connected : "<<client_fd<<"\n";
    thread client_thread(handleResponse,client_fd);
    client_thread.detach();
  }  
  close(server_fd);

  return 0;
}
