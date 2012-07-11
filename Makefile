CC=/opt/toolchain/bin/arm-none-linux-gnueabi-gcc 
ROOTFS:=/media/SmartHome_SD
TARGET_SHARED_OBJECT:=libmxdvr.so 
SOURCE_FILES:=v4l2dev.c mxc_vpu.c mxc_vpu_encoder.c mxc_vpu_decoder.c mxc_display.c platform.c font.c framebuf.c queue.c
OBJECTS:=$(subst .c,.o, $(SOURCE_FILES))


INCLUDE_DIRS=\
-I./include \
-I$(ROOTFS)/usr/include \
-I$(ROOTFS)/usr/include/cairo \
-I$(ROOTFS)/usr/include/pango-1.0 \
-I$(ROOTFS)/usr/include/glib-2.0 \
-I$(ROOTFS)/usr/lib/glib-2.0/include

LINK_LIBRARIES=-ljpeg -lipu -lvpu $(shell pkg-config --libs pangocairo)

CFLAGS:=-pipe -O3 -fPIC -Wall -march=armv6 $(INCLUDE_DIRS)
LDFLAGS:=-Wall -L$(ROOTFS)/usr/lib $(LINK_LIBRARIES) --sysroot=$(ROOTFS) 


all: $(TARGET_SHARED_OBJECT) dvrdemo

clean:
	rm -f $(TARGET_SHARED_OBJECT) *.o dvrdemo

%.o: %.c
	@echo "  CC	$@"
	@$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_SHARED_OBJECT): $(OBJECTS)
	@echo "  LD	$@"
	@$(CC) -shared $(LDFLAGS) $^ -o $@

dvrdemo: dvrdemo.o $(TARGET_SHARED_OBJECT)
	@echo "  LD	$@"
	@$(CC) -L./ $(LDFLAGS) -lmxdvr $< -o $@
	#@$(CC) -L./ $(LDFLAGS) $^ -o $@
