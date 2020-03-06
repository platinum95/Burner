CC          = gcc
GLSLC       = glslc
INCLUDES    = -I$(PWD)/include -I$(PWD)/CMore/
CFLAGS      = $(INCLUDES) -O0 -Wall -Werror -Wextra -Wformat=2 -Wshadow -pedantic -g -Werror=vla
LIBS        = -lm -lpthread -lvulkan -lglfw
MODELBAKE   = ./modelbake

DEFINES     = -DBURNER_VERSION_MAJOR=0 -DBURNER_VERSION_MINOR=0 -DBURNER_VERSION_PATCH=0
DEFINES    := -DBURNER_NAME="Burner"

ROOT_DIR        = $(CURDIR)
SRC_DIR         = $(ROOT_DIR)/src
OBJ_DIR         = $(ROOT_DIR)/obj
TESTS_DIR       = $(ROOT_DIR)/src/tests
RES_DIR         = $(ROOT_DIR)/res
RAW_RES_DIR     = $(ROOT_DIR)/rawres
SHADER_SRC_DIR  = $(SRC_DIR)/shaders
SHADER_OBJ_DIR  = $(RES_DIR)/shaders
RAW_MODELS_DIR  = $(RAW_RES_DIR)/models
BAKED_MODELS_DIR= $(RES_DIR)/models
TOOLS_DIR       = $(SRC_DIR)/tools
CMORE_DIR       = $(ROOT_DIR)/CMore

DIRS_TO_MAKE   := $(OBJ_DIR) $(RES_DIR) $(SHADER_OBJ_DIR) $(BAKED_MODELS_DIR)

ALL_SRC     = $(wildcard $(SRC_DIR)/*.c)
ALL_TESTS   = $(wildcard $(TESTS_DIR)/*.c)
ALL_SHADERS = $(wildcard $(SHADER_SRC_DIR)/*)
ALL_MODELS  := $(wildcard $(RAW_MODELS_DIR)/*/*.obj )

BURNER_SRC  = $(SRC_DIR)/main.c
TEST_SRC    = $(TESTS_DIR)/tests.c

# Remove the entry-point sources from the source lists
SRC         = $(filter-out $(BURNER_SRC),$(ALL_SRC))
TESTS_SRC   = $(filter-out $(TEST_SRC),$(ALL_TESTS))

OBJ         = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC))
BURNER_OBJ  = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(BURNER_SRC))
TESTS_OBJ   = $(patsubst $(TESTS_DIR)/%.c,$(OBJ_DIR)/%.o,$(TESTS_SRC))
TEST_OBJ    = $(patsubst $(TESTS_DIR)/%.c,$(OBJ_DIR)/%.o,$(TEST_SRC))
SHADERS_OBJ = $(patsubst $(SHADER_SRC_DIR)/%,$(SHADER_OBJ_DIR)/%.spv,$(ALL_SHADERS))
BAKED_MODELS= $(patsubst $(RAW_MODELS_DIR)/%.obj,$(BAKED_MODELS_DIR)/%.pomf,$(ALL_MODELS))
BAKED_MODELS := $(patsubst %.obj,$(BAKED_MODELS_DIR)/%.pomf,$(notdir $(ALL_MODELS)))

CMORE_STATIC_LIB = $(ROOT_DIR)/cmore.a

export CMORE_STATIC_LIB
export CFLAGS
export LIBS

all: burner tests

burner: $(OBJ) $(BURNER_OBJ) $(CMORE_STATIC_LIB) | $(SHADERS_OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

tests: $(OBJ) $(TESTS_OBJ) $(TEST_OBJ) $(CMORE_STATIC_LIB) | $(SHADERS_OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

tools:
	$(MAKE) -C $(TOOLS_DIR) OBJ_DIR=$(OBJ_DIR)

.PHONY: models
models: $(BAKED_MODELS)

$(CMORE_STATIC_LIB):
	$(MAKE) -e -C $(CMORE_DIR) $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(OBJ_DIR)/%.o: $(TESTS_DIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SHADER_OBJ_DIR)/%.spv: $(SHADER_SRC_DIR)/%
	$(GLSLC)  $< -o $@

$(BAKED_MODELS_DIR)/%.pomf: $(filter %$(basename $(notdir $@)).obj,$(ALL_MODELS)) tools
	$(MODELBAKE) $< $@

.PHONY: clean
clean:
	rm -f burner tests
	rm -r -f $(DIRS_TO_MAKE)
	$(MAKE) -C $(TOOLS_DIR) clean
	$(MAKE) -C $(CMORE_DIR) clean

# Make the obj directory
$(shell mkdir -p $(DIRS_TO_MAKE))
$(info $(shell \
	if [ ! -e "./CMore/.git" ];\
		then git submodule init && git submodule update;\
	fi ))