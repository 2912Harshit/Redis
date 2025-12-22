#include "TransactionHandler.h"
#include "state.h"
#include "resp_create.h"


string TransactionHandler::handleIncr(string key){
  bool key_exists=false;
  {
    lock_guard<mutex>lock(kv_mutex);
    if(kv.count(key))key_exists=true;
  }
  if(key_exists){
    if(kv[key][0]>='1' && kv[key][0]<='9'  && to_string(stoi(kv[key])).size()==kv[key].size())kv[key]=to_string(stoi(kv[key])+1);
    else return create_simple_error("value is not an integer or out of range");
  }else{
    kv[key]="1";
  }
  return create_integer(stoi(kv[key]));
}

string TransactionHandler::handleMulti(int client_fd){
    m_transaction[client_fd];
    m_client.insert(client_fd);
    return create_simple_string("OK");
}

bool TransactionHandler::checkClient(int client_fd){
    return m_client.count(client_fd);
}

string TransactionHandler::addRequest(int client_fd,deque<string>&parsed_request){
    m_transaction[client_fd].push(parsed_request);
    cout<<m_transaction[client_fd].size()<<endl;
    return create_simple_string("QUEUED");
}

string TransactionHandler::handleExec(int client_fd){
    if(!checkClient(client_fd))return create_simple_error("EXEC without MULTI");
    else{
      
      string resp="";
      int n=m_transaction[client_fd].size();
      
      if(n==0){
        resp=create_empty_array();
      }else{
        cout<<"here : "<<n<<endl;
        resp="*"+to_string(n)+"\r\n";
        cout<<"resp : "<<resp<<endl;
        queue<deque<string> >&queued_requests=m_transaction[client_fd];
        while(!queued_requests.empty()){
          deque<string>&parsed_request=queued_requests.front();
          string command=parsed_request[0];
          resp.append(commandMap[command](client_fd,parsed_request));
          queued_requests.pop();
        }
      }
      m_transaction.erase(client_fd);
      m_client.erase(client_fd);
      return resp;
    }
}



