Radio stream transmitter and receiver with control menu via telent.

Usage:

- playing stream directly via radio-proxy:

./radio-proxy -h waw02-03.ic.smcdn.pl -r /t050-1.mp3 -p 8000 >waw.mp3

- using radio-proxy as a proxy and playing stream via radio-clients

./radio-proxy -h waw02-03.ic.smcdn.pl -r /t050-1.mp3 -p 8000 -P 10000 -t 10

./radio-client -H 255.255.255.255 -P 10000 -p 20000 -t 10 >/dev/null


- telnet menu for radio-client:

![Telnet menu](https://i.imgur.com/mH7cPZw.png?1)


