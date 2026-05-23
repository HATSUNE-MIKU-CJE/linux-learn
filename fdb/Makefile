CC      = gcc
CFLAGS  = -Wall -Wextra -g -O2
LDFLAGS =
TARGET  = fdb
OBJS    = main.o fdb.o record.o index.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS)
