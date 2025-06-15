# server/Makefile

CC       ?= $(CROSS_COMPILE)gcc
CFLAGS   += -Wall -Wextra -pedantic
LDFLAGS  += 
LDLIBS   += -pthread -lrt

TARGET   = aesdsocket
SRCS     = aesdsocket.c

all: $(TARGET)

default: all

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	$(RM) $(TARGET)

.PHONY: all default clean
