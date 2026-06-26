CC = gcc
CFLAGS = -Wall -g -I../X32_emulator

all: test_x32_port

test_x32_port: test_x32_port.c ../X32_emulator/x32_port.h
	$(CC) $(CFLAGS) -o $@ test_x32_port.c

run: test_x32_port
	./test_x32_port

clean:
	rm -f test_x32_port
