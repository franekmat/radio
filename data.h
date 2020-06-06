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
#include <fcntl.h>
#include <deque>
#include <sys/time.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <algorithm>
#include "err.h"

#define DEFAULT_TIMEOUT 5
#define DEFAULT_META "no"
#define BUFFER_SIZE 1024
#define HEADER_SIZE 4
#define DISCOVER 1
#define IAM 2
#define KEEPALIVE 3
#define AUDIO 4
#define METADATA 6

// check if given string contains only digits
bool containsOnlyDigits (std::string s) {
  // we want to ignore leading and trailing spaces
  int l = 0, p = s.size() - 1;
  while (l < s.size() && s[l] == ' ') {
    l++;
  }
  while (p >= 0 && s[p] == ' ') {
    p--;
  }
  // now check if number contains something other than digit characters
  for (int i = l; i <= p; i++) {
    if (s[i] < '0' || s[i] > '9') {
      return false;
    }
  }
  return true;
}

// get int value from the string or return error if it is not valid number
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

// check if meta parameter is valid
void checkMeta (std::string meta) {
  if (meta != "yes" && meta != "no") {
    error ("Wrong format of meta argument");
  }
}

// gettimeofday in microsecond
unsigned long long gettimelocal() {
   struct timeval t = {0,0};
   gettimeofday(&t,NULL);
   return ((unsigned long long)t.tv_sec * 1000000) + t.tv_usec;
}

// convert received in network byte order number as 2 bytes into host byte order int
// https://stackoverflow.com/questions/5585532/c-int-to-byte-array/43515755#43515755
int bytesToInt (char b1, char b2) {
  int res = 0;
  res <<= 8;
  res |= b1;
  res <<= 8;
  res |= b2;
  return ntohs(res);
}

// convert host byte order int to 2 bytes in network byte order
// https://stackoverflow.com/questions/5585532/c-int-to-byte-array/43515755#43515755
std::string intToBytes (int x) {
  std::string res = "";
  x = htons(x);
  res += (char)((x >> 8) & 0xFF);
  res += (char)(x & 0xFF);
  return res;
}

// get string type from int value, as in the task description
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
  else if (n == 6) {
    return "METADATA";
  }
  else {
    error("Incorrect type of data");
  }
}

// checking if received header contains icy-metaint
bool containsMeta (std::string header) {
  return (header.find("icy-metaint:") != std::string::npos);
}

// get icy-metaint value from the header string
int getMetaInt (std::string header) {
  std::size_t found = header.find("icy-metaint:");
  header.erase(0, found + strlen("icy-metaint:"));
  std::string value = header.substr(0, header.find("\r\n"));
  return getValueFromString(value, "metaint");
}

// get radio name from the header string
std::string getMetaName (std::string header) {
  std::size_t found = header.find("icy-name:");
  header.erase(0, found + strlen("icy-name:"));
  return header.substr(0, header.find("\r\n"));
}

// checking if given buffer string contains the end of the header
bool containsEndOfHeader(std::string &buffer) {
  return (buffer.find("\r\n\r\n") != std::string::npos);
}

// get status from the header of the received response from the server
std::string getStatus (std::string &header) {
  std::size_t found = header.find("\r\n");
  return header.substr(0, found);
}

// checking if status of the response is OK or not
bool okStatus (std::string &status) {
  return (status == "HTTP/1.0 200 OK" || status == "HTTP/1.1 200 OK" || status == "ICY 200 OK");
}

// cut header from the buffer, we know that it is there, and return header
std::string getHeader(std::string &buffer) {
  std::string header = "";
  std::size_t found = buffer.find("\r\n\r\n");
  if (found != std::string::npos) {
    header = buffer.substr(0, found);
    buffer.erase(0, found + strlen("\r\n\r\n"));
  }
  return header;
}

// get size of the meta data, which is 16 times first byte of meta data
int getMetaSize(std::string &buffer) {
  if (buffer.empty()) {
    error("Cannot get size of meta from empty buffer");
  }
  return (int)buffer[0] * 16;
}

// get 4 byte header of the message from type and length of it
std::string getUdpHeader (std::string type, int length) {
  if (type == "DISCOVER") {
    return intToBytes(1) + intToBytes(length);
  }
  else if (type == "IAM") {
    return intToBytes(2) + intToBytes(length);
  }
  else if (type == "KEEPALIVE") {
    return intToBytes(3) + intToBytes(length);
  }
  else if (type == "AUDIO") {
    return intToBytes(4) + intToBytes(length);
  }
  else if (type == "METADATA") {
    return intToBytes(6) + intToBytes(length);
  }
  else {
    error ("Incorrect type of data");
  }
}

// checking if received message contains proper header and has valid length
bool checkReceivedMessage(std::string buffer, int len) {
  if (buffer.size() < 4) {
    return false;
  }
  std::string type = getType(bytesToInt(buffer[0], buffer[1]));
  int length = bytesToInt(buffer[2], buffer[3]);
  return length == len - 4;
}

// checking if given address is a multicast address
bool isMulticastAddress(in_addr_t address) {
  return (address >> 28) == 14;
}

#endif //DATA_H
