#pragma once

#include <string>
#include <vector>
using namespace std;

int handle_rpush(std::vector<std::string> &parsed_request, std::string &key);
int handle_lpush(std::vector<std::string> &parsed_request, std::string &key);
std::string handle_lpop(std::string &key);
void handle_multiple_lpop(int client_fd, std::string &key, int no_of_removals);
void handle_blpop(int client_fd, std::string &key, float time);

void set_key_value(std::string key, std::string value, int delay_time);
void remove_key(std::string key);
std::string handle_type_of(std::string key);


