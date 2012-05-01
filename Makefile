#CC=
TARGET_SHARED_OBJECT:=libmxdvr.so 
CFLAGS:=-pipe -g -fPIC -Wall
SOURCE_FILES:=v4l2dev.c
OBJECTS:=$(subst .c,.o, $(SOURCE_FILES))

all: $(TARGET_SHARED_OBJECT) v4l2test

clean:
	rm -f $(TARGET_SHARED_OBJECT) *.o

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

$(TARGET_SHARED_OBJECT): $(OBJECTS)
	$(CC) $^ -o $@ -shared

v4l2test: v4l2test.o $(TARGET_SHARED_OBJECT)
	$(CC) $^ -o $@ -L./ -lmxdvr
