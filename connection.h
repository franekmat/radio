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
void setUdpServerConnection(int &sock, int &port) {
  struct sockaddr_in server_address;

  sock = socket(AF_INET, SOCK_DGRAM, 0); // creating IPv4 UDP socket
	if (sock < 0) {
    error ("socket");
  }

  /* setsockopt: Handy debugging trick that lets
   * us rerun the server immediately after we kill it;
   * otherwise we have to wait about 20 secs.
   * Eliminates "ERROR on binding: Address already in use" error.
   */
  int optval = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));


  // bzero((char *) &server_address, sizeof(server_address)); // ????
  server_address.sin_family = AF_INET; // IPv4
	server_address.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
	server_address.sin_port = htons(port); // default port for receiving is PORT_NUM

  // bind the socket to a concrete address
	if (bind(sock, (struct sockaddr *) &server_address, (socklen_t) sizeof(server_address)) < 0) {
    error("bind");
  }
}

// // set multicast connection, which will be used for the communication with clients
// void setMulticastConnection(int &sock, std::string &host, int &port) {
//   char *multicast_dotted_address;
//   in_port_t local_port;
//   int sock;
//   struct sockaddr_in local_address;
//   struct ip_mreq ip_mreq;
//
//   sock = socket(AF_INET, SOCK_DGRAM, 0);
//   if (sock < 0) {
//     syserr("socket");
//   }
//
//   ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
//   if (inet_aton(multicast_dotted_address, &ip_mreq.imr_multiaddr) == 0) {
//     error("ERROR: inet_aton - invalid multicast address\n");
//   }
//   if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&ip_mreq, sizeof ip_mreq) < 0) {
//     error("setsockopt");
//   }
//
//   local_address.sin_family = AF_INET;
//   local_address.sin_addr.s_addr = htonl(INADDR_ANY);
//   local_address.sin_port = htons(port);
//   if (bind(sock, (struct sockaddr *)&local_address, sizeof local_address) < 0) {
//     syserr("bind");
//   }
// }

// set UDP connection, which will be used to communicate with radio-proxy programs
void setUdpClientConnection(int &sock, std::string host, int &port, struct sockaddr_in &my_address) {
  struct addrinfo addr_hints;
  struct addrinfo *addr_result;

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
  my_address.sin_addr.s_addr = ((struct sockaddr_in*) (addr_result->ai_addr))->sin_addr.s_addr; // address IP
  my_address.sin_port = htons((uint16_t) port); // port from the command line

  freeaddrinfo(addr_result);

  sock = socket(PF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    error("socket");
  }

  // activate broadcast option
  int optval = 1;
  setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const void *)&optval , sizeof(int));
}
