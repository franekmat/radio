1. Rozwiązanie na 16 punktów, tj. nie obsługuję rozsyłania grupowego w części B i C.
1. Rozwiązanie jest przeznaczone do testowania na maszynach o różnych adresach z tego względu, że zgodnie z treścią zadania każdy program radio-proxy wysyła klientom wiadomości z tego samego portu. Aby uruchomić rozwiązanie na jednej maszynie należy w radio-proxy, w mainie zmienić wywołanie funkcji "setUdpServerConnection(sock_udp, port_clients, false)" na "setUdpServerConnection(sock_udp, port_clients, true)"
1. Żeby “zrobić miejsce” dla linuksowego kursora w menu, przed każdą opcją (tj.  “Szukaj pośrednika”, “Koniec”, nazwami stacji oraz meta informacją o stacji) wypisuję jedną spację.
2. Stacje wysyłają metadane co jakiś czas, więc gdy włączymy stację pechowo tuż po tym jak radio wysyłało metadane, to na kolejne możemy trochę poczekać. Przez ten czas w ostatniej linii menu nie wyświetlają się żadne meta informacje, pojawią się dopiero o otrzymaniu meta danych od aktualnie odtwarzanego radia.
3. Przyjąłem brak obsługi polskich znaków w menu.
4. Czas - timeout, podany jako argument przy uruchomieniu radio-proxy i radio-client jest dodatnią liczbą całkowitą.
5.
