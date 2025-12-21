#pragma once

#include <string>
#include <vector>
#include <deque>
#include "StreamHandler.h"
using namespace std;

string handle_rpush(int client_fd,std::deque<std::string> &parsed_request);
string handle_lpush(int client_fd,std::deque<std::string> &parsed_request);
std::string handle_lpop(int client_fd,std::deque<std::string> &parsed_request);
void handle_multiple_lpop(int client_fd, std::string &key, int no_of_removals);
string handle_blpop(int client_fd,std::deque<std::string> &parsed_request);

void set_key_value(std::string key, std::string value, int delay_time);
void remove_key(std::string key);
std::string handle_type_of(int client_fd,deque<string>&parsed_request);
pair<deque<string>,deque<string>>get_stream_keys_ids(deque<string>&parsed_request);
string get_stream_name(deque<string>&parsed_request);

string handle_ping(int client_fd,deque<string>&parsed_request);
string handle_echo(int client_fd,deque<string>&parsed_request);
string handle_set(int client_fd,deque<string>&parsed_request);
string handle_get(int client_fd,deque<string>&parsed_request);
string handle_lrange(int client_fd,deque<string>&parsed_request);
string handle_llen(int client_fd,deque<string>&parsed_request);
string handle_xadd(int client_fd,deque<string>&parsed_request);
string handle_xrange(int client_fd,deque<string>&parsed_request);
string handle_xread(int client_fd,deque<string>&parsed_request);
string handle_incr(int client_fd,deque<string>&parsed_request);
string handle_multi(int client_fd,deque<string>&parsed_request);



