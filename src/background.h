#pragma once

// std::atomic<bool> bg_running{true};

// // background task queue
// std::queue<std::function<void()>> bg_tasks;
// std::mutex bg_task_mutex;
// std::condition_variable bg_cv;


void start_background_worker();
void enqueue_bg_task(std::function<void()> task);
void stop_background_worker();
void start_expiry_cleaner();
void map_handlers();

