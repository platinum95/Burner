INCLUDES    = ./include
CC          = gcc
CFLAGS      = -I$(INCLUDES) -O0 -Wall -Werror -Wextra -Wformat=2 -Wshadow -pedantic -g

SRC_DIR     = ./src
OBJ_DIR     = ./obj
TESTS_DIR   = ./src/tests

ALL_SRC     = $(wildcard $(SRC_DIR)/*.c)
ALL_TESTS   = $(wildcard $(TESTS_DIR)/*.c)

BURNER_SRC  = ./src/main.c
TEST_SRC    = ./src/tests/tests.c

# Remove the entry-point sources from the source lists
SRC         = $(filter-out $(BURNER_SRC),$(ALL_SRC))
TESTS_SRC   = $(filter-out $(TEST_SRC),$(ALL_TESTS))

OBJ         = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC))
BURNER_OBJ  = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(BURNER_SRC))
TESTS_OBJ   = $(patsubst $(TESTS_DIR)/%.c,$(OBJ_DIR)/%.o,$(TESTS_SRC))
TEST_OBJ    = $(patsubst $(TESTS_DIR)/%.c,$(OBJ_DIR)/%.o,$(TEST_SRC))
LIBS        = -lm -lpthread

all: burner tests

burner: $(OBJ) $(BURNER_OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

tests: $(OBJ) $(TESTS_OBJ) $(TEST_OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(OBJ_DIR)/%.o: $(TESTS_DIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(OBJ_DIR):
	mkdir $@

.PHONY: clean
clean:
	rm -f *~ core burner tests $(INCDIR)/*~
	rm -r $(OBJ_DIR)

# Make the obj directory
$(info $(shell mkdir -p $(OBJ_DIR)))