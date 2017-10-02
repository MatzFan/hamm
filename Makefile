CC = gcc
CFLAGS = -O3 -Wall -Wextra -Werror -std=gnu99

all : hamm

clean :
	rm -f hamm

hamm : hamm.o Makefile
	$(CC) hamm.o -o hamm
