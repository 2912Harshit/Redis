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

    string handleIncr(string key);
    string handleMulti(int client_fd);
    bool checkClient(int client_fd);
    string addRequest(int client_fd,deque<string>&parsed_request);
    string handleExec(int client_fd);
    string handleDiscard(int client_fd);

};
