# =============================================================================
# Makefile — Student Database System (Client-Server Edition)
# =============================================================================
#
# Targets:
#   make          — build both server.exe and client.exe
#   make server   — build server only
#   make client   — build client only
#   make run-server  — build + run server on default port (8080)
#   make run-client  — build + run client connecting to 127.0.0.1:8080
#   make clean    — remove all build artifacts
#
# Compiler: gcc (MinGW on Windows, gcc on Linux/macOS)
# Standard: C99
# =============================================================================

CC      = gcc
CFLAGS  = -Wall -Wextra -Wpedantic -std=c99 -g -O2
IFLAGS  = -I. -Icommon -Iserver -Iclient -Isrc

# Windows-specific linker flags
ifdef OS
    # OS is set on Windows (cmd / PowerShell)
    LDFLAGS_SERVER = -lws2_32
    LDFLAGS_CLIENT = -lws2_32
    SERVER_BIN = server.exe
    CLIENT_BIN = client.exe
    RM = del /Q /F
else
    LDFLAGS_SERVER =
    LDFLAGS_CLIENT =
    SERVER_BIN = server
    CLIENT_BIN = client
    RM = rm -f
endif

# -----------------------------------------------------------------------------
# Shared source files (compiled into both server and client)
# -----------------------------------------------------------------------------
SHARED_SRCS = src/validation.c \
              src/protocol.c

# -----------------------------------------------------------------------------
# Server source files
# -----------------------------------------------------------------------------
SERVER_SRCS = $(SHARED_SRCS)    \
              src/student.c     \
              src/file_manager.c \
              src/search.c      \
              src/sort.c        \
              server/database.c \
              server/request_handler.c \
              server/server.c

# -----------------------------------------------------------------------------
# Client source files
# -----------------------------------------------------------------------------
CLIENT_SRCS = $(SHARED_SRCS)    \
              src/student.c     \
              client/client.c   \
              client/client_menu.c

# -----------------------------------------------------------------------------
# Default target
# -----------------------------------------------------------------------------
.PHONY: all server client run-server run-client clean

all: server client
	@echo ""
	@echo "  Build complete."
	@echo "  Run:  $(SERVER_BIN)   (in one terminal)"
	@echo "  Run:  $(CLIENT_BIN)   (in another terminal)"
	@echo ""

# -----------------------------------------------------------------------------
# Server build
# -----------------------------------------------------------------------------
server: $(SERVER_BIN)

$(SERVER_BIN): $(SERVER_SRCS)
	$(CC) $(CFLAGS) $(IFLAGS) $(SERVER_SRCS) -o $(SERVER_BIN) $(LDFLAGS_SERVER)
	@echo "  [OK] Built $(SERVER_BIN)"

# -----------------------------------------------------------------------------
# Client build
# -----------------------------------------------------------------------------
client: $(CLIENT_BIN)

$(CLIENT_BIN): $(CLIENT_SRCS)
	$(CC) $(CFLAGS) $(IFLAGS) $(CLIENT_SRCS) -o $(CLIENT_BIN) $(LDFLAGS_CLIENT)
	@echo "  [OK] Built $(CLIENT_BIN)"

# -----------------------------------------------------------------------------
# Run targets
# -----------------------------------------------------------------------------
run-server: server
	./$(SERVER_BIN)

run-client: client
	./$(CLIENT_BIN) 127.0.0.1 8080

# -----------------------------------------------------------------------------
# Clean
# -----------------------------------------------------------------------------
clean:
	$(RM) $(SERVER_BIN) $(CLIENT_BIN) *.o
	@echo "  [OK] Cleaned build artifacts"
