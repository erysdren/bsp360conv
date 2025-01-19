
RM?=rm -f
PKGCONFIG?=pkg-config
PKGS?=sdl3

override CFLAGS+=$(shell $(PKGCONFIG) --cflags $(PKGS)) -g3
override LDFLAGS+=$(shell $(PKGCONFIG) --libs $(PKGS)) -llz

BINEXT?=
OBJEXT?=.o

EXEC?=bsp360conv$(BINEXT)
OBJS=bsp360conv$(OBJEXT)

all: $(EXEC)

clean:
	$(RM) $(EXEC) $(OBJS)

$(EXEC): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
