#include "TransactionHandler.h"
#include "state.h"


void TransactionHandler::handleIncr(int client_fd,string key){
  bool key_exists=false;
  {
    lock_guard<mutex>lock(kv_mutex);
    if(kv.count(key))key_exists=true;
  }
  if(key_exists){
    if(kv[key][0]>='1' && kv[key][0]<='9'  && to_string(stoi(kv[key])).size()==kv[key].size())kv[key]=to_string(stoi(kv[key])+1);
    else send_simple_error(client_fd,"value is not an integer or out of range");
  }else{
    kv[key]="1";
  }
  send_integer(client_fd,stoi(kv[key]));
}

void TransactionHandler::handleMulti(int client_fd){
    m_transaction[client_fd];
    m_client.insert(client_fd);
    send_simple_string(client_fd,"OK");
}

bool TransactionHandler::checkClient(int client_fd){
    return m_client.count(client_fd);
}

void TransactionHandler::addRequest(int client_fd,deque<string>&parsed_request){
    m_transaction[client_fd].push(parsed_request);
    send_simple_string(client_fd,"QUEUED");
}

void TransactionHandler::handleExec(int client_fd){
    if(!checkClient(client_fd))send_simple_error(client_fd,"EXEC without MULTI");
}



