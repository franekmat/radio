#ifndef ERR_H
#define ERR_H

#include <cstdio>
#include <cstdlib>

void error(std::string err_msg)
{
  const char *err_message = err_msg.c_str();
  perror(err_message);
  exit(1);
}


#endif //ERR_H
