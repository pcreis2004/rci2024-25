# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -g -O3

# Executable name
EXEC = ndn

# Source files
SRC = main.c lib.c commands.c

# Header files
HEADERS = lib.h commands.h 

# Object files (replace .c extensions with .o)
OBJ = $(SRC:.c=.o)

# Rule to build the executable
$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) -o $(EXEC) $(OBJ)

# Rule to compile object files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJ) $(EXEC)
