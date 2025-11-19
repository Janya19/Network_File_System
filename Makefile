# --- Variables ---
CC = gcc

# CFLAGS are the "compiler flags"
# -g: Adds debug symbols (for GDB)
# -Wall: Turns on "all" warnings
# -Iinclude: Tells gcc to look for headers in the 'include' folder
CFLAGS = -g -Wall -Iinclude

# THREAD_LIBS are for programs that use pthreads
THREAD_LIBS = -pthread

# --- Phony Targets ---
# Tells 'make' that these targets don't create files named 'all' or 'clean'
.PHONY: all clean

# --- Targets ---

# 'all' is the default target. If you just type 'make', it will build all three.
all: bin/name_server bin/storage_server bin/client

#add folder ss_data/ss1
	@mkdir -p ss_data/ss1

# Target for the Name Server (needs threads)
bin/name_server: name_server/nm.c
	@mkdir -p bin
	$(CC) $(CFLAGS) $(THREAD_LIBS) -o bin/name_server name_server/nm.c $(THREAD_LIBS)
	@echo "Compiled Name Server!"

# Target for the Storage Server (needs threads)
bin/storage_server: storage_server/ss.c
	@mkdir -p bin
	$(CC) $(CFLAGS) $(THREAD_LIBS) -o bin/storage_server storage_server/ss.c $(THREAD_LIBS)
	@echo "Compiled Storage Server!"

# Target for the Client (does NOT need threads)
bin/client: client/client.c
	@mkdir -p bin
	$(CC) $(CFLAGS) -o bin/client client/client.c
	@echo "Compiled Client!"

# 'clean' is a special target to clean up your project
clean:
	@echo "Cleaning compiled executables..."
	@rm -f bin/name_server bin/storage_server bin/client
	@echo "Cleaning logs and metadata..."
	@rm -f nm.log ss.log nm_metadata.dat
	@echo "Cleaning all stored files..."
	@rm -rf ss_data
	@echo "Cleanup complete."