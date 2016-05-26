
CC := gcc
#CFLAGS = -g -c -Wall
CFLAGS = -g -c
LDFLAGS=

# you must always place libraries after the files you link
LINKED= -lsqlite3

EXECUTABLE = srv

# if other source dirs are added into src/
# they will be added here
SRC_DIRS := . 
SRC_DIRS := $(addprefix src/, $(SRC_DIRS) )
SOURCES := $(wildcard $(addsuffix /*.c, $(SRC_DIRS)) )
OBJECTS := $(notdir $(SOURCES) )
OBJECTS := $(OBJECTS:.c=.o)

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $@ $(LINKED)

VPATH := $(SRC_DIRS)


%.o: %.c
#	$(CC) $(CFLAGS) $<
	$(CC) $(CFLAGS) $< $(addprefix -I, $(SRC_DIRS)) -MD

include $(wildcard *.d)

.PHONY: clean

clean:
	rm -rf $(EXECUTABLE) $(OBJECTS) *.d
