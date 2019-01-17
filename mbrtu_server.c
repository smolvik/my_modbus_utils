#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <modbus.h>

#define N_REGS 8
#define REGISTERS_ADDRESS 0

char *comname;
int boderate = 115200;
int serverid = 1;

int parse_command(int argc, char** argv)
{
  int i = 0;

  if(argc < 2) return 0;

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
									
															
				default:
					return 0;
			}
	 	}

 	}/* for(i = 1; i < argc; i++) */

  return 1;
}

int main(int argc, char *argv[])
{
    int i;
    modbus_t *ctx = 0;
    modbus_mapping_t *mb_mapping;
    uint8_t *query = 0;
    int header_length;
    int rc;
    
	parse_command(argc, argv);
    
	ctx = modbus_new_rtu(comname, boderate, 'N', 8, 1);
    modbus_set_slave(ctx, serverid);    
    modbus_set_debug(ctx, TRUE);       

    query = malloc(MODBUS_RTU_MAX_ADU_LENGTH);
	header_length = modbus_get_header_length(ctx);
	printf("%u\n", header_length);
    
	rc = modbus_connect(ctx);
	if (rc == -1) {
		fprintf(stderr, "Unable to connect %s\n", modbus_strerror(errno));
		modbus_free(ctx);
		return -1;
	}    
	    
    mb_mapping = modbus_mapping_new(0, 0, REGISTERS_ADDRESS+N_REGS, 0);

    if (mb_mapping == NULL) 
    {
        fprintf(stderr, "Failed to allocate the mapping: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return -1;
    }
    
	memset(mb_mapping->tab_registers, 0, N_REGS);
	mb_mapping->tab_registers[0] = 0x001c;
	mb_mapping->tab_registers[1] = 0x0304;
	mb_mapping->tab_registers[2] = 0x0403;
	mb_mapping->tab_registers[3] = 0x0604;
	mb_mapping->tab_registers[4] = 0x0805;
	mb_mapping->tab_registers[5] = 0x0306;
	mb_mapping->tab_registers[6] = 0x0807;
	mb_mapping->tab_registers[7] = 0x0408;
	mb_mapping->tab_registers[8] = 0x0309;
	mb_mapping->tab_registers[9] = 0x020a;
	
	printf("------------------------\n");
	printf("%u\n", mb_mapping->nb_registers);
	for(i = 0; i < N_REGS; i++)
	{
		printf("reg[%u]: %04x\n", i, mb_mapping->tab_registers[i]);
	}
	printf("------------------------\n");
	
    while(1)
    {
        rc = modbus_receive(ctx, query);
        if (rc == -1) {
            // Connection closed by the client or error
            break;
        }

        rc = modbus_reply(ctx, query, rc, mb_mapping);
        if (rc == -1) {
            break;
        }
        
		printf("------------------------\n");
		for(i = 0; i < N_REGS; i++)
		{
			printf("reg[%u]: %04x\n", i, mb_mapping->tab_registers[i]);
		}
		printf("------------------------\n");        
    }	   

    return 0;
}
