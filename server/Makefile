TARGET = aesdsocket
SRCS = aesdsocket.c

CFLAGS += -Wall -Wextra -pedantic
LDLIBS += -pthread -lrt

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	$(RM) $(TARGET)

.PHONY: all clean

