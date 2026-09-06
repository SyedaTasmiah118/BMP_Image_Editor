CC := clang
CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -O2 -I/usr/local/include/iup

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
	# macOS: IUP links against the native Cocoa/Carbon frameworks.
	LDFLAGS := -L/usr/local/lib -liup \
		-framework Cocoa -framework Carbon -framework QuartzCore \
		-framework UserNotifications -framework SystemConfiguration
else
	# Linux: IUP links against GTK. Adjust the include/lib paths below
	# if your IUP install lives somewhere other than /usr/local.
	CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -O2 \
		-I/usr/local/include/iup $(shell pkg-config --cflags gtk+-2.0)
	LDFLAGS := -L/usr/local/lib -liup $(shell pkg-config --libs gtk+-2.0)
endif

TARGET := image_editor
SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)

.PHONY: all run test clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

# Builds and runs the IUP-independent smoke test (image I/O, algorithms,
# undo stack). Does not require IUP to be installed.
test:
	$(CC) -std=c99 -Wall -Wextra -Wpedantic -Isrc \
		tests/test_core.c src/image.c src/image_processing.c src/undo.c \
		-o tests/test_core
	./tests/test_core

clean:
	rm -f $(OBJ) $(TARGET) tests/test_core tests/test.bmp
