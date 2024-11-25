BIN := bin/prototype

CC := clang
CFLAGS := -std=c99 -Wall -Wextra -ggdb -MD -MP
IFLAGS := -Iinclude -Isrc -Ilibs/ds/include -Ilibs/glad/include -Ilibs/freetype/include -Ilibs/stb/
LFLAGS := -lm libs/ds/ds.o -lglfw libs/glad/glad.o -Llibs/freetype -lfreetype libs/stb/stb.o

SRC := $(wildcard src/*.c) $(wildcard src/**/*.c)
VPATH := $(dir $(SRC))

OBJ := $(patsubst src/%,obj/%,$(SRC:%.c=%.o))

DEP := $(OBJ:%.o=%.d)
-include $(DEP)

.DEFAULT_GOAL := build

obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@ $(IFLAGS)

build: libs $(OBJ)
	@mkdir -p $(dir $(BIN))
	$(CC) $(CFLAGS) $(OBJ) -o $(BIN) $(LFLAGS)

libs: libs/ds/ds.o libs/glad/glad.o

libs/ds/ds.o: libs/ds/src/ds.c
	$(CC) $(CFLAGS) -c libs/ds/src/ds.c -o libs/ds/ds.o $(IFLAGS)

libs/glad/glad.o: libs/glad/src/gl.c
	$(CC) -O3 -c libs/glad/src/gl.c -o libs/glad/glad.o -Ilibs/glad/include

libs/freetype/build/libfreetype.a:
	cd libs/freetype && cmake -B build && cmake --build build

libs/stb/stb.o: libs/stb/stb.c
	$(CC) -O3 -c libs/stb/stb.c -o libs/stb/stb.o -Ilibs/stb

libs: libs/ds/ds.o libs/freetype/build/libfreetype.a libs/stb/stb.o

.PHONY: clean
clean:
	rm -rf obj/
	rm -f $(BIN)
