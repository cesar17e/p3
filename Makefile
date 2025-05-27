CC = gcc
CFLAGS =  -Wextra -g

# List of object files
OBJS = mysh.o arraylist.o builtInCommands.o

# Default target: build mysh
all: mysh

# Link the object files to create the executable 'mysh'
mysh: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o mysh

# Pattern rule: compile .c file into .o file
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean: remove the executable and object files
clean:
	rm -f mysh $(OBJS)