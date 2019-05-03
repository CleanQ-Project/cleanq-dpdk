all: clean objs udp_bench

C:=g++
MAKEDEPEND:=makedepend -Y


INC_DIR = -I../include/
CFLAGS = -g -Wall -std=c++11 -Wno-literal-suffix $(INC_DIR) -O3
CFLAGS += -D_GNU_SOURCE 
CLIBS = -lpthread

LIB =
%.o:: %.cc
	$(CC) $(CPPFLAGS)  -o $@ -c $<

%.o:: %.c
	$(C) $(CFLAGS) $(LIB) $(INC_DIR) -o $@ -c $<

c_FILES=\
udp_bench.c\

H_FILES := $(C_FILES:%.C=%.H)

h_FILES := $(c_FILES:%.c=%.h)

OBJ_FILES:=$(C_FILES:%.cc=%.o) $(c_FILES:%.c=%.o)

objs: $(OBJ_FILES)

clean:
	-rm -f *.o
	-rm -f *~; rm udp_bench

clobber:
	-rm -f *.o
	-rm -f *~

udp_bench: ;$(C) $(CFLAGS) $(LIB) $(INC_DIR) $(c_FILES) -o udp_bench $(CLIBS)

depend:
	$(MAKEDEPEND) $(INCS) $(SINCS) $(C_FILES)

# DO NOT DELETE THIS LINE -- make depend depends on it.
