.POSIX:

CPPFLAGS = -std=c++17 -O2 -Wall -Wno-unused-function -ftrapv

.PHONY: all
all: icfpc

.PHONY: debug
debug: CPPFLAGS += -Og -g -DDEBUG
debug: LDFLAGS += -g
debug: all

icfpc: icfpc.o
icfpc.o: icfpc.cpp

solve_lambdaman: solve_lambdaman.o
solve_lambdaman.o: solve_lambdaman.cpp

.PHONY: clean
clean:
	rm -rf *.o icfpc solve_lambdaman
