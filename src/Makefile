.POSIX:

CPPFLAGS = -std=c++17 -O2 -Wall -Wno-unused-function -ftrapv

.PHONY: all
all: icfpc

.PHONY: debug
debug: CPPFLAGS += -O0 -g -DDEBUG
debug: LDFLAGS += -g
debug: all

.PHONY: sanitize
sanitize: CPPFLAGS += -fsanitize=address
sanitize: LDFLAGS += -fsanitize=address
sanitize: all

icfpc: icfpc.o
icfpc.o: icfpc.cpp

.PHONY: clean
clean:
	rm -rf *.o *.dSYM icfpc
