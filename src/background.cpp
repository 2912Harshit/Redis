#include <thread>
#include <mutex>
#include <chrono>
#include "background.h"
#include "state.h"
#include "handlers.h"
#include "stdc++.h"

using namespace std;

std::atomic<bool> bg_running{true};

// background task queue
std::queue<std::function<void()>> bg_tasks;
std::mutex bg_task_mutex;
std::condition_variable bg_cv;

void start_background_worker() {
    std::thread([] {
        while (bg_running.load()) {

            // ===== 1️⃣ Execute queued background jobs =====
            {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(bg_task_mutex);
                    bg_cv.wait_for(lock, std::chrono::milliseconds(100), [] {
                        return !bg_tasks.empty() || !bg_running.load();
                    });

                    if (!bg_running.load()) break;

                    if (!bg_tasks.empty()) {
                        task = std::move(bg_tasks.front());
                        bg_tasks.pop();
                    }
                }

                if (task) {
                    task();   // async publish / socket write / etc.
                }
            }

            // ===== 2️⃣ Expiry cleanup (periodic) =====
            {
                
                lock_guard<mutex>lock1(kv_mutex);
                lock_guard<mutex>lock2(expiry_map_mutex);
                auto now = std::chrono::steady_clock::now();
                for (auto it = expiry_map.begin(); it != expiry_map.end();) {
                    if (now >= it->second) {
                        kv.erase(it->first);
                        it = expiry_map.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
    }).detach();
}

void enqueue_bg_task(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(bg_task_mutex);
        bg_tasks.push(std::move(task));
    }
    bg_cv.notify_one();
}

void stop_background_worker() {
    bg_running.store(false);
    bg_cv.notify_all();
}


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
  pubSubCommandMap["subscribe"]=&handle_subscribe;
  pubSubCommandMap["unsubscribe"]=&handle_unsubscribe;
  pubSubCommandMap["publish"]=&handle_publish;
  // pubSubCommandMap["ping"]=&handle_subscriber_ping;

}




