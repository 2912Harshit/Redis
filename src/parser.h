#pragma once

#include <string>
#include <vector>
#include <deque>

std::deque<std::string> parse_redis_command(const std::string &buffer);


