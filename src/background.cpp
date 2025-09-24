#include <thread>
#include <mutex>
#include <chrono>
#include "background.h"
#include "state.h"

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


