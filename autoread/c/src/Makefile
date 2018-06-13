OBJS += tmr_strerror.o
OBJS += serial_transport_posix.o
OBJS += osdep_posix.o


HEADERS += receive_autonomous_reading.h
HEADERS += tmr_gpio.h
HEADERS += tmr_serial_transport.h
HEADERS += tmr_status.h
HEADERS += tmr_tag_data.h
HEADERS += tmr_tag_protocol.h
HEADERS += tmr_types.h
HEADERS += tmr_utils.h
HEADERS += osdep.h

PROGS += receiveAutonomousReading

DBG ?= -g
CWARN = -Werror -Wall
CFLAGS += -I. $(DBG) $(CWARN)

# Position-independent code required for shared libraries
CFLAGS += -fPIC

all:$(PROGS)

receiveAutonomousReading.o: $(HEADERS) $(OBJS)
receiveAutonomousReading:	receiveAutonomousReading.o $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread -g

.PHONY: clean                                                                                                                                                          
clean:
	  rm -f  $(PROGS) *.o  core tests/*.output

