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
CFLAGS 	:= $(INCLUDE) -V4.6.3,gcc_ntoarmv7le -O2 -g
LDFLAGS	:= $(LIBPATHS) $(LIBS)

ASSET 	:= Device-Debug
BINARY	:= Term48-dev

SRCS  	:= $(wildcard src/*.c)
OBJS  	:= $(SRCS:.c=.o )

include ./signing/bbpass

all: debug

$(BINARY): $(OBJS)
	mkdir -p $(ASSET)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $(ASSET)/$(BINARY)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -fv src/*.o
	@rm -fv $(ASSET)/$(BINARY)

signing/debugtoken.bar:
	$(error Debug token error: place debug token in signing/debugtoken.bar or see signing/Makefile))
debug: $(BINARY) signing/debugtoken.bar
	blackberry-nativepackager -package $(BINARY).bar bar-descriptor.xml -devMode -debugToken signing/debugtoken.bar

BBIP ?= 169.254.0.1

deploy: debug
	blackberry-deploy -installApp $(BBIP) -password $(BBPASS) $(BINARY).bar
