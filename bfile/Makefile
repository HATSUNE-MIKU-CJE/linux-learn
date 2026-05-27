CC      = gcc
CFLAGS  = -Wall -Wextra -g -O2
LDFLAGS =
TARGET  = bfile
OBJS    = main.o parse.o read.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS)
