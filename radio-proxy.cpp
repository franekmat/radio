#include "data.h"
#include "connection.h"
#include "err.h"

typedef std::deque <std::pair<struct sockaddr_in, unsigned long long> > ClientsDeque;


// radio-proxy started without paramater -P shall work as stated in
// the A part of the task
// ACTIVATE_CLIENTS = false, if there is no -P
// ACTIVATE_CLIENTS = true, if there is -P
bool ACTIVATE_CLIENTS = false;

// function, which prints proper usage of the 'name' program
void printUsageError(std::string name) {
  std::string err_msg = "Usage: " + name + " -h host -r resource -p port -m yes|no -t timeout -P port -B multi -T timeout";
  error(err_msg);
}

// parsing input arguments, checking whether they are proper
// (for example, port number can not contains other characters than digits),
// and then saving them
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
        host = optarg;
        host_inp = true;
        break;
      case 'r' :
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
        multi = optarg;
        break;
      case 'T' :
        checkTimeout(optarg);
        timeout_clients = getValueFromString(optarg, "timeout");
        if (timeout_clients <= 0) {
          error("Timeout value shall be bigger than 0");
        }
        break;
      case '?' :
        printUsageError(argv[0]);
      default:
        printUsageError(argv[0]);
    }
  }

  if (!host_inp || !resource_inp || !port_inp) {
    printUsageError(argv[0]);
  }
}

// delete - given as the function argument - client, if it occurs in clients deque
// return true, if it happened, otherwise false
// the function is used to erase client, which sent KEEPALIVE info to the
// radio-proxy from the deque, before we will add them with updated contact time
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

// checking if - given as the function argument - client, occurs in clients deque
bool findClient(struct sockaddr_in &client, ClientsDeque &clients) {
  for (int i = 0; i < clients.size(); i++) {
    if (clients[i].first.sin_addr.s_addr == client.sin_addr.s_addr &&
        clients[i].first.sin_port == client.sin_port) {
      return true;
    }
  }
  return false;
}

// given type of message, length of data and data string,
// return message ready to be send to clients (4 byte header + data)
std::string getUdpMessage(std::string type, int length, std::string data) {
  std::string message = getUdpHeader(type, length);
  if (type == "AUDIO" || type == "METADATA") {
    message += data;
  }
  return message;
}

// check if some new client send us DISCOVER message and update clients deque
// send them back IAM message with radio information
void findNewClients(int &sock_disc, int &sock_udp, ClientsDeque &clients, std::string radio_name) {
  struct sockaddr_in client_address;
  socklen_t rcva_len;
  ssize_t len = 1;
  std::string buffer;

  while (len > 0) {
    rcva_len = (socklen_t) sizeof(client_address);
    buffer.resize(BUFFER_SIZE + HEADER_SIZE);
    len = recvfrom(sock_disc, &buffer[0], buffer.size(), 0, (struct sockaddr *) &client_address, &rcva_len);
    if (len < 0) {
      if (errno != EAGAIN) {
        error("error on datagram from client socket");
      }
      break;
    }
    buffer.resize(len);

    if (!checkReceivedMessage(buffer, len)) {
      continue;
    }

    if (getType(bytesToInt(buffer[0], buffer[1])) == "DISCOVER" && !findClient(client_address, clients)) {
      std::string message = getUdpHeader("IAM", radio_name.size()) + radio_name;
      ssize_t snd_len = sendto(sock_udp, message.data(), message.size(), 0, (struct sockaddr *) &client_address, rcva_len);
      if (snd_len != message.size()) {
        error("error on sending datagram to client socket");
      }
      std::cerr << "wyslalem DISCOVER do kogos.............\n";
    }
    deleteClient(client_address, clients);
    clients.push_back(std::make_pair(client_address, gettimelocal()));
  }
}

// check if someone send us DISCOVER or KEEPALIVE message and update clients deque
// if that was DISCOVER message send them back IAM message with radio information
void updateClients(int &sock_udp, ClientsDeque &clients, std::string radio_name) {
  struct sockaddr_in client_address;
  socklen_t rcva_len;
  ssize_t len = 1;
  std::string buffer;

  while (len > 0) {
    rcva_len = (socklen_t) sizeof(client_address);
    buffer.resize(BUFFER_SIZE + HEADER_SIZE);
    len = recvfrom(sock_udp, &buffer[0], buffer.size(), 0, (struct sockaddr *) &client_address, &rcva_len);
    if (len < 0) {
      if (errno != EAGAIN) {
        error("error on datagram from client socket");
      }
      break;
    }
    buffer.resize(len);

    if (!checkReceivedMessage(buffer, len)) {
      continue;
    }

    if (getType(bytesToInt(buffer[0], buffer[1])) == "DISCOVER" && !findClient(client_address, clients)) {
      std::string message = getUdpHeader("IAM", radio_name.size()) + radio_name;
      ssize_t snd_len = sendto(sock_udp, message.data(), message.size(), 0, (struct sockaddr *) &client_address, rcva_len);
      if (snd_len != message.size()) {
        error("error on sending datagram to client socket");
      }
    }

    deleteClient(client_address, clients);
    clients.push_back(std::make_pair(client_address, gettimelocal()));
  }
}

// send prepared proper message via UDP to all active clients
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

// print data stream - by sending it to all active clients (after updating clients deque)
// or by printing it to the standard output (depends on program argument -P as stated at the beginning)
void printData (std::string data, int &sock_disc, int &sock_udp, ClientsDeque &clients, std::string radio_name) {
  if (data.empty()) {
    return;
  }
  if (ACTIVATE_CLIENTS) {
    findNewClients(sock_disc, sock_udp, clients, radio_name);
    updateClients(sock_udp, clients, radio_name);
    std::string message = getUdpMessage("AUDIO", (int)data.size(), data);
    sendUdpMessage(sock_udp, message, clients);
  }
  else {
    std::cout << data;
  }
}

// print meta data - by sending it to all active clients (after updating clients deque)
// or by printing it to the standard error output (depends on program argument -P as stated at the beginning)
void printMeta (std::string meta, int &sock_disc, int &sock_udp, ClientsDeque &clients, std::string radio_name) {
  if (meta.empty()) {
    return;
  }
  if (ACTIVATE_CLIENTS) {
    findNewClients(sock_disc, sock_udp, clients, radio_name);
    updateClients(sock_udp, clients, radio_name);
    std::string message = getUdpMessage("METADATA", (int)meta.size(), meta);
    sendUdpMessage(sock_udp, message, clients);
  }
  else {
    std::cerr << meta;
  }
}

// set request that will be send to the radio server in order to receive
// response with the stream
std::string setRequest (std::string host, std::string resource, std::string meta) {
  std::string message = "GET " + resource + " HTTP/1.1\r\n" + "Host: " + host + "\r\n";
  if (meta == "yes") {
    message += "Icy-MetaData: 1\r\n";
  }
  message += "Connection: close\r\n\r\n";
  return message;
}

// send TCP request to the radio server
void sendRequest(int &sock, std::string &message) {
  ssize_t len = message.size();
  if (write(sock, message.c_str(), len) != len) {
    printf("partial / failed write");
  }
}

// read data received from the radio server, returns length of it
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

// read data received from the radio server, as it is used at the first
// responses from the server, it should contains header of the message
// after receiving whole header, the function returns it
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

// read data received from the radio server in the case that it won't contains
// any meta data
void readDataWithoutMeta(int &sock, int &sock_disc, int &sock_udp, std::string &buffer, int timeout, ClientsDeque &clients, std::string radio_name) {
  printData(buffer, sock_disc, sock_udp, clients, radio_name);
  ssize_t rcv_len = 1;

  while (rcv_len > 0) {
    rcv_len = readTCP(sock, buffer, timeout);
    printData(buffer, sock_disc, sock_udp, clients, radio_name);
  }
}

// read meta data received from the radio server
// given as argument buffer contains a prefix of meta data or a whole meta data
void readMeta (int &sock, int &sock_disc, int &sock_udp, std::string &buffer, int timeout, ClientsDeque &clients, std::string radio_name) {
  if (buffer.empty()) {
    return;
  }

  // first byte of meta data is represents integer equals to the 1/16 of meta data
  int size = getMetaSize(buffer);
  buffer.erase(0, 1);

  if (size < 0) {
    error("wrong size of meta data");
  }
  else if (size == 0) {
    return;
  }

  // if given buffer does not contains whole meta data, we need to read the rest of it
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

  // print meta data and remove it from the buffer
  printMeta(buffer.substr(0, size), sock_disc, sock_udp, clients, radio_name);
  buffer.erase(0, size);
}

// read data received from the radio server in the case that it will contains meta data
void readDataWithMeta(int &sock, int &sock_disc, int &sock_udp, std::string &buffer, int size, int timeout, ClientsDeque &clients, std::string radio_name) {
  int counter = size;
  std::string tmp = "";
  ssize_t rcv_len = 1;

  // loop works while we are still receiving something from the server
  // or while the buffer is not empty, the second case is the scenario, when we
  // already don't receive anything, but we still have some data in the buffer to print
  while (rcv_len > 0 || !buffer.empty()) {
    if (buffer.empty()) {
      rcv_len = readTCP(sock, tmp, timeout);
      buffer += tmp;
    }

    // data size is equal or less than remaining data stream size
    // (which is equal to the metaint), so we know that it doesn't contain
    // any meta data and we can print it
    if (buffer.size() <= counter) {
      counter -= buffer.size();
      printData(buffer, sock_disc, sock_udp, clients, radio_name);
      buffer.clear();
    }
    // otherwise, we know that buffer contains some part, or the whole meta data
    else {
      printData(buffer.substr(0, counter), sock_disc, sock_udp, clients, radio_name);
      buffer.erase(0, counter);
      readMeta(sock, sock_disc, sock_udp, buffer, timeout, clients, radio_name);
      counter = size;
    }
  }
}

// function that handles responses from the radio server
// it gets header of it, check status, get the name of the radio and then
// start to receive data stream
void handleResponse(int &sock, int &sock_disc, int &sock_udp, std::string &meta, int timeout, ClientsDeque &clients) {
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
    readDataWithoutMeta(sock, sock_disc, sock_udp, buffer, timeout, clients, radio_name);
  }
  else {
    if (meta != "yes") {
      error("We did not ask server for meta data");
    }
    readDataWithMeta(sock, sock_disc, sock_udp, buffer, metaIntVal, timeout, clients, radio_name);
  }
}

int main(int argc, char** argv) {
  // for A part of the task
  int port = -1, timeout = DEFAULT_TIMEOUT, sock;
  std::string host = "", resource = "", meta = DEFAULT_META;
  // for B part of the task
  int port_clients = -1, timeout_clients = DEFAULT_TIMEOUT, sock_disc, sock_udp;
  std::string multi = "";
  ClientsDeque clients;

  parseInput(argc, argv, host, resource, port, meta, timeout, port_clients, timeout_clients, multi);

  setTcpClientConnection(sock, host, port);
  // socket for receiving discover messages from unknown yet clients
  // (there could be one socket instead of two (sock_disc, sock_udp) if the third
  // argument in both setUdpServerConnection() functions would be true.
  // I've made 2 sockets to make it easier in case of testing multiple radio-proxy
  // on 1 machine with the same address to distinguish such radio stations)
  setUdpServerConnection(sock_disc, port_clients, true);
  // socket for communicating with known clients
  // if you would like to run multiple radio-proxy on 1 machine with the same
  // address, change last argument to 'false' to not make binding
  setUdpServerConnection(sock_udp, port_clients, false);

  std::string message = setRequest(host, resource, meta);
  sendRequest(sock, message);
  handleResponse(sock, sock_disc, sock_udp, meta, timeout, clients);

  (void) close(sock);

  return 0;
}
