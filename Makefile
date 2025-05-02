SRC = src
INCLUDE = include
TEST_SRC = tests

# Compiler settings
CC = gcc
CCFLAGS = -Wall -Werror -g -I$(INCLUDE)

# Source files
FSS_MANAGER_SRC = $(SRC)/fss_manager.c $(SRC)/fss_logic.c $(SRC)/hashmap.c $(SRC)/cli_parser.c
FSS_CONSOLE_SRC = $(SRC)/fss_console.c
WORKER_SRC = $(SRC)/worker.c

# Executables
FSS_MANAGER_EXEC = fss_manager
FSS_CONSOLE_EXEC = fss_console
WORKER_EXEC = worker
TEST_EXEC = test_fssmanager

# Default target
all: $(FSS_MANAGER_EXEC) $(FSS_CONSOLE_EXEC) $(WORKER_EXEC)

# Build main executables
$(FSS_MANAGER_EXEC): $(FSS_MANAGER_SRC)
	$(CC) $(CCFLAGS) -o $@ $^

$(FSS_CONSOLE_EXEC): $(FSS_CONSOLE_SRC)
	$(CC) $(CCFLAGS) -o $@ $^

$(WORKER_EXEC): $(WORKER_SRC)
	$(CC) $(CCFLAGS) -o $@ $^

# Run manager manually
run_fss_manager: $(FSS_MANAGER_EXEC)
	./$(FSS_MANAGER_EXEC) -l manager.log -c test_config.txt -n 5

# Run console manually
run_fss_console: $(FSS_CONSOLE_EXEC)
	./$(FSS_CONSOLE_EXEC) -l console.log

# Run fss_manager under Valgrind
valgrind_fss_manager_test: $(FSS_MANAGER_EXEC)
	valgrind --leak-check=full --show-leak-kinds=all ./$(FSS_MANAGER_EXEC) -l manager.log -c test_config.txt -n 5

# === Tests ===
# Build and run hashmap unit test
test_hashmap: $(TEST_SRC)/test_hashmap.c $(SRC)/hashmap.c
	$(CC) $(CCFLAGS) -o test_hashmap $^
	./test_hashmap

# Build test_fssall
test_fssall: $(TEST_SRC)/test_fssall.c $(SRC)/hashmap.c
	$(CC) $(CCFLAGS) -o test_fssall $(TEST_SRC)/test_fssall.c $(SRC)/hashmap.c

# Run all tests
run_tests: test_fssall
	./test_fssall

# Run with Valgrind
valgrind_test: test_fssall
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./test_fssall

# Create test config file
test_config.txt:
	echo "/tmp/source1 /tmp/target1" > test_config.txt
	mkdir -p /tmp/source1 /tmp/target1

# Create the script
fss_script.sh:
	echo '#!/bin/bash' > fss_script.sh
	cat $(SRC)/fss_script.sh >> fss_script.sh
	chmod +x fss_script.sh

# Clean up
clean:
	rm -f *.o $(FSS_MANAGER_EXEC) $(FSS_CONSOLE_EXEC) $(WORKER_EXEC) test_hashmap $(TEST_EXEC) fss_in fss_out test_config.txt test_log.txt manager.log console.log test_fssall