CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Werror
TARGET = writer
SRC = writer.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) *.o