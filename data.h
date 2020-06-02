#ifndef DATA_H
#define DATA_H

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
#include <deque>
#include <sys/time.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <algorithm>
#include "err.h"

#define DEFAULT_TIMEOUT 5
#define DEFAULT_META "no"
#define BUFFER_SIZE 2048
#define DISCOVER 1
#define IAM 2
#define KEEPALIVE 3
#define AUDIO 4
#define METADATA 6

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

unsigned long long gettimelocal() {
   struct timeval t = {0,0};
   gettimeofday(&t,NULL);
   return ((unsigned long long)t.tv_sec * 1000000) + t.tv_usec;
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

/* get radio name from header */
std::string getMetaName (std::string header) {
  std::size_t found = header.find("icy-name:");
  header.erase(0, found + strlen("icy-name:"));
  return header.substr(0, header.find("\r\n"));
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

int getMetaSize(std::string &buffer) {
  if (buffer.empty()) {
    error("Cannot get size of meta from empty buffer");
  }
  return (int)buffer[0] * 16;
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
  else {
    n = 6;
  }
  res += (char)((n >> 8) & 0xFF);
  res += (char)(n & 0xFF);
  res += (char)((length >> 8) & 0xFF);
  res += (char)(length & 0xFF);

  return res;
}

#endif //DATA_H
