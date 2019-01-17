CC=gcc
AS=as
STRIP=strip


CFLAGS+=-Wall -O2 -I/usr/include/modbus
LDFLAGS+=-lmodbus 

all: mbrtu_server mbrtu_client mbtcp_server mbtcp_client mbrtutcp_bridge

mbtcp_server: mbtcp_server.c
	$(CC) $(CFLAGS) -o $@ mbtcp_server.c $(LDFLAGS) 
	$(STRIP) $@

mbtcp_client: mbtcp_client.c
	$(CC) $(CFLAGS) -o $@ mbtcp_client.c $(LDFLAGS) 
	$(STRIP) $@

mbrtu_server: mbrtu_server.c
	$(CC) $(CFLAGS) -o $@ mbrtu_server.c $(LDFLAGS) 
	$(STRIP) $@
	
mbrtu_client: mbrtu_client.c
	$(CC) $(CFLAGS) -o $@ mbrtu_client.c $(LDFLAGS) 
	$(STRIP) $@	
	
mbrtutcp_bridge: mbrtutcp_bridge.c
	$(CC) $(CFLAGS) -o $@ mbrtutcp_bridge.c $(LDFLAGS) 
	$(STRIP) $@	

clean:
	rm -f *.o mbrtu_server mbrtu_client mbtcp_server mbtcp_client mbrtutcp_bridge

