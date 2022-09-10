CC=g++
CXXFLAGS = -std=c++0x
CFLAGS=-I
WebServer: main.o
	$(CC) -o ./webserver main.o --std=c++11 -pthread
	rm -f ./*.o

clean:
	rm -f ./*.o