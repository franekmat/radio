#ifndef TELNETMENU_H
#define TELNETMENU_H

#include <string>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <poll.h>
#include <stdexcept>
#include <vector>
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

  // checking whether clicked button was up arrow
  bool arrowUp (char *buf, int len) {
    if (len == 3 && buf[0] == 27 && buf[1] == 91 && buf[2] == 65) {
      return true;
    }
    return false;
  }

  // checking whether clicked button was down arrow
  bool arrowDown (char *buf, int len) {
    if (len == 3 && buf[0] == 27 && buf[1] == 91 && buf[2] == 66) {
      return true;
    }
    return false;
  }

  // checking whether clicked button was enter button
  bool enter (char *buf, int len) {
    if (len == 2 && buf[0] == 13 && buf[1] == 0) {
      return true;
    }
    return false;
  }

public:

  TelnetMenu(int port) : port(port) {}

  // set new telnet connection via TCP
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

  // add a new client - the new radio-proxy
  int addClient(int &sock) {
    int client_size = sizeof( struct sockaddr_in);
    struct sockaddr_in client_addr;
    msgsock = accept(sock, (struct sockaddr*) &client_addr, (socklen_t *) &client_size);
    if (msgsock == -1) {
      error("accept");
    }
    return msgsock;
  }

  // write string on the telnet menu
  void writeTelnet(std::string s) {
    if (write(msgsock, s.data(), s.size()) == -1) {
      error("send");
    }
  }

  // write linux cursor at the beginning on the pos line of the telnet menu
  void writeCursor(int pos) {
    writeTelnet("\033[" + std::to_string(pos + 1) + ";1H");
  }

  // clearing telnet menu vector and add 2 default options
  void setupTelnetMenu () {
    telnet_menu.clear();
    telnet_menu.push_back("Szukaj pośrednika");
    telnet_menu.push_back("Koniec");
  }

  // write all lines of the telnet menu from telnet_menu vector
  void writeTelnetMenu() {
    writeTelnet(CLEAR);
    for (int i = 0; i < telnet_menu.size(); i++) {
      std::string s = " " + telnet_menu[i]; // space before radio name for a cursor
      if (i == playing_pos) { // that line contains radio that is playing now
        s += POINTER; // place '*' on the right of the radio name
      }
      writeTelnet(s + NEW_LINE);
    }
    writeCursor(curr_pos);
  }

  // setup telnet menu by turning it into character mode and sending all default options
  void setupTelnet() {
    writeTelnet(SETUP_TELNET);
    setupTelnetMenu();
    curr_pos = 0;
    writeTelnetMenu();
  }

  // add new discovered radio to the telnet menu
  void addRadio(std::string s) {
    // the last option in the menu is the 'Koniec' option
    if (telnet_menu.back() == "Koniec") {
      telnet_menu.pop_back();
      telnet_menu.push_back(s);
      telnet_menu.push_back("Koniec");
    }
    // the last option in the menu is an informaton about playing radio
    else {
      std::string meta = telnet_menu.back();
      telnet_menu.pop_back();
      telnet_menu.pop_back();
      telnet_menu.push_back(s);
      telnet_menu.push_back("Koniec");
      telnet_menu.push_back(meta);
    }
    // curr_pos = 0;
    writeTelnetMenu();
  }

  // get current position of the cursor in the telnet menu
  int getCurrPos() {
    return curr_pos;
  }

  // set playing radio in the telnet menu
  // (by placing then '*' on the right of that radio name)
  void setPlayingPos(int pos) {
    playing_pos = pos;
    writeTelnetMenu();
  }

  // get the title of the playing song from meta data string
  std::string getTitleFromMeta(std::string meta) {
    if (meta.substr(0, 13) == "StreamTitle='") {
      std::string res = "";
      for (int i = 13; i < meta.size() && meta[i] != '\''; i++) {
        res += meta[i];
      }
      return res;
    }
    return meta;
  }

  // change title of the song displayed at the end of the menu to the new one
  void changeMeta(std::string s) {
    if (telnet_menu.back() != "Koniec") {
      telnet_menu.pop_back();
    }
    telnet_menu.push_back(getTitleFromMeta(s));
    curr_pos = 0;
    writeTelnetMenu();
  }

  // delete radio, which is displayed at the radio_pos line in the telnet menu
  void deleteRadio(int radio_pos) {
    telnet_menu.erase(telnet_menu.begin() + radio_pos);
    if (radio_pos == curr_pos) {
      curr_pos = 0;
    }
    if (radio_pos == playing_pos) {
      playing_pos = -1;
    }
    writeTelnetMenu();
  }

  // detect actions - pressed button on telnet menu
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
        int max_size = (int) telnet_menu.size() - 1;
        if (telnet_menu[max_size] != "Koniec") {
          max_size--;
        }
        curr_pos = std::min(curr_pos + 1, max_size);
        writeTelnetMenu();
      }
      else if (enter(buf, len)) {
        // search proxy
        if (curr_pos == 0) {
          return 1;
        }
        // close menu
        else if (curr_pos < telnet_menu.size() && telnet_menu[curr_pos] == "Koniec") {
          if (close(msgsock) < 0) {
            error("Closing socket");
          }
        }
        // change playing radio
        else {
          return 2;
        }
      }
    }
    return 0;
  }
};

#endif /* TELNETMENU_H */
