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
#include <poll.h>
#include <stdexcept>
#include "err.h"

#define DEFAULT_TIMEOUT 5
#define DEFAULT_META "no"
#define BUFFER_SIZE 5005

void error(std::string err_msg)
{
  const char *err_message = err_msg.c_str();
  perror(err_message);
  exit(1);
}

void checkHostName (std::string host) {
  //check if proper value
}

bool containsOnlyDigits (std::string s) {
  for (int i = 0; i < s.size(); i++) {
    if (s[i] < '0' || s[i] > '9') {
      return false;
    }
  }
  return true;
}

void checkPort (std::string port) {
  if (!containsOnlyDigits(port)) {
    error("Invalid port number");
  }
  try {
    int port_value = std::stoi(port);
  }
  catch (std::invalid_argument const &e) {
		error("Bad input: std::invalid_argument thrown");
	}
	catch (std::out_of_range const &e) {
		error("Integer overflow: std::out_of_range thrown");
	}
}

void checkTimeout (std::string timeout) {
  if (!containsOnlyDigits(timeout)) {
    error("Invalid timeout number");
  }
  try {
    int timeout_value = std::stoi(timeout);
    if (timeout_value <= 0) {
      error("Timeout value shall be bigger than 0");
    }
  }
  catch (std::invalid_argument const &e) {
		error("Bad input: std::invalid_argument thrown");
	}
	catch (std::out_of_range const &e) {
		error("Integer overflow: std::out_of_range thrown");
	}
}

void printUsageError(std::string name) {
  std::string err_msg = "Usage: " + name + " -H host -P port -p port -T timeout";
  error(err_msg);
}

void parseInput(int argc, char **argv, std::string &host, int &port_udp, int &port_tcp, int &timeout) {
  int opt;
  bool host_inp = false, port_udp_inp = false, port_tcp_inp = false;

  if (argc < 3) {
    printUsageError(argv[0]);
  }

  while ((opt = getopt(argc, argv, "H:P:p:T:")) != EOF) {
    switch(opt) {
      case 'H' :
        checkHostName(optarg);
        host = optarg;
        host_inp = true;
        break;
      case 'P' :
        checkPort(optarg);
        port_udp_inp = true;
        /*  I checked if port is a proper integer so no need to check it again */
        port_udp = std::stoi(optarg);
        break;
      case 'p' :
        checkPort(optarg);
        port_tcp_inp = true;
        /*  I checked if port is a proper integer so no need to check it again */
        port_tcp = std::stoi(optarg);
        break;
      case 'T' :
        checkTimeout(optarg);
        /*  I checked if tiemout is a proper integer so no need to check it again */
        timeout = std::stoi(optarg);
        break;
      case '?' :
        printUsageError(argv[0]);
      default:
        printUsageError(argv[0]);
    }
  }

  if (!host_inp || !port_udp_inp || !port_tcp_inp) {
    printUsageError(argv[0]);
  }
}

void setUdpConnection(int &sock, std::string host, int &port, int timeout) {
  struct addrinfo addr_hints;
  struct addrinfo *addr_result;

  struct sockaddr_in my_address;
  struct sockaddr_in srvr_address;

  // 'converting' host/port in string to struct addrinfo
  (void) memset(&addr_hints, 0, sizeof(struct addrinfo));
  addr_hints.ai_family = AF_INET; // IPv4
  addr_hints.ai_socktype = SOCK_DGRAM;
  addr_hints.ai_protocol = IPPROTO_UDP;
  addr_hints.ai_flags = 0;
  addr_hints.ai_addrlen = 0;
  addr_hints.ai_addr = NULL;
  addr_hints.ai_canonname = NULL;
  addr_hints.ai_next = NULL;
  if (getaddrinfo(host.c_str(), NULL, &addr_hints, &addr_result) != 0) {
    error("getaddrinfo");
  }

  my_address.sin_family = AF_INET; // IPv4
  my_address.sin_addr.s_addr =
      ((struct sockaddr_in*) (addr_result->ai_addr))->sin_addr.s_addr; // address IP
  my_address.sin_port = htons((uint16_t) port); // port from the command line

  freeaddrinfo(addr_result);

  sock = socket(PF_INET, SOCK_DGRAM, 0);
  if (sock < 0)
    error("socket");


  while(1) {
  char buffer[BUFFER_SIZE];
  socklen_t snda_len, rcva_len;
	ssize_t len, snd_len, rcv_len;

  std::string nth = "sdfdf";
  len = nth.size();

  rcva_len = (socklen_t) sizeof(my_address);
  snd_len = sendto(sock, nth.c_str(), nth.size(), 0,
      (struct sockaddr *) &my_address, rcva_len);
  if (snd_len != (ssize_t) len) {
    error("partial / failed write");
  }

  (void) memset(buffer, 0, sizeof(buffer));
  len = (size_t) sizeof(buffer) - 1;
  rcva_len = (socklen_t) sizeof(srvr_address);

  std::string tmp;
  tmp.resize(BUFFER_SIZE);
  rcv_len = recvfrom(sock, &tmp[0], len, 0,
      (struct sockaddr *) &srvr_address, &rcva_len);
  if (rcv_len < 0) {
    error("read");
  }
  tmp.resize(rcv_len);
  std::cout << tmp;
}
}

int main(int argc, char** argv) {
  int port_udp = -1, port_tcp = -1, timeout = DEFAULT_TIMEOUT, sock;
  std::string host = "";
  struct addrinfo addr_hints, *addr_result = NULL;

  parseInput(argc, argv, host, port_udp, port_tcp, timeout);

  setUdpConnection(sock, host, port_udp, 0);

  return 0;
}
