#include "parser.h"

std::vector<std::string> parse_redis_command(const std::string &buffer)
{
  std::vector<std::string> tokens;
  size_t pos = 0;

  if (buffer.empty() || buffer[pos] != '*')
    return tokens;

  size_t end_line = buffer.find("\r\n", pos);
  int num_args = stoi(buffer.substr(pos + 1, end_line - pos - 1));
  pos = end_line + 2;

  for (int i = 0; i < num_args; ++i)
  {
    if (buffer[pos] != '$')
      break;
    end_line = buffer.find("\r\n", pos);
    int len = stoi(buffer.substr(pos + 1, end_line - pos - 1));
    pos = end_line + 2;

    tokens.push_back(buffer.substr(pos, len));
    pos += len + 2;
  }

  return tokens;
}


