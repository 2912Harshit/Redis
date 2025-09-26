#include <string>
#include <deque>
#include <climits>
#include <iostream>
#include "resp_create.h"
#include "state.h"

using namespace std;

string create_simple_string(string &msg)
{
  return "+" + msg + "\r\n";
}
string create_bulk_string(string &msg)
{
  return "$" + to_string(msg.size()) + "\r\n" + msg + "\r\n";
}
string create_integer(int &msg)
{
  return ":" + to_string(msg) + "\r\n";
}
string create_resp_array(int client_fd, deque<string> &list, int start, int end)
{
  if (end > (int)list.size() - 1)
    end = (int)list.size() - 1;
  string resp_array = "*" + to_string(end - start + 1) + "\r\n";
  for (int i = start; i <= end; i++)
  {
    resp_array.append(create_bulk_string(list[i]));
  }
  return resp_array;
}

string create_simple_error(string msg)
{
  return "-ERR "+msg;
}


