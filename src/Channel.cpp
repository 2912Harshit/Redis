#include "Channel.h"
#include "resp_send.h"
#include "resp_create.h"
#include "background.h"

bool Channel::subscribe(int client_fd,int sub_count){
    deque<string>response={create_bulk_string("subscribe"),create_bulk_string(name)};
    bool ret=false;
    {
        lock_guard<mutex>lock(channel_mutex);
        if(subscribers.insert(client_fd).second){
            sub_count++;
            ret=true;
        }
    }
    response.push_back(create_integer(sub_count));
    send_array(client_fd,response,0,INT_MAX,true);
    return ret;
}

bool Channel::unsubscribe(int client_fd,int sub_count){
    deque<string>response={create_bulk_string("unsubscribe"),create_bulk_string(name)};
    bool ret=false;
    {
        lock_guard<mutex>lock(channel_mutex);
        if(subscribers.erase(client_fd)){
            sub_count--;
            ret=true;
        }
    }
    response.push_back(create_integer(sub_count));
    send_array(client_fd,response,0,INT_MAX,true);
    return ret;
}

void Channel::publish(int client_fd,string& msg) {
    std::vector<int> targets;
    deque<string>list={"message",name,msg};
    string resp=create_resp_array(list);
    {
        std::lock_guard<std::mutex> lock(channel_mutex);
        targets.assign(subscribers.begin(), subscribers.end());
    }

    // enqueue async sends
    for (int client : targets) {
        enqueue_bg_task([client,resp] {
            send(client,resp.c_str(),resp.size(),0);  // may block
        });
    }
    send_integer(client_fd,getSize());
}


// void Channel::publish(string& msg){
//     {
//         lock_guard<mutex>lock(channel_mutex);
//         for(int client_fd : subscribers){
//         }
//     }
// }

int Channel::getSize(){
    return subscribers.size();
}

void PubSubHandler::subscribe(int client_fd,string& ch_name){
    Channel *ch;
    bool insert=false;
    {
        lock_guard<mutex>lock(pubsubhandler_mutex);
        if(!channels[ch_name]){
            channels[ch_name]=make_unique<Channel>(ch_name);
        }

    }
    ch=channels[ch_name].get();
    insert=ch->subscribe(client_fd,clients_sub_count[client_fd]);
    if(insert){
        lock_guard<mutex>lock(pubsubhandler_mutex);
        clients_sub_count[client_fd]++;
    }
}
void PubSubHandler::unsubscribe(int client_fd,string& ch_name){
    Channel *ch;
    bool erase=true;
    {
        lock_guard<mutex>lock(pubsubhandler_mutex);
        if(!channels[ch_name]){
            channels[ch_name]=make_unique<Channel>(ch_name);
        }
    }
    ch=channels[ch_name].get();
    erase=ch->unsubscribe(client_fd,clients_sub_count[client_fd]);
    if(erase){
        lock_guard<mutex>lock(pubsubhandler_mutex);
        clients_sub_count[client_fd]--;
        if(clients_sub_count[client_fd]==0)clients_sub_count.erase(client_fd);
    }
}

void PubSubHandler::publish(int client_fd,string& ch_name,string& msg){
    Channel *ch;
    {
        lock_guard<mutex>lock(pubsubhandler_mutex);
        if(!channels[ch_name]){
            channels[ch_name]=make_unique<Channel>(ch_name);
        }
    }
    ch=channels[ch_name].get();
    ch->publish(client_fd,msg);
}

bool PubSubHandler::checkClient(int client_fd){
    return clients_sub_count[client_fd]>0;
}

void PubSubHandler::ping(int client_fd){
    deque<string>resp_arr={"pong",""};
    send_array(client_fd,resp_arr);
}

