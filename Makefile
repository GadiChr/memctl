SOURCES = memctl.c
TARGET = memctl
CFLAGS = -Wall
LDFLAGS =

OBJS = $(SOURCES:.c=.o)

all: $(TARGET)

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean

