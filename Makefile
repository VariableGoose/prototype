BIN := bin/prototype

CC := gcc
CFLAGS := -std=c99 -Wall -Wextra -ggdb -MD -MP
IFLAGS := -Iinclude -Ilibs/ds/include
LFLAGS := -lm libs/ds/ds.o

SRC := $(wildcard src/*.c) $(wildcard src/**/*.c)
VPATH := $(dir $(SRC))

OBJ := $(patsubst src/%,obj/%,$(SRC:%.c=%.o))

DEP := $(OBJ:%.o=%.d)
-include $(DEP)

.DEFAULT_GOAL := build

obj/%.o: %.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@ $(IFLAGS)

build: libs $(OBJ)
	@mkdir -p $(dir $(BIN))
	$(CC) $(CFLAGS) $(OBJ) -o $(BIN) $(LFLAGS)

libs/ds/ds.o: libs/ds/src/ds.c
	$(CC) $(CFLAGS) -c libs/ds/src/ds.c -o libs/ds/ds.o $(IFLAGS)

libs: libs/ds/ds.o

.PHONY: clean
clean:
	rm -rf obj/
	rm -f $(BIN)
