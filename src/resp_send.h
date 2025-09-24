#pragma once

#include <string>
#include <deque>
#include <climits>

void send_simple_string(int client_fd, std::string msg);
void send_bulk_string(int client_fd, std::string msg);
void send_integer(int client_fd, int msg);
void send_null_string(int client_fd);
void send_null_array(int client_fd);
void send_array(int client_fd, std::deque<std::string> &list, int start = 0, int end = INT_MAX);


