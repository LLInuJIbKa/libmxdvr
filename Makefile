#CC=
TARGET_SHARED_OBJECT:=libmxdvr.so 
CFLAGS:=-pipe -O2 -fPIC
SOURCE_FILES:=v4l2dev.c
OBJECTS:=$(subst .c,.o, $(SOURCE_FILES))

all: $(TARGET_SHARED_OBJECT)

clean:
	rm -f $(TARGET_SHARED_OBJECT) *.o

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

$(TARGET_SHARED_OBJECT): $(OBJECTS)
	$(CC) $^ -o $@ -shared
