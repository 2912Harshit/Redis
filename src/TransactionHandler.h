#pragma once
#include <string>
#include <mutex>
#include "resp_send.h"
#include "stdc++.h" 



using namespace std;


class TransactionHandler{
    private :
        unordered_map<int,queue<deque<string>>>m_transaction;
        unordered_set<int>m_client;
    public: 

    void handleIncr(int client_fd,string key);
    void handleMulti(int client_fd);
    bool checkClient(int client_fd);
    void addRequest(int client_fd,deque<string>&parsed_request);
    void handleExec(int client_fd);

};
