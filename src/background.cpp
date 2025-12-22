#include <thread>
#include <mutex>
#include <chrono>
#include "background.h"
#include "state.h"
#include "handlers.h"

void start_expiry_cleaner()
{
  std::thread([]()
              {
    while(true){
      {
        std::lock_guard<std::mutex>lock1(kv_mutex);
        std::lock_guard<std::mutex>lock2(expiry_map_mutex);
        auto now=std::chrono::steady_clock::now();
        for(auto it=expiry_map.begin();it!=expiry_map.end();){
          if(now>=it->second){
            kv.erase(it->first);
            it=expiry_map.erase(it);
          }else ++it;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } })
      .detach();
}

void map_handlers(){
  commandMap["ping"]=&handle_ping;
  commandMap["echo"]=&handle_echo;
  commandMap["set"]=&handle_set;
  commandMap["get"]=&handle_get;
  commandMap["rpush"]=&handle_rpush;
  commandMap["lrange"]=&handle_lrange;
  commandMap["lpush"]=&handle_lpush;
  commandMap["llen"]=&handle_llen;
  commandMap["lpop"]=&handle_lpop;
  commandMap["blpop"]=&handle_blpop;
  commandMap["type"]=&handle_type_of;
  commandMap["xadd"]=&handle_xadd;
  commandMap["xrange"]=&handle_xrange;
  commandMap["xread"]=&handle_xread;
  commandMap["incr"]=&handle_incr;
  commandMap["multi"]=&handle_multi;
  commandMap["exec"]=&handle_exec;
  commandMap["discard"]=&handle_discard;
}




