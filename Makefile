# compiler stuff
CC ?= cc
CFLAGS ?= -mtune=generic -Os -pipe -Wall -pedantic
LDFLAGS = $(shell pkg-config --libs libudev)

# installation stuff
PREFIX ?= /usr
BIN_DIR ?= $(PREFIX)/bin
SBIN_DIR ?= $(PREFIX)/sbin

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

part-hotplug-monitor: part-hotplug-monitor.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean:
	rm -v -f part-hotplug-monitor *.o

install: part-hotplug-monitor
	install -v -m 755 -D part-hotplug-monitor $(SBIN_DIR)/part-hotplug-monitor
	install -v -m 755 -D part-hotplug-handler $(SBIN_DIR)/part-hotplug-handler
	install -v -m 755 -D part-icon $(BIN_DIR)/part-icon

uninstall:
	rm -v -f $(BIN_DIR)/part-icon
	rm -v -f $(SBIN_DIR)/part-hotplug-handler
	rm -v -f $(SBIN_DIR)/part-hotplug-monitor