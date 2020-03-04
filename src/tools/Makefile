
TOOL_SRC_DIR    = ./

MODELBAKE_SRC   = $(TOOL_SRC_DIR)/modelbake.c
MODELBAKE_OBJ   = $(patsubst $(TOOL_SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(MODELBAKE_SRC))
MODELBAKE_LIBS  = -lassimp
MODELBAKE_DEPS  = $(OBJ_DIR)/hashmap.o
MODELBAKE_BIN   = $(ROOT_DIR)/modelbake

all: $(MODELBAKE_BIN)

$(MODELBAKE_BIN): $(MODELBAKE_OBJ)
	$(CC) -o $@ $^ $(MODELBAKE_DEPS) $(CFLAGS) $(LIBS) $(MODELBAKE_LIBS) 

$(OBJ_DIR)/%.o: $(TOOL_SRC_DIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f $(MODELBAKE_BIN)