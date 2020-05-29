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
#include <vector>
#include <algorithm>
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
  my_address.sin_addr.s_addr =
      ((struct sockaddr_in*) (addr_result->ai_addr))->sin_addr.s_addr; // address IP
  my_address.sin_port = htons((uint16_t) port); // port from the command line

  freeaddrinfo(addr_result);

  sock = socket(PF_INET, SOCK_DGRAM, 0);
  if (sock < 0)
    error("socket");
}

std::string getUdpHeader (std::string type, int length) {
  std::string res = "";
  int n;
  if (type == "DISCOVER") {
    n = 1;
  }
  else if (type == "IAM") {
    n = 2;
  }
  else if (type == "KEEPALIVE") {
    n = 3;
  }
  else if (type == "AUDIO") {
    n = 4;
  }
  else if (type == "METADATA") {
    n = 6;
  }
  res += (char)((n >> 8) & 0xFF);
  res += (char)(n & 0xFF);
  res += (char)((length >> 8) & 0xFF);
  res += (char)(length & 0xFF);

  return res;
}

int bytesToInt (char b1, char b2) {
  int res = 0;
  res <<= 8;
  res |= b1;
  res <<= 8;
  res |= b2;
  return res;
}

std::string getType (int n) {
  if (n == 1) {
    return "DISCOVER";
  }
  else if (n == 2) {
    return "IAM";
  }
  else if (n == 3) {
    return "KEEPALIVE";
  }
  else if (n == 4) {
    return "AUDIO";
  }
  else {
    return "METADATA";
  }
}

void receiveUdpData (int &sock, struct sockaddr_in &my_address) {
  while(1) { //dodać poruszanie strzalkami?
    char buffer[BUFFER_SIZE];
    socklen_t snda_len, rcva_len;
  	ssize_t len, snd_len, rcv_len;
    struct sockaddr_in srvr_address;

    std::string nth = getUdpHeader("KEEPALIVE", 0);
    len = nth.size();


    // std::cout << "wysylam " << nth << "\n";

    rcva_len = (socklen_t) sizeof(my_address);
    snd_len = sendto(sock, nth.data(), nth.size(), 0,
        (struct sockaddr *) &my_address, rcva_len);
    if (snd_len != (ssize_t) len) {
      error("partial / failed write");
    }
    // std::cout << "wyslalem\n";

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


    if (rcv_len < 4) {
      error("wrong size of rcv_len");
    }

    std::string type = getType(bytesToInt(tmp[0], tmp[1]));
    int leng = bytesToInt(tmp[2], tmp[3]);
    tmp.erase(0, 4);

    // std::cout << type << "\n";

    if (type == "AUDIO") {
      // std::cout << "no i co?\n";
      // std::cout << tmp.size() << "\n";
      std::cout << tmp;
    }
    else if (type == "METADATA") {
      std::cerr << tmp;
    }
  }
}

int newTelnetConnection(int &sock_telnet, int &port_tcp) {
  struct sockaddr_in server, their_addr;
  struct sigaction action;
  sigset_t block_mask;

  sock_telnet = socket(PF_INET, SOCK_STREAM, 0);
  if (sock_telnet == -1) {
    error("Opening stream socket");
  }

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port = htons(port_tcp);
  if (bind(sock_telnet, (struct sockaddr*)&server, (socklen_t)sizeof(server)) == -1) {
    error("Binding stream socket");
  }

  if (listen(sock_telnet, 5) == -1) {
    error("Starting to listen");
  }

  int sin_size = sizeof( struct sockaddr_in);
  int msgsock = accept(sock_telnet, (struct sockaddr*) &their_addr, (socklen_t *) &sin_size);
  if (msgsock == -1) {
    error("accept");
  }

  return msgsock;
}

void writeTelnet(int &msgsock, std::string s) {
  if (write(msgsock, s.data(), s.size()) == -1) {
    error("send");
  }
}

void setupTelnetMenu (std::vector<std::string> &telnet_menu) {
  telnet_menu.clear();
  telnet_menu.push_back("Szukaj pośrednika");
  telnet_menu.push_back("Koniec");
}

void writeTelnetMenu(int &msgsock, std::vector<std::string> &telnet_menu, int curr_pos) {
  std::string clear = "\033[1J\033[H";
  std::string pointer = " *";
  std::string new_line = "\r\n";

  writeTelnet(msgsock, clear);

  for (int i = 0; i < telnet_menu.size(); i++) {
    if (i != curr_pos) {
      writeTelnet(msgsock, telnet_menu[i] + new_line);
    }
    else {
      writeTelnet(msgsock, telnet_menu[i] + pointer + new_line);
    }
  }
}

void setupTelnet(int &msgsock, std::vector<std::string> &telnet_menu) {
  std::string setup = "\377\375\042\377\373\001";
  writeTelnet(msgsock, setup);
  setupTelnetMenu(telnet_menu);
  writeTelnetMenu(msgsock, telnet_menu, 0);
}

void searchProxy (int &sock_udp, struct sockaddr_in &my_address, int &msgsock, std::vector<std::string> &telnet_menu) {
  char buffer[BUFFER_SIZE];
  socklen_t snda_len, rcva_len;
	ssize_t len, snd_len, rcv_len;
  struct sockaddr_in srvr_address;

  std::string mess = getUdpHeader("DISCOVER", 0);
  len = mess.size();

  rcva_len = (socklen_t) sizeof(my_address);
  snd_len = sendto(sock_udp, mess.c_str(), mess.size(), 0, (struct sockaddr *) &my_address, rcva_len);
  if (snd_len != (ssize_t) len) {
    error("partial / failed write");
  }

  (void) memset(buffer, 0, sizeof(buffer));
  len = (size_t) sizeof(buffer) - 1;
  rcva_len = (socklen_t) sizeof(srvr_address);

  std::string tmp;
  tmp.resize(BUFFER_SIZE);
  struct pollfd fds[1] = {{sock_udp, 0 | POLLIN}};

  rcv_len = 1;
  while (rcv_len > 0) {
    if (poll(fds, 1, 500) == 0) {
      break;
    }
    rcv_len = recvfrom(sock_udp, &tmp[0], len, 0, (struct sockaddr *) &srvr_address, &rcva_len);
    if (rcv_len < 0) {
      error("read");
    }
    tmp.resize(rcv_len);
    // std::cout << "rozmiar = " << rcv_len << "\n";
    if (rcv_len < 4) {
      error("wrong size of rcv_len");
    }
    int type = bytesToInt(tmp[0], tmp[1]);
    int leng = bytesToInt(tmp[2], tmp[3]);
    tmp.erase(0, 4);


    //lepiej to dać po pętli, a tutaj wrzucać do jakiegoś wektora żeby potem
    //raz zaktualizować menu
    if (getType(type) == "IAM") {
      telnet_menu.pop_back();
      telnet_menu.push_back(tmp);
      telnet_menu.push_back("Koniec");
      writeTelnetMenu(msgsock, telnet_menu, 0);
    }
  }
}

bool arrowUp (char *buf, int len) {
  if (len == 3 && buf[0] == 27 && buf[1] == 91 && buf[2] == 65) {
    return true;
  }
  return false;
}

bool arrowDown (char *buf, int len) {
  if (len == 3 && buf[0] == 27 && buf[1] == 91 && buf[2] == 66) {
    return true;
  }
  return false;
}

bool enter (char *buf, int len) {
  if (len == 2 && buf[0] == 13 && buf[1] == 0) {
    return true;
  }
  return false;
}

void runTelnet(int &msgsock, std::vector<std::string> &telnet_menu, int &sock_udp, struct sockaddr_in &my_address) {
  int curr_pos = 0;
  while(1) {
    int buf_size = 16;
    char buf[buf_size];
    int len = recv(msgsock, buf, buf_size, 0);

    if (arrowUp(buf, len)) {
      curr_pos = std::max(0, curr_pos - 1);
      writeTelnetMenu(msgsock, telnet_menu, curr_pos);
    }
    else if (arrowDown(buf, len)) {
      curr_pos = std::min(curr_pos + 1, (int)telnet_menu.size() - 1);
      writeTelnetMenu(msgsock, telnet_menu, curr_pos);
    }
    else if (enter(buf, len)) {
      if (curr_pos == 0) {
        // std::cout << "start searching proxy\n";
        searchProxy(sock_udp, my_address, msgsock, telnet_menu);
      }
      else if (curr_pos == (int) telnet_menu.size() - 1) {
        if (close(msgsock) < 0) {
          error("Closing socket");
        }
      }
      else {
        receiveUdpData (sock_udp, my_address);
      }
    }
  }
}

int main(int argc, char** argv) {
  int port_udp = -1, port_tcp = -1, timeout = DEFAULT_TIMEOUT, sock, sock_telnet;
  std::string host = "";
  struct addrinfo addr_hints, *addr_result = NULL;
  struct sockaddr_in my_address;
  std::vector <std::string> telnet_menu;

  parseInput(argc, argv, host, port_udp, port_tcp, timeout);

  setUdpConnection(sock, host, port_udp, 0, my_address);

  // receiveUdpData (sock, my_address);
  int msgsock = newTelnetConnection(sock_telnet, port_tcp);
  setupTelnet(msgsock, telnet_menu);
  runTelnet(msgsock, telnet_menu, sock, my_address);

  return 0;
}
