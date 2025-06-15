CROSS_COMPILE ?=
CC := $(CROSS_COMPILE)gcc

CFLAGS += -Wall -Wextra -pedantic
LDLIBS += -pthread -lrt

TARGET = aesdsocket
SRCS = aesdsocket.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

clean:
	$(RM) $(TARGET)

.PHONY: all clean

