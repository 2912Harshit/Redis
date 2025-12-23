#include "stdc++.h"

using namespace std;

class Channel{
    string name;
    unordered_set<int>subscribers;
    mutex channel_mutex;
    public:
        Channel(string name){
            this->name=name;
        }
        bool subscribe(int client_fd,int sub_count);
        bool unsubscribe(int client_fd,int sub_count);
        void publish(int client_fd,string& msg);
        int getSize();
};

class PubSubHandler{
    unordered_map<string,unique_ptr<Channel>> channels;
    unordered_map<int,int>clients_sub_count;
    mutex pubsubhandler_mutex;
    public:
        void subscribe(int client_fd,string& ch_name);
        void unsubscribe(int client_fd,string& ch_name);
        void publish(int client_fd,string& ch_name,string& msg); 
        bool checkClient(int client_fd);
        void ping(int client_fd);
};