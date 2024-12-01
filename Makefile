# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -std=c++17

# Static library name
LIBRARY = libdatabase.a

# Object files
OBJ = REPL.o database.o 

# Target: Create the static library
$(LIBRARY): $(OBJ)
	$(AR) rcs $(LIBRARY) $(OBJ)

# Rule to compile .cpp files to .o object files
database.o: database.cpp database.h
	$(CXX) $(CXXFLAGS) -c database.cpp
REPL.o: REPL.cpp REPL.h
	$(CXX) $(CXXFLAGS) -c REPL.cpp
# Clean up generated files
clean:
	rm -f $(OBJ) $(LIBRARY)

# Phony targets
.PHONY: clean
