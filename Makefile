INCLUDES = ./include
CC=gcc
CFLAGS=-I$(INCLUDES) -O0 -Wall -Werror -Wextra -Wformat=2 -Wshadow -pedantic -pg

SRC_DIR = ./src
SRC = $(wildcard $(SRC_DIR)/*.c)

OBJ_DIR = obj

LIBS = -lm -lpthread

OBJ = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC))

burner: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(OBJ_DIR)
	$(CC) -c -o $@ $< $(CFLAGS)

$(OBJ_DIR):
	mkdir $@

.PHONY: clean

clean:
	rm -f $(OBJ_DIR)/*.o *~ core $(INCDIR)/*~

