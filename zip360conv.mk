
RM?=rm -f
PKGCONFIG?=pkg-config
PKGS?=sdl3

override CFLAGS+=$(shell $(PKGCONFIG) --cflags $(PKGS)) -g3
override LDFLAGS+=$(shell $(PKGCONFIG) --libs $(PKGS)) -llzma

BINEXT?=
OBJEXT?=.o

EXEC?=zip360conv$(BINEXT)
OBJS=zip360conv$(OBJEXT) decompress_lzma$(OBJEXT) utils$(OBJEXT)

all: $(EXEC)

clean:
	$(RM) $(EXEC) $(OBJS)

$(EXEC): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
