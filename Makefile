CC          = gcc
GLSLC       = glslc
INCLUDES    = $(PWD)/include
CFLAGS      = -I$(INCLUDES) -O0 -Wall -Werror -Wextra -Wformat=2 -Wshadow -pedantic -g -Werror=vla
LIBS        = -lm -lpthread -lvulkan -lglfw

DEFINES     = -DBURNER_VERSION_MAJOR=0 -DBURNER_VERSION_MINOR=0 -DBURNER_VERSION_PATCH=0
DEFINES    := -DBURNER_NAME="Burner"

ROOT_DIR        = $(PWD)/
SRC_DIR         = ./src
OBJ_DIR         = $(PWD)/obj
TESTS_DIR       = ./src/tests
RES_DIR         = ./res
SHADER_SRC_DIR  = ./src/shaders
SHADER_OBJ_DIR  = $(RES_DIR)/shaders

ALL_SRC     = $(wildcard $(SRC_DIR)/*.c)
ALL_TESTS   = $(wildcard $(TESTS_DIR)/*.c)
ALL_SHADERS = $(wildcard $(SHADER_SRC_DIR)/*)

BURNER_SRC  = ./src/main.c
TEST_SRC    = ./src/tests/tests.c

# Remove the entry-point sources from the source lists
SRC         = $(filter-out $(BURNER_SRC),$(ALL_SRC))
TESTS_SRC   = $(filter-out $(TEST_SRC),$(ALL_TESTS))

OBJ         = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC))
BURNER_OBJ  = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(BURNER_SRC))
TESTS_OBJ   = $(patsubst $(TESTS_DIR)/%.c,$(OBJ_DIR)/%.o,$(TESTS_SRC))
TEST_OBJ    = $(patsubst $(TESTS_DIR)/%.c,$(OBJ_DIR)/%.o,$(TEST_SRC))
SHADERS_OBJ = $(patsubst $(SHADER_SRC_DIR)/%,$(SHADER_OBJ_DIR)/%.spv,$(ALL_SHADERS))

TOOLS_DIR   = ./src/tools

export CFLAGS
export OBJ_DIR
export LIBS
export ROOT_DIR

all: burner tests tools

burner: $(OBJ) $(BURNER_OBJ) | $(SHADERS_OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

tests: $(OBJ) $(TESTS_OBJ) $(TEST_OBJ) | $(SHADERS_OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

tools:
	$(MAKE) -C $(TOOLS_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(OBJ_DIR)/%.o: $(TESTS_DIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SHADER_OBJ_DIR)/%.spv: $(SHADER_SRC_DIR)/%
	$(GLSLC)  $< -o $@

$(OBJ_DIR):
	mkdir $@

$(SHADER_OBJ_DIR):
	mkdir -p $@

$(RES_DIR):
	mkdir -p $@

.PHONY: clean
clean:
	rm -f *~ core burner tests $(INCDIR)/*~
	rm -r $(OBJ_DIR) $(RES_DIR)
	$(MAKE) -C $(TOOLS_DIR) clean

# Make the obj directory
$(shell mkdir -p $(OBJ_DIR))
$(shell mkdir -p $(RES_DIR))
$(shell mkdir -p $(SHADER_OBJ_DIR))