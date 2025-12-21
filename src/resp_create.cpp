#include <string>
#include <deque>
#include <climits>
#include <iostream>
#include "resp_create.h"
#include "state.h"

using namespace std;

string create_simple_string(string msg)
{
  return "+" + msg + "\r\n";
}
string create_bulk_string(string &msg)
{
  return "$" + to_string(msg.size()) + "\r\n" + msg + "\r\n";
}
string create_integer(int msg)
{
  return ":" + to_string(msg) + "\r\n";
}
string create_resp_array(deque<string> &list, int start, int end,bool dontEncode)
{
  if (end > (int)list.size() - 1)
    end = (int)list.size() - 1;
  string resp_array = "*" + to_string(end - start + 1) + "\r\n";
  for (int i = start; i <= end; i++)
  {
    if(!dontEncode)resp_array.append(create_bulk_string(list[i]));
    else resp_array.append(list[i]);

  }
  return resp_array;
}

string create_simple_error(string msg)
{
  return "-ERR "+msg+"\r\n";
}

string create_null_bulk_string(){
  return "$-1\r\n";
}

string create_null_array(){
  return "*-1\r\n";
}
string create_empty_array(){
  return "*0\r\n";
}


