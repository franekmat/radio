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
#include "err.h"

#define DEFAULT_TIMEOUT 5
#define DEFAULT_META "no"
#define BUFFER_SIZE 10005

void error(std::string err_msg)
{
  const char *err_message = err_msg.c_str();
  perror(err_message);
  exit(1);
}

void checkHostName (std::string host) {
  //check if proper value
}

void checkResource (std::string resource) {
  //check if proper value
}

void checkMulti (std::string multi) {
  //check if proper value
}

bool containsOnlyDigits (std::string s) {
  /* I want to ignore leading and trailing spaces */
  int l = 0, p = s.size() - 1;
  while (l < s.size() && s[l] == ' ') {
    l++;
  }
  while (p >= 0 && s[p] == ' ') {
    p--;
  }
  /* now check if number contains something other than digit characters */
  for (int i = l; i <= p; i++) {
    if (s[i] < '0' || s[i] > '9') {
      return false;
    }
  }
  return true;
}

int getValueFromString (std::string value, std::string whatsthat) {
  if (!containsOnlyDigits(value)) {
    std::string err_msg = "Invalid " + whatsthat + " number";
    error(err_msg);
  }
  int ret_val;
  try {
    ret_val = std::stoi(value);
  }
  catch (std::invalid_argument const &e) {
		error("Bad input: std::invalid_argument thrown");
	}
	catch (std::out_of_range const &e) {
		error("Integer overflow: std::out_of_range thrown");
	}
  return ret_val;
}

void checkPort (std::string port) {
  if (0) {
    error("Invalid port number");
  }
}

void checkMeta (std::string meta) {
  if (meta != "yes" && meta != "no") {
    error ("Wrong format of meta argument");
  }
}

void checkTimeout (std::string timeout) {
  if (0) {
    error("Invalid timeout number");
  }
}

void printUsageError(std::string name) {
  std::string err_msg = "Usage: " + name + " -h host -r resource -p port -m yes|no -t timeout -P port -B multi -T timeout";
  error(err_msg);
}

void parseInput(int argc, char **argv, std::string &host, std::string &resource,
  int &port, std::string &meta, int &timeout, int &port_client, int &timeout_clients, std::string &multi) {
  int opt;
  bool host_inp = false, resource_inp = false, port_inp = false;
  bool port_client_inp = false;

  if (argc < 3) {
    printUsageError(argv[0]);
  }

  while ((opt = getopt(argc, argv, "h:r:p:m:t:P:B:T:")) != EOF) {
    switch(opt) {
      case 'h' :
        checkHostName(optarg);
        host = optarg;
        host_inp = true;
        break;
      case 'r' :
        checkResource(optarg);
        resource = optarg;
        resource_inp = true;
        break;
      case 'p' :
        checkPort(optarg);
        port_inp = true;
        port = getValueFromString(optarg, "port");
        break;
      case 'm' :
        checkMeta(optarg);
        meta = optarg;
        break;
      case 't' :
        checkTimeout(optarg);
        timeout = getValueFromString(optarg, "timeout");
        if (timeout <= 0) {
          error("Timeout value shall be bigger than 0");
        }
        break;
      case 'P' :
        checkPort(optarg);
        port_client_inp = true;
        port_client = getValueFromString(optarg, "port");
        break;
      case 'B' :
        checkMulti(optarg);
        multi = optarg;
        break;
      case 'T' :
        checkTimeout(optarg); //not sure if rules are the same as in -t case
        timeout_clients = getValueFromString(optarg, "timeout");
        if (timeout_clients <= 0) { //tu też?
          error("Timeout value shall be bigger than 0");
        }
        break;
      case '?' :
        printUsageError(argv[0]);
      default:
        printUsageError(argv[0]);
    }
  }

  if (!host_inp || !resource_inp || !port_inp) { //dodać port_client_inp
    printUsageError(argv[0]);
  }
}

std::string setRequest (std::string host, std::string resource, std::string meta) {
  std::string message =
                   "GET " + resource + " HTTP/1.0\r\n" +
                   "Host: " + host + "\r\n" +
                   "User-Agent: Orban/Coding Technologies AAC/aacPlus Plugin 1.0 (os=Windows NT 5.1 Service Pack 2)\r\n" +
                   "Accept: */*\r\n";
  if (meta == "yes") {
    message += "Icy-MetaData: 1\r\n";
  }
  message += "Connection: close\r\n\r\n";
  return message;
}

void setTcpConnection(int &sock, std::string &host, int &port) {
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

void setUdpConnection(int &sock, int &port, int timeout) {
  struct sockaddr_in server_address;
	struct sockaddr_in client_address;

  sock = socket(AF_INET, SOCK_DGRAM, 0); // creating IPv4 UDP socket
	if (sock < 0) {
    error ("socket");
  }

  server_address.sin_family = AF_INET; // IPv4
	server_address.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
	server_address.sin_port = htons(port); // default port for receiving is PORT_NUM

  // bind the socket to a concrete address
	if (bind(sock, (struct sockaddr *) &server_address, (socklen_t) sizeof(server_address)) < 0) {
    error("bind");
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
  return (header.find("icy-metaint:") != std::string::npos);
}

/* get icy-metaint value from header */
int getMetaInt (std::string header) {
  std::size_t found = header.find("icy-metaint:");
  header.erase(0, found + strlen("icy-metaint:"));
  std::string value = header.substr(0, header.find("\r\n"));
  return getValueFromString(value, "metaint");
}

bool containsEndOfHeader(std::string &buffer) {
  return (buffer.find("\r\n\r\n") != std::string::npos);
}

std::string getStatus (std::string &header) {
  std::size_t found = header.find("\r\n");
  return header.substr(0, found);
}

bool okStatus (std::string &status) {
  return (status == "HTTP/1.0 200 OK" || status == "HTTP/1.1 200 OK" || status == "ICY 200 OK");
}

/* cut header from buffer and return header */
std::string getHeader(std::string &buffer) {
  std::size_t found = buffer.find("\r\n\r\n");
  std::string header = buffer.substr(0, found);
  buffer.erase(0, found + strlen("\r\n\r\n"));
  return header;
}

/* read and return rcv_len */
ssize_t readTCP(int &sock, std::string &tmp, int timeout) {
  ssize_t rcv_len;
  struct pollfd fds[1] = {{sock, 0 | POLLIN}};

  tmp.resize(BUFFER_SIZE);
  if (poll(fds, 1, timeout * 1000) == 0) {
    error("Read timeout");
  }
  rcv_len = read(sock, &tmp[0], BUFFER_SIZE - 1);
  if (rcv_len < 0) {
    error("read");
  }
  tmp.resize(rcv_len);
  return rcv_len;
}

/* receive header and return response status */
std::string handleHeader(int &sock, std::string &buffer, int timeout) {
  std::string tmp = "";
  ssize_t rcv_len = 1;

  while (rcv_len > 0) {
    rcv_len = readTCP(sock, tmp, timeout);
    buffer += tmp;
    if (containsEndOfHeader(buffer)) {
      break;
    }
  }

  return getHeader(buffer);
}

void readDataWithoutMeta(int &sock, std::string &buffer, int timeout) {
  std::cout << buffer;
  ssize_t rcv_len = 1;

  while (rcv_len > 0) {
    rcv_len = readTCP(sock, buffer, timeout);
    std::cout << buffer;
  }
}


int getMetaSize(std::string &buffer) {
  if (buffer.empty()) {
    error("Cannot get size of meta from empty buffer");
  }
  return (int)buffer[0] * 16;
}

/* I know that buffer is not empty */
void readMeta (int &sock, std::string &buffer, int timeout) {
  int size = getMetaSize(buffer);
  buffer.erase(0, 1);

  if (size < 0) {
    error("wrong size of meta data");
  }
  else if (size == 0) {
    return;
  }

  if (buffer.size() < size) {
    std::string tmp = "";
    ssize_t rcv_len = 1;

    while (rcv_len > 0) {
      rcv_len = readTCP(sock, tmp, timeout);
      buffer += tmp;
      if (buffer.size() >= size) {
        break;
      }
    }
  }

  // std::cerr << "usuwam " << size << "\n";
  std::cerr << buffer.substr(0, size) << "\n";
  buffer.erase(0, size);
}

void readDataWithMeta(int &sock, std::string &buffer, int size, int timeout) {
  int counter = size;
  std::string tmp = "";
  ssize_t rcv_len = 1;

  while (rcv_len > 0 || !buffer.empty()) {
    // i do not have yet buffer filled with data
    if (buffer.size() < counter || counter == 0) {
      rcv_len = readTCP(sock, tmp, timeout);
      buffer += tmp;
    }

    if (buffer.size() <= counter) {
      counter -= buffer.size();
      // std::cerr << buffer.size() << "<\n";
      std::cout << buffer;
      buffer = "";
    }
    else {
      // std::cerr << counter << "\n";
      std::cout << buffer.substr(0, counter);
      buffer.erase(0, counter);
      readMeta(sock, buffer, timeout);
      counter = size;
    }
  }
}

void handleResponse(int &sock, std::string &meta, int timeout) {
  std::string buffer = "";
  std::string header = handleHeader(sock, buffer, timeout);

  std::string status = getStatus(header);
  if (!okStatus(status)) {
    error("Bad status");
  }

  int metaIntVal = -1;
  if (containsMeta(header)) {
    metaIntVal = getMetaInt(header);
  }

  // std::cerr << "metaint = " << metaIntVal << "\n";

  if (metaIntVal == -1) {
    readDataWithoutMeta(sock, buffer, timeout);
  }
  else {
    if (meta != "yes") {
      error("We did not ask server for meta data");
    }
    readDataWithMeta(sock, buffer, metaIntVal, timeout);
  }
}

int main(int argc, char** argv) {
  /* for A part of the task */
  int port = -1, timeout = DEFAULT_TIMEOUT, sock;
  std::string host = "", resource = "", meta = DEFAULT_META;
  /* for B part of the task */
  int port_client = -1, timeout_clients = DEFAULT_TIMEOUT, sock_udp;
  std::string multi = "";

  parseInput(argc, argv, host, resource, port, meta, timeout, port_client, timeout_clients, multi);
  setTcpConnection(sock, host, port);
  setUdpConnection(sock_udp, port_client, timeout_clients);

  std::string message = setRequest(host, resource, meta);
  sendRequest(sock, message);
  handleResponse(sock, meta, timeout);

  (void) close(sock);

  return 0;
}
