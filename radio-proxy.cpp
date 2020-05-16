#include <iostream>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>

#define DEFAULT_TIMEOUT 5
#define DEFAULT_META "no"

void checkHostName (std::string host) {
  //check if proper value
}

void checkResource (std::string resource) {
  //check if proper value
}

void checkPort (std::string port) {
  //check if proper value
}

void checkMeta (std::string meta) {
  //check if proper value
}

void checkTimeout (std::string timeout) {
  //check if proper value
}

void input(int argc, char **argv, std::string &host, std::string &resource, int &port, std::string &meta, int &timeout) {
  int opt;
  bool host_inp = false, resource_inp = false, port_inp = false;
  while ((opt = getopt(argc, argv, "h:r:p:m:t:")) != EOF) {
    switch(opt) {
      case 'h' :
        checkHostName(optarg);
        host = optarg;
        host_inp = true;
        std::cout << "host = " << host << "\n";
        break;
      case 'r' :
        checkResource(optarg);
        resource = optarg;
        resource_inp = true;
        std::cout << "resource = " << resource << "\n";
        break;
      case 'p' :
        checkPort(optarg);
        port_inp = true;
        port = atoi(optarg);
        std::cout << "port = " << port << "\n";
        break;
      case 'm' :
        checkMeta(optarg);
        meta = optarg;
        std::cout << "meta = " << meta << "\n";
        break;
      case 't' :
        checkTimeout(optarg);
        timeout = atoi(optarg);
        std::cout << "timeout = " << timeout << "\n";
        break;
      case '?' :
        //uzupelnic usage
        fprintf(stderr, "usage ...");
      default:
        std::cout << "\n";
        //wrong parameter
        abort();
    }
  }

  if (!host_inp || !resource_inp || !port_inp) {
    //te parametry są obowiązkowe
    abort();
  }
}

int main(int argc, char** argv) {
  int port = -1, timeout = DEFAULT_TIMEOUT;
  std::string host = "", resource = "", meta = DEFAULT_META;

  input(argc, argv, host, resource, port, meta, timeout);

  return 0;
}
