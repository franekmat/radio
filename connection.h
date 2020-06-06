#ifndef CONNECTION_H
#define CONNECTION_H

#include "data.h"
#include "err.h"

// set tcp connection, which will be used for the communication with the radio server
void setTcpClientConnection(int &sock, std::string &host, int &port) {
  struct addrinfo addr_hints, *addr_result = NULL;
  memset(&addr_hints, 0, sizeof(struct addrinfo));
  addr_hints.ai_family = AF_INET;
  addr_hints.ai_socktype = SOCK_STREAM;
  addr_hints.ai_protocol = IPPROTO_TCP;

  int err = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &addr_hints, &addr_result);
  if (err == EAI_SYSTEM) {
    error("getaddrinfo: " + std::string(gai_strerror(err)));
  }
  else if (err != 0) {
    error("getaddrinfo: " + std::string(gai_strerror(err)));
  }

  sock = socket(addr_result->ai_family, addr_result->ai_socktype, addr_result->ai_protocol);
  if (sock < 0) {
    error("socket");
  }

  if (connect(sock, addr_result->ai_addr, addr_result->ai_addrlen) < 0) {
    error("connect");
  }

  freeaddrinfo(addr_result);
}

// set udp connection, which will be used for the communication with clients
// binding value is false only if we would like to send messages from radio-proxy
// from different ports
void setUdpServerConnection(int &sock, int &port, bool binding) {
  struct sockaddr_in server_address;
  sock = socket(AF_INET, SOCK_DGRAM, 0); // creating IPv4 UDP socket
	if (sock < 0) {
    error ("socket");
  }

  int optval = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (const void *)&optval , sizeof(int));

  if(fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK) < 0) {
    error("fcntl");
  }

  server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
  server_address.sin_port = htons(port);

	if (binding && bind(sock, (struct sockaddr *) &server_address, (socklen_t) sizeof(server_address)) < 0) {
    error("bind");
  }
}

// set UDP connection, which will be used to communicate with radio-proxy programs
void setUdpClientConnection(int &sock, std::string host, int &port, struct sockaddr_in &my_address) {
  struct addrinfo addr_hints;
  struct addrinfo *addr_result;

  // 'converting' host/port in string to struct addrinfo
  (void) memset(&addr_hints, 0, sizeof(struct addrinfo));
  addr_hints.ai_family = AF_INET;
  addr_hints.ai_socktype = SOCK_DGRAM;
  addr_hints.ai_protocol = IPPROTO_UDP;
  addr_hints.ai_flags = 0;
  addr_hints.ai_addrlen = 0;
  addr_hints.ai_addr = NULL;
  addr_hints.ai_canonname = NULL;
  addr_hints.ai_next = NULL;

  if (getaddrinfo(host.data(), NULL, &addr_hints, &addr_result) != 0) {
    error("getaddrinfo");
  }

  my_address.sin_family = AF_INET;
  my_address.sin_addr.s_addr = ((struct sockaddr_in*) (addr_result->ai_addr))->sin_addr.s_addr;
  my_address.sin_port = htons(port);

  freeaddrinfo(addr_result);

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    error("socket");
  }

  // activate broadcast option
  int optval = 1;
  setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const void *)&optval , sizeof(int));

  if(fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK) < 0) {
    error("fcntl");
  }
}

#endif // CONNECTION_H
