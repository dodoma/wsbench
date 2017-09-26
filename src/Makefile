LOCALBASE = ./
include $(LOCALBASE)Make.env
include $(BASEDIR)Make.env

SOURCES = $(wildcard *.c)
OBJS = $(patsubst %.c, %.o, $(SOURCES))
DEPEND = .depend

INCS = $(INCBASE) $(INC_LOCAL)
LIBS = $(LIBBASE) $(LIB_LOCAL)

all: wsbench

$(DEPEND): $(SOURCES)
	@$(CC) $(CFLAGS) $(INCS) -MM $^ > $@

-include $(DEPEND)
%.o: %.c
	$(CC) $(CFLAGS) $(INCS) -c $< -o $@

wsbench: $(OBJS)
	$(CC) $^ -o $@ $(LIBS)

clean:
	@rm -rf $(OBJS) wsbench
