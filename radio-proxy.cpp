#include "data.h"
#include "err.h"

#define DEFAULT_TIMEOUT 5
#define DEFAULT_META "no"
#define BUFFER_SIZE 2048
#define HEADER_SIZE 4

typedef std::deque <std::pair<struct sockaddr_in, unsigned long long> > ClientsDeque;

/*
   Program radio-proxy uruchomiony bez parametru -P ma działać
   wg specyfikacji części A zadania.
   ACTIVATE_CLIENTS = false, gdy nie ma -P
   ACTIVATE_CLIENTS = true, gdy jest -P
*/
bool ACTIVATE_CLIENTS = false;

void printUsageError(std::string name) {
  std::string err_msg = "Usage: " + name + " -h host -r resource -p port -m yes|no -t timeout -P port -B multi -T timeout";
  error(err_msg);
}

void parseInput(int argc, char **argv, std::string &host, std::string &resource,
  int &port, std::string &meta, int &timeout, int &port_clients, int &timeout_clients, std::string &multi) {
  int opt;
  bool host_inp = false, resource_inp = false, port_inp = false;
  bool port_clients_inp = false;

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
        port_clients_inp = true;
        ACTIVATE_CLIENTS = true;
        port_clients = getValueFromString(optarg, "port");
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

  if (!host_inp || !resource_inp || !port_inp) { //dodać port_clients_inp
    printUsageError(argv[0]);
  }
}

bool deleteClient(struct sockaddr_in &client, ClientsDeque &clients) {
  for (int i = 0; i < clients.size(); i++) {
    if (clients[i].first.sin_addr.s_addr == client.sin_addr.s_addr &&
        clients[i].first.sin_port == client.sin_port) {
      clients.erase(clients.begin() + i);
      return true;
    }
  }
  return false;
}

std::string getUdpMessage(std::string type, int length, std::string data) {
  std::string message = getUdpHeader(type, length);

  if (type == "AUDIO" || type == "METADATA") {
    message += data;
  }

  return message;
}

void updateClients(int &sock_udp, ClientsDeque &clients, std::string radio_name) {
  struct sockaddr_in client_address;
  socklen_t rcva_len;
  ssize_t len = 1;
  std::string buffer;
  buffer.resize(BUFFER_SIZE + HEADER_SIZE);
  struct pollfd fds[1] = {{sock_udp, 0 | POLLIN}};


  while (len > 0) {
    rcva_len = (socklen_t) sizeof(client_address);
    if (poll(fds, 1, 100) == 0) {
      break;
    }
    len = recvfrom(sock_udp, &buffer[0], buffer.size(), 0, (struct sockaddr *) &client_address, &rcva_len);
    if (len < 0) {
      error("error on datagram from client socket");
    }
    buffer.resize(len);

    if (!checkReceivedMessage(buffer, len)) {
      continue;
    }

    if (getType(bytesToInt(buffer[0], buffer[1])) == "DISCOVER") {
      std::string message = getUdpHeader("IAM", radio_name.size()) + radio_name;
      ssize_t snd_len = sendto(sock_udp, message.data(), message.size(), 0, (struct sockaddr *) &client_address, rcva_len);
      if (snd_len != message.size()) {
        error("error on sending datagram to client socket");
      }
    }
    // else {
      deleteClient(client_address, clients);
      clients.push_back(std::make_pair(client_address, gettimelocal()));
    // }
  }
}

void sendUdpMessage(int &sock_udp, std::string message, ClientsDeque &clients) {
  unsigned long long current_time = gettimelocal();
  ssize_t snd_len;
  socklen_t snda_len;
  struct sockaddr_in client_address;
  for (auto client : clients) {
    snda_len = (socklen_t) sizeof(client_address);
    if (current_time - client.second < 5000000) {
      std::cout << "sending " << message.size() << " to " << client.first.sin_port << "\n";
      client_address = client.first;
      snd_len = sendto(sock_udp, message.data(), message.size(), 0, (struct sockaddr *) &client_address, snda_len);
      if (snd_len != message.size()) {
        error("error on sending datagram to client socket");
      }
    }
  }
}

void printData (std::string data, int &sock_udp, ClientsDeque &clients, std::string radio_name) {
  if (data.empty()) {
    return;
  }
  if (ACTIVATE_CLIENTS) {
    updateClients(sock_udp, clients, radio_name);
    std::string message = getUdpMessage("AUDIO", (int)data.size(), data);
    sendUdpMessage(sock_udp, message, clients);
  }
  else {
    std::cout << data;
  }
}

void printMeta (std::string meta, int &sock_udp, ClientsDeque &clients, std::string radio_name) {
  if (meta.empty()) {
    return;
  }
  if (ACTIVATE_CLIENTS) {
    updateClients(sock_udp, clients, radio_name);
    std::string message = getUdpMessage("METADATA", (int)meta.size(), meta);
    sendUdpMessage(sock_udp, message, clients);
  }
  else {
    std::cerr << meta;
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

void sendRequest(int &sock, std::string &message) {
  ssize_t len = message.size();
  if (write(sock, message.c_str(), len) != len) {
    printf("partial / failed write");
  }
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

void readDataWithoutMeta(int &sock, int &sock_udp, std::string &buffer, int timeout, ClientsDeque &clients, std::string radio_name) {
  printData(buffer, sock_udp, clients, radio_name);
  ssize_t rcv_len = 1;

  while (rcv_len > 0) {
    rcv_len = readTCP(sock, buffer, timeout);
    printData(buffer, sock_udp, clients, radio_name);
  }
}

void readMeta (int &sock, int &sock_udp, std::string &buffer, int timeout, ClientsDeque &clients, std::string radio_name) {
  if (buffer.empty()) {
    return;
  }

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

  printMeta(buffer.substr(0, size), sock_udp, clients, radio_name);
  buffer.erase(0, size);
}

void readDataWithMeta(int &sock, int &sock_udp, std::string &buffer, int size, int timeout, ClientsDeque &clients, std::string radio_name) {
  int counter = size;
  std::string tmp = "";
  ssize_t rcv_len = 1;

  while (rcv_len > 0 || !buffer.empty()) {
    if (buffer.empty()) {
      rcv_len = readTCP(sock, tmp, timeout);
      buffer += tmp;
    }

    if (buffer.size() <= counter) {
      counter -= buffer.size();
      printData(buffer, sock_udp, clients, radio_name);
      buffer.clear();
    }
    else {
      printData(buffer.substr(0, counter), sock_udp, clients, radio_name);
      buffer.erase(0, counter);
      readMeta(sock, sock_udp, buffer, timeout, clients, radio_name);
      counter = size;
    }
  }
}

void handleResponse(int &sock, int &sock_udp, std::string &meta, int timeout, ClientsDeque &clients) {
  std::string buffer = "";
  std::string header = handleHeader(sock, buffer, timeout);

  std::string status = getStatus(header);
  if (!okStatus(status)) {
    error("Bad status");
  }

  std::string radio_name = getMetaName(header);

  int metaIntVal = -1;
  if (containsMeta(header)) {
    metaIntVal = getMetaInt(header);
  }

  if (metaIntVal == -1) {
    readDataWithoutMeta(sock, sock_udp, buffer, timeout, clients, radio_name);
  }
  else {
    if (meta != "yes") {
      error("We did not ask server for meta data");
    }
    readDataWithMeta(sock, sock_udp, buffer, metaIntVal, timeout, clients, radio_name);
  }
}

int main(int argc, char** argv) {
  /* for A part of the task */
  int port = -1, timeout = DEFAULT_TIMEOUT, sock;
  std::string host = "", resource = "", meta = DEFAULT_META;
  /* for B part of the task */
  int port_clients = -1, timeout_clients = DEFAULT_TIMEOUT, sock_udp;
  std::string multi = "";
  ClientsDeque clients;

  parseInput(argc, argv, host, resource, port, meta, timeout, port_clients, timeout_clients, multi);

  setTcpConnection(sock, host, port);
  setUdpConnection(sock_udp, port_clients, timeout_clients);

  std::string message = setRequest(host, resource, meta);
  sendRequest(sock, message);
  handleResponse(sock, sock_udp, meta, timeout, clients);

  (void) close(sock);

  return 0;
}
