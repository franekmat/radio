// author: Mateusz Frankowski, 385442
#include "data.h"
#include "connection.h"
#include "telnetmenu.h"
#include "err.h"

// RadiosDeque type: deque of pairs (a, (b, c)),
// where a - radio-proxy's address, b - time in microseconds of the last sent
// KEEPALIVE to this radio-proxy, c - time in microsceonds of the last received
// audio or meta from that radio-proxy
typedef std::deque <std::pair<struct sockaddr_in, std::pair<unsigned long long, unsigned long long> > > RadiosDeque;

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
        host = optarg;
        host_inp = true;
        break;
      case 'P' :
        port_udp_inp = true;
        port_udp = getValueFromString(optarg, "port");
        break;
      case 'p' :
        port_tcp_inp = true;
        port_tcp = getValueFromString(optarg, "port");
        break;
      case 'T' :
        timeout = getValueFromString(optarg, "timeout");
        if (timeout <= 0) {
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

// send DISCOVER message to radio-proxy programs via UDP
void searchProxy(int &sock_udp, struct sockaddr_in &address) {
  std::string mess = getUdpHeader("DISCOVER", 0);
  ssize_t len = mess.size();
  socklen_t rcva_len = (socklen_t) sizeof(address);
  ssize_t snd_len = sendto(sock_udp, mess.data(), mess.size(), 0, (struct sockaddr *) &address, rcva_len);
  if (snd_len != (ssize_t) len) {
    error("partial / failed write");
  }
}

// send KEEPALIVE message to radio-proxy programs and save time when that happens
void sendKeepAlive(int &sock_udp, struct sockaddr_in &my_address) {
  std::string nth = getUdpHeader("KEEPALIVE", 0);
  ssize_t len = nth.size();

  socklen_t rcva_len = (socklen_t) sizeof(my_address);
  ssize_t snd_len = sendto(sock_udp, nth.data(), nth.size(), 0, (struct sockaddr *) &my_address, rcva_len);
  if (snd_len != (ssize_t) len) {
    error("partial / failed write");
  }
}

// checking if 2 given radios are the same
bool compareRadios(struct sockaddr_in &radio1, struct sockaddr_in &radio2) {
  return (radio1.sin_addr.s_addr == radio2.sin_addr.s_addr &&
      radio1.sin_port == radio2.sin_port);
}

// receive response from the radio-proxy: AUDIO, METADATA or IAM
void receiveStream(int &sock_udp, TelnetMenu *&menu, int timeout, int &radio_pos, RadiosDeque &radios) {
  struct sockaddr_in radio_address;
  ssize_t rcv_len;
  socklen_t rcva_len = (socklen_t) sizeof(radio_address);
  std::string buffer;

  buffer.resize(BUFFER_SIZE + HEADER_SIZE);
  rcv_len = recvfrom(sock_udp, &buffer[0], buffer.size(), 0, (struct sockaddr *) &radio_address, &rcva_len);
  if (rcv_len < 0) {
    if (errno != EAGAIN) {
      error("read");
    }
    else if (radio_pos != -1 && gettimelocal() - radios[radio_pos - 1].second.second >= (unsigned long long)timeout * 1000000) {
      menu->deleteRadio(radio_pos);
      radios.erase(radios.begin() + radio_pos - 1);
      radio_pos = -1;
    }
    return;
  }
  buffer.resize(rcv_len);

  // check whether message is valid
  if (!checkReceivedMessage(buffer, rcv_len)) {
    return receiveStream(sock_udp, menu, timeout, radio_pos, radios);
  }

  std::string type = getType(bytesToInt(buffer[0], buffer[1]));
  buffer.erase(0, HEADER_SIZE);

  if (radio_pos != -1 && type.compare("AUDIO") == 0 && compareRadios(radio_address, radios[radio_pos - 1].first)) {
    std::cout << buffer;
    std::cout.flush();
    radios[radio_pos - 1].second.second = gettimelocal();
  }
  else if (radio_pos != -1 && type == "METADATA" && compareRadios(radio_address, radios[radio_pos - 1].first)) {
    radios[radio_pos - 1].second.second = gettimelocal();
    menu->changeMeta(buffer);
  }
  else if (type == "IAM") {
    radios.push_back(std::make_pair(radio_address, std::make_pair(gettimelocal(), gettimelocal())));
    menu->addRadio(buffer);
  }
}

// function that detects actions on the telnet menu, receive stream from
// the radio-proxy and send them KEEPALIVE messages
int runClient (int &sock_udp, struct sockaddr_in &my_address, struct sockaddr_in &broadcast_address, TelnetMenu *&menu, int timeout, int &radio_pos, RadiosDeque &radios) {
  int action = menu->runTelnet(1);
  // action = 1 means that someone selected searching proxy in menu
  if (action == 1) {
    searchProxy(sock_udp, broadcast_address);
    receiveStream(sock_udp, menu, timeout, radio_pos, radios);
  }
  // action = 2 means that someone selected radio station to play in menu
  else if (action == 2) {
    radio_pos = menu->getCurrPos();
    menu->setPlayingPos(radio_pos);
    my_address = radios[radio_pos - 1].first;
    radios[radio_pos - 1].second.second = gettimelocal();
    sendKeepAlive(sock_udp, my_address);
  }
  // action = -1 means that someone closed connection with telnet menu
  // without pressing "Koniec" button
  // action = -2 means that someone pressed "Koniec" button
  else if (action == -1 || action == -2) {
    return action;
  }
  if (radio_pos != -1 && gettimelocal() - radios[radio_pos - 1].second.first >= KEEPALIVE_TIMEOUT_MICROS) {
    sendKeepAlive(sock_udp, my_address);
    radios[radio_pos - 1].second.first = gettimelocal();
  }
  receiveStream(sock_udp, menu, timeout, radio_pos, radios);

  return 0;
}

int main(int argc, char** argv) {
  std::ios_base::sync_with_stdio(false);
  int port_udp = -1, port_tcp = -1, timeout = DEFAULT_TIMEOUT_S, sock, sock_telnet;
  std::string host = "";
  struct sockaddr_in my_address;
  RadiosDeque radios;

  parseInput(argc, argv, host, port_udp, port_tcp, timeout);

  setUdpClientConnection(sock, host, port_udp, my_address);

  struct sockaddr_in broadcast_address = my_address;

  TelnetMenu *menu = new TelnetMenu(port_tcp);
  menu->newTelnetConnection(sock_telnet);
  menu->addClient(sock_telnet);
  menu->setupTelnet();

  int radio_pos = -1, ret = 0;
  // ret = -2 means that someone pressed "Koniec" in the telnet menu
  while (ret != -2) {
    ret = runClient(sock, my_address, broadcast_address, menu, timeout, radio_pos, radios);
    // ret = -1 means that someone closed connection with telnet menu
    // without pressing "Koniec", so they are allowed to open menu again
    if (ret == -1) {
      menu->addClient(sock_telnet);
      menu->setupTelnet();
    }
  }

  if (close(sock) < 0) {
    error("Closing socket");
  }
  delete menu;

  return 0;
}
