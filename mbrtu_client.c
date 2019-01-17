#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "modbus.h"

#define N_REGS 8

char *comname;
uint16_t tab_regs[N_REGS];
int boderate = 115200;
int serverid = 1;
int nregfwr = 0;
int res_timeout_ms = 500;
int tab_offset = 0;

static int parse_command(int argc, char** argv)
{
  int i = 0;
  int j = 0;

  //if(argc < 5) return 0;

  for(i = 1; i < argc; i++)
  {
	  if(argv[i][0]=='-')
	  {
			switch(argv[i][1])
			{
				case 'd':
					comname = argv[i]+2;
					break;
					
				case 'b':
					boderate = atoi(&argv[i][2]);
					break;	
					
				case 'm':
					serverid = atoi(&argv[i][2]);
					break;										
					
				case 't':
					res_timeout_ms = atoi(&argv[i][2]);
					break;															
					
				case 's':
					tab_offset = atoi(&argv[i][2]);
					j = tab_offset;
					break;																				
										
				default:
					return 0;
	  	}
	 	}
	 	else{
			if(j < N_REGS){ 	 	
				tab_regs[j++] = (uint16_t)(0xffff&strtol(argv[i], 0, 16));
				nregfwr++;
			}
			else{
				break;
			}
	 	}
 	}/* for(i = 1; i < argc; i++) */

  return 1;
}

int main(int argc, char *argv[])
{
    modbus_t *ctx;
    int i, rc;
	struct timeval old_response_timeout;
	struct timeval response_timeout;

	if(!parse_command(argc, argv))
	{
		fprintf(stderr, "Error parsing comline\n");
		return -1;
	}
	
	/*for(i =0 ; i < N_REGS; i++)
	{
		printf("%u\n", tab_regs[i]);
	}
	printf("%u\n", boderate);
	printf("%s\n", comname);
	return 0;*/

	// some init ..
    ctx = modbus_new_rtu(comname, boderate, 'N', 8, 1);
    if (ctx == NULL) 
    {
        fprintf(stderr, "Unable to allocate libmodbus context\n");
        return -1;
    }
    //modbus_set_debug(ctx, TRUE);
    modbus_set_slave(ctx, serverid);

    if (modbus_connect(ctx) == -1) 
    {
        fprintf(stderr, "Connection failed: %s\n",
                modbus_strerror(errno));
        modbus_free(ctx);
        return -1;
    }
    
    // set timeout
    //modbus_get_response_timeout(ctx, &byte_timeout);
    //printf("%u %u\n", byte_timeout.tv_sec, byte_timeout.tv_usec);
    
	// Save original timeout
	modbus_get_response_timeout(ctx, &old_response_timeout);

	// Define a new and too short timeout!
	response_timeout.tv_sec = 0;
	response_timeout.tv_usec = res_timeout_ms * 1000;
	modbus_set_response_timeout(ctx, &response_timeout);    
    

	// lets do it
	if(nregfwr > 0)
	{	 
		rc = modbus_write_registers(ctx, tab_offset, nregfwr, &tab_regs[tab_offset]);
		if (rc == nregfwr) {
			//printf("OK\n");
		} else {
			//printf("FAILED\n");
			goto close;
		}
	}

    rc = modbus_read_registers(ctx, 0, N_REGS, tab_regs);
    if (rc != N_REGS) {
        //printf("FAILED (nb points %d)\n", rc);
        goto close;
    }

	for(i = 0; i < N_REGS-1; i++)
	{
		printf("0x%04x ", tab_regs[i]);
	}
	printf("0x%04x", tab_regs[N_REGS-1]);

close:

    /* Close the connection */
    modbus_close(ctx);
    modbus_free(ctx);

    return 0;
}
