CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -pthread
LDFLAGS = -lws2_32

all: server.exe client.exe

server.exe: server.cpp
	$(CXX) $(CXXFLAGS) server.cpp -o server.exe $(LDFLAGS)

client.exe: client.cpp
	$(CXX) $(CXXFLAGS) client.cpp -o client.exe $(LDFLAGS)

clean:
	del /Q *.exe 2>nul || rm -f *.exe

.PHONY: all clean