
CC = g++
CFLAGS += -std=c++0x -Wall -Wextra

SOURCE = ideal_init.cpp
MODULE = ideal_init


ideal_init: $(OBJECTS)
	$(CC) $(SOURCE) $(CFLAGS) -o $(MODULE)

all: ideal_init

clean:
	-rm -f $(MODULE)

