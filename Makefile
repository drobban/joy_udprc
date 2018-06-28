LIBS = "-lusbhid"
CFLAGS = -std=c99 -pedantic -Werror

PROGRAM = joy

OBJS	= debug.o hid.o main.o com.o

all: $(PROGRAM)

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(PROGRAM) : $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $^ $(LIBS)
# $(CC) $(CFLAGS) $(OBJS) /usr/src/lib/libusbhid/libusbhid.a -o $@ $^ $(LIBS)

clean:
	-rm -f $(OBJS) $(PROGRAM)
