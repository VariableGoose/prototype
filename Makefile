BIN := bin/prototype

CC := gcc
CFLAGS := -std=c99 -Wall -Wextra -ggdb -MD -MP
IFLAGS := -Iinclude
LFLAGS := -lm

SRC := $(wildcard src/*.c) $(wildcard src/**/*.c)
VPATH := $(dir $(SRC))

OBJ := $(patsubst src/%,obj/%,$(SRC:%.c=%.o))

DEP := $(OBJ:%.o=%.d)
-include $(DEP)

.DEFAULT_GOAL := build

obj/%.o: %.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@ $(IFLAGS)

build: $(OBJ)
	@mkdir -p $(dir $(BIN))
	$(CC) $(CFLAGS) $(OBJ) -o $(BIN) $(LFLAGS)

.PHONY: clean
clean:
	rm -rf obj/
	rm -f $(BIN)
