#include <iostream>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "err.h"

#define DEFAULT_TIMEOUT 5
#define DEFAULT_META "no"
#define BUFFER_SIZE 1000005

void error(const char *err_message)
{
    perror(err_message);
    exit(1);
}

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

void parseInput(int argc, char **argv, std::string &host, std::string &resource, int &port, std::string &meta, int &timeout) {
  int opt;
  bool host_inp = false, resource_inp = false, port_inp = false;

  if (argc < 3) {
    std::string name = argv[0];
    std::string err_msg = "Usage: " + name + " -h host -r resource -p port -m yes|no -t timeout";
    error(err_msg.c_str());
  }

  while ((opt = getopt(argc, argv, "h:r:p:m:t:")) != EOF) {
    switch(opt) {
      case 'h' :
        checkHostName(optarg);
        host = optarg;
        host_inp = true;
        // std::cout << "host = " << host << "\n";
        break;
      case 'r' :
        checkResource(optarg);
        resource = optarg;
        resource_inp = true;
        // std::cout << "resource = " << resource << "\n";
        break;
      case 'p' :
        checkPort(optarg);
        port_inp = true;
        port = atoi(optarg);
        // std::cout << "port = " << port << "\n";
        break;
      case 'm' :
        checkMeta(optarg);
        meta = optarg;
        // std::cout << "meta = " << meta << "\n";
        break;
      case 't' :
        checkTimeout(optarg);
        timeout = atoi(optarg);
        // std::cout << "timeout = " << timeout << "\n";
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

std::string setRequest (std::string host, std::string resource) {
  std::string message =
                   "GET " + resource + " HTTP/1.0\r\n" +
                   "Host: " + host + "\r\n" +
                   "User-Agent: Orban/Coding Technologies AAC/aacPlus Plugin 1.0 (os=Windows NT 5.1 Service Pack 2)\r\n" +
                   "Accept: */*\r\n" +
                   // "Icy-MetaData: 1\r\n" +
                   "Connection: close\r\n" + "\r\n";
  return message;
}

void setConnection(int &sock, std::string &host, int &port, struct addrinfo *addr_hints, struct addrinfo **addr_result) {
  memset(addr_hints, 0, sizeof(struct addrinfo));
  addr_hints->ai_family = AF_INET;
  addr_hints->ai_socktype = SOCK_STREAM;
  addr_hints->ai_protocol = IPPROTO_TCP;

  int err = getaddrinfo(host.c_str(), std::to_string(port).c_str(), addr_hints, addr_result);
  if (err == EAI_SYSTEM) {
    printf("getaddrinfo: %s", gai_strerror(err));
  }
  else if (err != 0) {
    printf("getaddrinfo: %s", gai_strerror(err));
  }

  sock = socket((*addr_result)->ai_family, (*addr_result)->ai_socktype, (*addr_result)->ai_protocol);
  if (sock < 0) {
    printf("socket");
  }
  if (connect(sock, (*addr_result)->ai_addr, (*addr_result)->ai_addrlen) < 0) {
    printf("connect");
  }
}

void sendRequest(int &sock, std::string &message) {
  ssize_t len = message.size();
  if (write(sock, message.c_str(), len) != len) {
    printf("partial / failed write");
  }
}

/* check if received header contains icy-metaint */
bool containsMeta (std::string header) {
  return false;
}

/* get icy-metaint value from header */
int getMetaInt (std::string header) {
  return 1;
}

bool containsEndOfHeader(std::string &buffer) {
  return (buffer.find("\r\n\r\n") != std::string::npos);
}

std::string getStatus (std::string &header) {
  return header;
}

/* cut header from buffer and return header */
std::string getHeader(std::string &buffer) {
  std::size_t found = buffer.find("\r\n\r\n");
  std::string header = buffer.substr(0, found);
  buffer.erase(0, found + strlen("\r\n\r\n"));
  return header;
}

/* receive header and return response status */
std::string handleHeader(int &sock, std::string &buffer) {
  std::string response = "", header;
  ssize_t rcv_len = 1;

  while (rcv_len > 0) {
    rcv_len = read(sock, &buffer[0], BUFFER_SIZE - 1);
    if (rcv_len < 0) {
      std::string err_msg = "read";
      error(err_msg.c_str());
    }
    buffer.resize(rcv_len);
    response += buffer;

    if (containsEndOfHeader(buffer)) {
      header = getHeader(buffer);
      break;
    }
  }

  return getStatus(header);
}

void handleResponse(int &sock) {
  std::string buffer;
  buffer.resize(BUFFER_SIZE);

  std::string status = handleHeader(sock, buffer);

  if (0) {

  }

  std::cout << buffer;

  ssize_t rcv_len = 1;
  while (rcv_len > 0) {
    buffer.resize(BUFFER_SIZE);
    rcv_len = read(sock, &buffer[0], BUFFER_SIZE - 1);
    if (rcv_len < 0) {
      std::string err_msg = "read";
      error(err_msg.c_str());
    }
    buffer.resize(rcv_len);
    std::cout << buffer;
  }
}

int main(int argc, char** argv) {
  int port = -1, timeout = DEFAULT_TIMEOUT, sock;
  std::string host = "", resource = "", meta = DEFAULT_META;
  struct addrinfo addr_hints, *addr_result = NULL;

  parseInput(argc, argv, host, resource, port, meta, timeout);

  setConnection(sock, host, port, &addr_hints, &addr_result);
  freeaddrinfo(addr_result);

  std::string message = setRequest(host, resource);
  sendRequest(sock, message);

  handleResponse(sock);

  return 0;
}
