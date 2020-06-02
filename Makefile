CC = g++
CFLAGS = -O2 -Wall -std=c++11

all: radio-proxy radio-client

radio-proxy: radio-proxy.cpp
	$(CC) $(CFLAGS) -o radio-proxy radio-proxy.cpp

radio-client: radio-client.cpp
	$(CC) $(CFLAGS) -o radio-client radio-client.cpp

clean:
	rm radio-proxy radio-client
