#include "data.h"
#include "connection.h"
#include "telnetmenu.h"
#include "err.h"

typedef std::deque <std::pair<struct sockaddr_in, unsigned long long> > RadiosDeque;

std::string bufer = "";

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
  struct pollfd fds[1] = {{sock_udp, 0 | POLLIN}};

  buffer.resize(BUFFER_SIZE + HEADER_SIZE);
  if (radio_pos != -1 && poll(fds, 1, timeout * 1000) == 0) {
    if (radio_pos != -1) {
      menu->deleteRadio(radio_pos);
      radios.erase(radios.begin() + radio_pos - 1);
      radio_pos = -1;
    }
    return;
  }
  else if (radio_pos == -1 && poll(fds, 1, 1) == 0) {
    return;
  }
  rcv_len = recvfrom(sock_udp, &buffer[0], buffer.size(), 0, (struct sockaddr *) &radio_address, &rcva_len);
  if (rcv_len < 0) {
    error("read");
  }
  buffer.resize(rcv_len);

  // std::cerr << "cos dostalem...\n";

  // check whether message is valid
  if (!checkReceivedMessage(buffer, rcv_len)) {
    return receiveStream(sock_udp, menu, timeout, radio_pos, radios);
  }

  std::string type = getType(bytesToInt(buffer[0], buffer[1]));
  int leng = bytesToInt(buffer[2], buffer[3]);
  buffer.erase(0, 4);

  if (radio_pos != -1 && type.compare("AUDIO") == 0 && compareRadios(radio_address, radios[radio_pos - 1].first)) {

    std::cout << buffer;

    std::cerr << "odebralem stream (" << buffer.size() << ") od " << radios[radio_pos - 1].first.sin_addr.s_addr << "(" << radios[radio_pos - 1].first.sin_port << ")\n";
  }
  else if (radio_pos != -1 && type == "METADATA" && compareRadios(radio_address, radios[radio_pos - 1].first)) {
    menu->changeMeta(buffer);
  }
  else if (type == "IAM") {
    std::cerr << "MAM NOWE RADIO!!!!!!!!!!!!!!!\n";
    radios.push_back(std::make_pair(radio_address, gettimelocal()));
    menu->addRadio(buffer);
  }
  // else {
  //   int xx = std::max(20000, (int)bufer.size());
  //   std::cout << bufer.substr(0, xx);
  //   bufer.erase(0, xx);
  // }
  // std::cerr << "BUFFER SIZE = " << bufer.size() << "\n";
}

// function that detects actions on the telnet menu, receive stream from
// the radio-proxy and send them KEEPALIVE messages
void runClient (int &sock_udp, struct sockaddr_in &my_address, struct sockaddr_in &broadcast_address, TelnetMenu *&menu, int timeout, int &radio_pos, RadiosDeque &radios) {
  int action = menu->runTelnet(1);
  /* akcja równa 1 oznacza, że ktoś wybrał w menu szukanie pośrednika */
  if (action == 1) {
    // for (int i = 0; i < radios.size() + 1; i++) {
      searchProxy(sock_udp, broadcast_address);
    // }
    receiveStream(sock_udp, menu, timeout, radio_pos, radios);
  }
  /* akcja równa 2 oznacza, że ktoś wybrał w menu radio do odtwarzania */
  else if (action == 2) {
    // return runClient (sock_udp, my_address, menu, timeout, menu->getCurrPos());

    radio_pos = menu->getCurrPos();
    // std::cerr << "booom selected radio #" << radio_pos << "\n";
    menu->setPlayingPos(radio_pos);
    my_address = radios[radio_pos - 1].first;
    sendKeepAlive(sock_udp, my_address);
  }
  //nic nie odtwarzamy, wiec nikomu nie wysylamy keepalive (czy aby na pewno?)
  // if (radio_pos < 0) {
  //   return;
  // }
  //czy takie mierzenie tego czasu jest ok?
  if (radio_pos != -1 && gettimelocal() - radios[radio_pos - 1].second >= 3500000) {
    std::cerr << "wysylam keep alive do " << radios[radio_pos - 1].first.sin_addr.s_addr << "(" << radios[radio_pos - 1].first.sin_port << ")\n";
    sendKeepAlive(sock_udp, my_address);
    radios[radio_pos - 1].second = gettimelocal();
  }
  receiveStream(sock_udp, menu, timeout, radio_pos, radios);
}

int main(int argc, char** argv) {
  int port_udp = -1, port_tcp = -1, timeout = DEFAULT_TIMEOUT, sock, sock_telnet;
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

  int radio_pos = -1;
  while (1) {
    runClient(sock, my_address, broadcast_address, menu, timeout, radio_pos, radios);
  }

  return 0;
}
