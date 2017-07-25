CC := qcc

INCLUDE := -I$(QNX_TARGET)/usr/include/qt4/QtOpenGL
INCLUDE += -I$(QNX_TARGET)/usr/include/freetype2
INCLUDE += -I$(QNX_TARGET)/usr/include
INCLUDE += -I./external/include

# BB10 libraries
LIBPATHS	:= -L$(QNX_TARGET)/armle-v7/lib
LIBS    	:= -lbps -licui18n -licuuc -lscreen -lm -lfreetype -lconfig -lclipboard

# OpenGL libraries
LIBPATHS	+= -L$(QNX_TARGET)/armle-v7/usr/lib
LIBS    	+= -lGLESv1_CM -lGLESv2 -lEGL

# SDL and related
LIBPATHS	+= -L./external/lib
LIBS    	+= -lconfig -lSDL12 -lTouchControlOverlay

# change these as needed (debug right now)
CFLAGS 	:= $(INCLUDE) -V4.6.3,gcc_ntoarmv7le -O2 -g -DDEBUG
LDFLAGS	:= $(LIBPATHS) $(LIBS)

ASSET 	:= Device-Debug
BINARY	:= Term48-dev
BINARY_PATH := $(ASSET)/$(BINARY)

SRCS  	:= $(wildcard src/*.c)
OBJS  	:= $(SRCS:.c=.o )

include ./signing/bbpass

.PHONY: all clean package-debug deploy launch-debug

all: package-debug

$(BINARY): $(OBJS)
	mkdir -p $(ASSET)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $(BINARY_PATH)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -fv src/*.o
	@rm -fv $(BINARY_PATH)
	@rmdir -v $(ASSET)
	@rm -fv $(BINARY).bar

signing/debugtoken.bar:
	$(error Debug token error: place debug token in signing/debugtoken.bar or see signing/Makefile))
package-debug: $(BINARY) signing/debugtoken.bar
	blackberry-nativepackager -package $(BINARY).bar bar-descriptor.xml -devMode -debugToken signing/debugtoken.bar

signing/ssh-key:
	$(error SSH key error: signing/ssh-key not found. `cd signing` and `make ssh-key`))
connect: signing/ssh-key
	blackberry-connect $(BBIP) -password $(BBPASS) -sshPublicKey signing/ssh-key.pub

BBIP ?= 169.254.0.1

deploy: package-debug
	blackberry-deploy -installApp $(BBIP) -password $(BBPASS) $(BINARY).bar

launch-debug: deploy
	blackberry-deploy -debugNative -device $(BBIP) -password $(BBPASS) -launchApp $(BINARY).bar
	trap '' SIGINT; BINARY_PATH=$(BINARY_PATH) BBIP=$(BBIP) ntoarm-gdb -x scripts/gdb-debug-setup.py
