#include "telnetmenu.h"
#include "data.h"
#include "err.h"

#define DEFAULT_TIMEOUT 5
#define DEFAULT_META "no"
#define BUFFER_SIZE 2048
#define HEADER_SIZE 4

// function, which prints proper usage of the 'name' program
void printUsageError(std::string name) {
  std::string err_msg = "Usage: " + name + " -H host -P port -p port -T timeout";
  error(err_msg);
}

// parsing input arguments, checking whether they are proper
// (for example, port number can not contains other characters than digits),
// and then saving them
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
        port_udp = getValueFromString(optarg, "port");
        break;
      case 'p' :
        checkPort(optarg);
        port_tcp_inp = true;
        port_tcp = getValueFromString(optarg, "port");
        break;
      case 'T' :
        checkTimeout(optarg);
        timeout = getValueFromString(optarg, "timeout");
        if (timeout <= 0) { //tu też?
          error("Timeout value shall be bigger than 0");
        }
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

// set UDP connection, which will be used to communicate with radio-proxy programs
void setUdpConnection(int &sock, std::string host, int &port, int timeout, struct sockaddr_in &my_address) {
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

// send DISCOVER message to radio-proxy programs via UDP
void searchProxy(int &sock_udp, struct sockaddr_in &my_address) {
  std::string mess = getUdpHeader("DISCOVER", 0);
  ssize_t len = mess.size();

  socklen_t rcva_len = (socklen_t) sizeof(my_address);
  ssize_t snd_len = sendto(sock_udp, mess.data(), mess.size(), 0, (struct sockaddr *) &my_address, rcva_len);
  if (snd_len != (ssize_t) len) {
    error("partial / failed write");
  }
}

// send KEEPALIVE message to radio-proxy programs and save time when that happens
void sendKeepAlive(int &sock_udp, struct sockaddr_in &my_address, int &last_time) {
  last_time = gettimelocal();
  std::string nth = getUdpHeader("KEEPALIVE", 0);
  ssize_t len = nth.size();

  socklen_t rcva_len = (socklen_t) sizeof(my_address);
  ssize_t snd_len = sendto(sock_udp, nth.data(), nth.size(), 0, (struct sockaddr *) &my_address, rcva_len);
  if (snd_len != (ssize_t) len) {
    error("partial / failed write");
  }
}

// receive response from the radio-proxy: AUDIO, METADATA or IAM
void receiveStream(int &sock_udp, TelnetMenu *&menu, int timeout, int &radio_pos) {
  struct sockaddr_in srvr_address;
  ssize_t rcv_len;
  socklen_t rcva_len = (socklen_t) sizeof(srvr_address);
  std::string buffer;
  struct pollfd fds[1] = {{sock_udp, 0 | POLLIN}};

  buffer.resize(BUFFER_SIZE + HEADER_SIZE);
  if (poll(fds, 1, timeout * 1000) == 0) {
    menu->deleteRadio(radio_pos);
    radio_pos = -1;
    return;
  }
  rcv_len = recvfrom(sock_udp, &buffer[0], buffer.size(), 0, (struct sockaddr *) &srvr_address, &rcva_len);
  if (rcv_len < 0) {
    error("read");
  }
  buffer.resize(rcv_len);

  // check whether message is valid
  if (!checkReceivedMessage(buffer, rcv_len)) {
    return receiveStream(sock_udp, menu, timeout, radio_pos);
  }

  std::string type = getType(bytesToInt(buffer[0], buffer[1]));
  int leng = bytesToInt(buffer[2], buffer[3]);
  buffer.erase(0, 4);

  if (type == "AUDIO") {
    std::cout << buffer;
  }
  else if (type == "METADATA") {
    menu->changeMeta(buffer);
  }
  else if (type == "IAM") {
    menu->addRadio(buffer);
  }
}

// function that detects actions on the telnet menu, receive stream from
// the radio-proxy and send them KEEPALIVE messages
void runClient (int &sock_udp, struct sockaddr_in &my_address, TelnetMenu *&menu, int timeout, int &last_time, int &radio_pos) {
  int action = menu->runTelnet(10);
  /* akcja równa 1 oznacza, że ktoś wybrał w menu szukanie pośrednika */
  if (action == 1) {
    searchProxy(sock_udp, my_address);
    receiveStream(sock_udp, menu, timeout, radio_pos);
  }
  /* akcja równa 2 oznacza, że ktoś wybrał w menu radio do odtwarzania */
  else if (action == 2) {
    // return runClient (sock_udp, my_address, menu, timeout, menu->getCurrPos());
    radio_pos = menu->getCurrPos();
    menu->setPlayingPos(radio_pos);
  }
  //nic nie odtwarzamy, wiec nikomu nie wysylamy keepalive (czy aby na pewno?)
  if (radio_pos < 0) {
    return;
  }
  //czy takie mierzenie tego czasu jest ok?
  if (gettimelocal() - last_time >= 3500000 || last_time == -1) {
    sendKeepAlive(sock_udp, my_address, last_time);
  }
  receiveStream(sock_udp, menu, timeout, radio_pos);
}

int main(int argc, char** argv) {
  int port_udp = -1, port_tcp = -1, timeout = DEFAULT_TIMEOUT, sock, sock_telnet;
  std::string host = "";
  struct sockaddr_in my_address;

  parseInput(argc, argv, host, port_udp, port_tcp, timeout);

  setUdpConnection(sock, host, port_udp, 0, my_address);

  TelnetMenu *menu = new TelnetMenu(port_tcp);
  menu->newTelnetConnection(sock_telnet);
  menu->addClient(sock_telnet);
  menu->setupTelnet();

  int last_keepalive_time = -1, radio_pos = -1;
  while (1) {
    runClient(sock, my_address, menu, timeout, last_keepalive_time, radio_pos);
  }

  return 0;
}
