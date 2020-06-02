#ifndef ERR_H
#define ERR_H

#include <iostream>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <poll.h>
#include <stdexcept>
#include <sys/time.h>

void error(std::string err_msg)
{
  const char *err_message = err_msg.c_str();
  perror(err_message);
  exit(1);
}


#endif //ERR_H
