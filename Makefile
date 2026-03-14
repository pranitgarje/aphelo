# Compiler settings
CXX = g++
CXXFLAGS = -Wall -Wextra -O2 -g

# Define the output executables and the new library
TARGETS = server client
LIBRARY = libavl.a

# Default target: builds both server and client
all: $(TARGETS)

# ---------------------------------------------------------
# 1. BUILD THE AVL STATIC LIBRARY
# ---------------------------------------------------------
# Step A: Compile avl.cpp into an object file (avl.o)
avl.o: avl.cpp avl.h
	$(CXX) $(CXXFLAGS) -c avl.cpp -o avl.o

# Step B: Archive the object file into a static library (libavl.a)
$(LIBRARY): avl.o
	ar rcs $(LIBRARY) avl.o

# ---------------------------------------------------------
# 2. SERVER & CLIENT BUILD RULES
# ---------------------------------------------------------
# Server now depends on the static library
server: server.cpp hashtable.cpp hashtable.h $(LIBRARY)
	$(CXX) $(CXXFLAGS) server.cpp hashtable.cpp -L. -lavl -o server

client: client.cpp $(LIBRARY)
	$(CXX) $(CXXFLAGS) client.cpp -o client

# Cleanup rule to remove binaries, object files, and libraries
clean:
	rm -f $(TARGETS) *.o *.a

.PHONY: all clean