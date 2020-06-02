#ifndef TELNETMENU_H
#define TELNETMENU_H

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
#include <sys/time.h>
#include "err.h"

class TelnetMenu {
private:
  const int QUEUE_LENGTH = 5;
  const std::string SETUP_TELNET = "\377\375\042\377\373\001";
  const std::string CLEAR = "\033[1J\033[H";
  const std::string POINTER = " *";
  const std::string NEW_LINE = "\r\n";

  std::vector<std::string> telnet_menu;
  int port, msgsock, curr_pos = 0, playing_pos = -1;
  struct sockaddr_in server;

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

public:

  TelnetMenu(int port) : port(port) {}

  void newTelnetConnection(int &sock) {
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
      error("Opening stream socket");
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&server, (socklen_t)sizeof(server)) == -1) {
      error("Binding stream socket");
    }

    if (listen(sock, QUEUE_LENGTH) == -1) {
      error("Starting to listen");
    }
  }

  int addClient(int &sock) {
    int client_size = sizeof( struct sockaddr_in);
    struct sockaddr_in client_addr;
    msgsock = accept(sock, (struct sockaddr*) &client_addr, (socklen_t *) &client_size);
    if (msgsock == -1) {
      error("accept");
    }
    return msgsock;
  }

  void writeTelnet(std::string s) {
    if (write(msgsock, s.data(), s.size()) == -1) {
      error("send");
    }
  }

  void writeCursor(int pos) {
    writeTelnet("\033[" + std::to_string(pos + 1) + ";1H");
  }

  void setupTelnetMenu () {
    telnet_menu.clear();
    telnet_menu.push_back("Szukaj pośrednika");
    telnet_menu.push_back("Koniec");
  }

  void writeTelnetMenu() {
    writeTelnet(CLEAR);
    for (int i = 0; i < telnet_menu.size(); i++) {
      std::string s = " " + telnet_menu[i];
      if (i == playing_pos) {
        s += POINTER;
      }
      writeTelnet(s + NEW_LINE);
    }
    writeCursor(curr_pos);
  }

  void setupTelnet() {
    writeTelnet(SETUP_TELNET);
    setupTelnetMenu();
    curr_pos = 0;
    writeTelnetMenu();
  }

  void addRadio(std::string s) {
    if (telnet_menu.back() == "Koniec") {
      telnet_menu.pop_back();
      telnet_menu.push_back(s);
      telnet_menu.push_back("Koniec");
    }
    else {
      std::string meta = telnet_menu.back();
      telnet_menu.pop_back();
      telnet_menu.pop_back();
      telnet_menu.push_back(s);
      telnet_menu.push_back("Koniec");
      telnet_menu.push_back(meta);
    }
    curr_pos = 0;
    writeTelnetMenu();
  }

  int getCurrPos() {
    return curr_pos;
  }

  void setPlayingPos(int pos) {
    playing_pos = pos;
    writeTelnetMenu();
  }

  void changeMeta(std::string s) {
    if (telnet_menu.back() != "Koniec") {
      telnet_menu.pop_back();
    }
    telnet_menu.push_back(s);
    curr_pos = 0;
    writeTelnetMenu();
  }

  void deleteRadio(int radio_pos) {
    telnet_menu.erase(telnet_menu.begin() + radio_pos);
    //zmiana curr_pos?
    writeTelnetMenu();
  }

  int runTelnet(int timeout) {
    int buf_size = 16, len;
    char buf[buf_size];
    struct pollfd fds[1] = {{msgsock, 0 | POLLIN}};

    if (timeout == 0 || poll(fds, 1, timeout) != 0) {
      len = recv(msgsock, buf, buf_size, 0);
      if (arrowUp(buf, len)) {
        curr_pos = std::max(0, curr_pos - 1);
        writeTelnetMenu();
      }
      else if (arrowDown(buf, len)) {
          curr_pos = std::min(curr_pos + 1, (int)telnet_menu.size() - 1);
          writeTelnetMenu();
      }
      else if (enter(buf, len)) {
        if (curr_pos == 0) {
          return 1;
        }
        else if (curr_pos == (int) telnet_menu.size() - 1) {
          if (close(msgsock) < 0) {
            error("Closing socket");
          }
        }
        else {
          return 2;
        }
      }
    }
    return 0;
  }
};

#endif /* TELNETMENU_H */
