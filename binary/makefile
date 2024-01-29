# This is the name of the targets and the executable
PROGRAM_FILE := ucinode

UCINODE_TARGET := ucinode

CLEAN_TARGET := clean
HELP_TARGET := help

# These are the flags and command for the clean target
DELETE_FLAGS :=
DELETE_CMD := rm

# This is the compiler and the compile flags you want to use
COMPILER := gcc
COMPILE_FLAGS := -Wall -Werror -g -Og -std=gnu99 -oFast

SOURCE_DIR := ../source
OBJECT_DIR := ../object

SOURCE_FILES := $(wildcard $(SOURCE_DIR)/*.c)
HEADER_FILES := $(wildcard $(SOURCE_DIR)/*.h)
OBJECT_FILES := $(addprefix $(OBJECT_DIR)/, $(notdir $(SOURCE_FILES:.c=.o)))

all: $(UCINODE_TARGET)

$(UCINODE_TARGET): $(OBJECT_FILES) $(SOURCE_FILES) $(HEADER_FILES)
	$(COMPILER) $(OBJECT_FILES) $(LINKER_FLAGS) -o $(PROGRAM_FILE)

$(OBJECT_DIR)/%.o: $(SOURCE_DIR)/%.c 
	$(COMPILER) $< -c $(COMPILE_FLAGS) -o $@

# These are the files that should be cleaned
DELETE_FILES := $(OBJECT_DIR)/*.o $(PROGRAM_FILE)

# These are the files that should be kept (not automatically deleted)
.PRECIOUS: $(DELETE_FILES)

$(CLEAN_TARGET):
	$(DELETE_CMD) $(DELETE_FLAGS) $(DELETE_FILES)

$(HELP_TARGET):
	@echo $(UCINODE_TARGET) $(CLEAN_TARGET)
