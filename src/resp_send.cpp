#include <string>
#include <deque>
#include <sys/socket.h>
#include <climits>
#include "resp_send.h"
#include "resp_create.h"
#include "state.h"

using namespace std;

void send_simple_string(int client_fd, string msg)
{
  string resp_simple;
  resp_simple = create_simple_string(msg);
  send(client_fd, resp_simple.c_str(), resp_simple.size(), 0);
}
void send_bulk_string(int client_fd, string msg)
{
  string resp_simple;
  resp_simple = create_bulk_string(msg);
  send(client_fd, resp_simple.c_str(), resp_simple.size(), 0);
}
void send_integer(int client_fd, int msg)
{
  string resp_simple;
  resp_simple = create_integer(msg);
  send(client_fd, resp_simple.c_str(), resp_simple.size(), 0);
}

void send_null_string(int client_fd)
{
  string resp_simple = "$-1\r\n";
  send(client_fd, resp_simple.c_str(), resp_simple.size(), 0);
}
void send_null_array(int client_fd)
{
  string resp_null_array = "*0\r\n";
  send(client_fd, resp_null_array.c_str(), resp_null_array.size(), 0);
}
void send_array(int client_fd, deque<string> &list, int start, int end)
{
  lock_guard<mutex> lock1(lists_mutex);
  string resp_array = create_resp_array(client_fd, list, start, end);
  send(client_fd, resp_array.c_str(), resp_array.size(), 0);
}


