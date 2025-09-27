#pragma once

#include <string>
#include <deque>
#include <climits>

std::string create_simple_string(std::string &msg);
std::string create_bulk_string(std::string &msg);
std::string create_integer(int &msg);
std::string create_resp_array(std::deque<std::string> &list, int start = 0, int end = INT_MAX ,bool dontEncode=false);
std::string create_simple_error(std::string msg);


