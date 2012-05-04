CC=/opt/toolchain/bin/arm-none-linux-gnueabi-gcc -I/media/SmartHome_SD/usr/include
ROOTFS:=/media/SmartHome_SD
TARGET_SHARED_OBJECT:=libmxdvr.so 
CFLAGS:=-pipe -O3 -fPIC -Wall -march=armv6
LDFLAGS:=-Wall -L$(ROOTFS)/usr/lib -lipu --sysroot=$(ROOTFS)
SOURCE_FILES:=v4l2dev.c mxc_ipu.c platform.c
OBJECTS:=$(subst .c,.o, $(SOURCE_FILES))

all: $(TARGET_SHARED_OBJECT) v4l2test

clean:
	rm -f $(TARGET_SHARED_OBJECT) *.o v4l2test

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

$(TARGET_SHARED_OBJECT): $(OBJECTS)
	$(CC) $^ -o $@ -shared $(LDFLAGS)

v4l2test: v4l2test.o $(TARGET_SHARED_OBJECT)
	$(CC) $^ -o $@ -L./ -lmxdvr $(LDFLAGS)

