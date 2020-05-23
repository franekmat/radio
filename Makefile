TARGET: radio-proxy

CC	= cc
CFLAGS	= -Wall -O2
LFLAGS	= -Wall

radio-proxy.o err.o: err.h

radio-proxy: radio-proxy.o err.o
	$(CC) $(LFLAGS) $^ -o $@

.PHONY: clean TARGET
clean:
	rm -f radio-proxy *.o *~ *.bak *.conf
