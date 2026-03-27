CC=gcc
CFLAGS=-Wall -Wextra -O2
SRC=src
BIN=bin

TCP_TARGETS=$(BIN)/broker_tcp $(BIN)/publisher_tcp $(BIN)/subscriber_tcp
UDP_TARGETS=$(BIN)/broker_udp $(BIN)/publisher_udp $(BIN)/subscriber_udp

all: dirs tcp udp

dirs:
	mkdir -p $(BIN)

tcp: dirs $(TCP_TARGETS)

udp: dirs $(UDP_TARGETS)

$(BIN)/broker_tcp: $(SRC)/broker_tcp.c
	$(CC) $(CFLAGS) $< -o $@

$(BIN)/publisher_tcp: $(SRC)/publisher_tcp.c
	$(CC) $(CFLAGS) $< -o $@

$(BIN)/subscriber_tcp: $(SRC)/subscriber_tcp.c
	$(CC) $(CFLAGS) $< -o $@

$(BIN)/broker_udp: $(SRC)/broker_udp.c
	$(CC) $(CFLAGS) $< -o $@

$(BIN)/publisher_udp: $(SRC)/publisher_udp.c
	$(CC) $(CFLAGS) $< -o $@

$(BIN)/subscriber_udp: $(SRC)/subscriber_udp.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf $(BIN)