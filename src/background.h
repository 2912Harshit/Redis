#pragma once

#include <functional>

// Starts the single background worker thread
void start_background_worker();

// Enqueue a background task (non-blocking)
void enqueue_bg_task(std::function<void()> task);

// Gracefully stop the background worker
void stop_background_worker();

// Optional: register handlers / init hooks
void map_handlers();
