CC = x86_64-w64-mingw32-gcc
GIT_VERSION ?= $(shell git describe --tags --always --dirty 2>/dev/null || echo "dev")
CFLAGS = -O2 -Wall -Wextra -std=c11 -I./src -DMOD_VERSION=\"$(GIT_VERSION)\"
LDFLAGS = -shared -static -static-libgcc -s src/version.def

SRCS = src/main.c src/logger.c src/scanner.c src/patches.c src/proxy.c src/context_hook.c src/steam_input.c
OBJS = $(SRCS:.c=.o)
TARGET = version.dll

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
