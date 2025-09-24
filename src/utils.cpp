#include <algorithm>
#include <cctype>
#include "utils.h"

void to_lowercase(std::string &str)
{
  std::transform(str.begin(), str.end(), str.begin(),
            [](unsigned char c)
            { return std::tolower(c); });
}


