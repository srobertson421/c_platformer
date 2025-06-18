CC = gcc
CFLAGS = -Wall -Wextra -std=c11 `sdl2-config --cflags`

# Platform-specific libraries
ifeq ($(OS),Windows_NT)
    LDFLAGS = `sdl2-config --libs` -lSDL2_image -lchipmunk -ldbghelp -limagehlp -lm
else
    LDFLAGS = `sdl2-config --libs` -lSDL2_image -lchipmunk -lm
endif

TARGET = platformer
SRC = main.c logging.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run