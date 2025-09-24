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
using namespace std;

unordered_map<string,string>kv;
mutex kv_mutex;

void set_key_value(string key,string value){
    lock_guard<mutex>lock(kv_mutex);
    kv[key]=value;
}

void remove_key(string key){
    lock_guard<mutex>lock(kv_mutex);
    kv.erase(key);
}

void set_timeout(function<void(string)>remove_key,int delayMs,string key){
  thread([remove_key,delayMs,key](){
    this_thread::sleep_for(chrono::milliseconds(delayMs));
    remove_key(key);
  }).detach();
}

string token_to_resp_bulk(string token){
  string res="";
  if(token.empty()){
    res= "$-1\r\n";
  }else if(token=="OK" || token=="PONG"){
    res="+"+token+"\r\n";
  }
  else res= "$"+to_string(token.size())+"\r\n"+token+"\r\n";
  return res;
}


void send_string_wrap(int client_fd,string msg){
  string resp_bulk=token_to_resp_bulk(msg);
  send(client_fd,resp_bulk.c_str(),resp_bulk.size(),0);
}
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
        parsed_request.push_back(request.substr(start, end - start));
        start = end + 2;
        end = request.find("\r\n", start);
    }
    // parsed_request.push_back(request.substr(start));
    string command=parsed_request[2];
    string response="";
    transform(command.begin(),command.end(),command.begin(),
                  [](unsigned char c){return tolower(c);});
    if(command=="ping"){
      send_string_wrap(client_fd,"PONG");
    }else if(command=="echo"){
        send_string_wrap(client_fd,parsed_request[4]);
    }else if(command=="set"){
      string key=parsed_request[4];
      string value=parsed_request[6];
      set_key_value(key,value);
      if(parsed_request.size()>7){
        for(string str:parsed_request)cout<<str<<" ";
      }
      send_string_wrap(client_fd,"OK");
    }else if(command=="get"){
      string key=parsed_request[4];
      if(kv.count(key))send_string_wrap(client_fd,kv[key]);
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

  while(true){
    int client_fd=accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
    std::cout << "Client connected : "<<client_fd<<"\n";
    thread client_thread(handleResponse,client_fd);
    client_thread.detach();
  }  
  close(server_fd);

  return 0;
}
