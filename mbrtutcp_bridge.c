#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <modbus.h>
#include <sys/param.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/resource.h>


#define N_REGS 1024
#define N_REGSWR 1024

char *serverip;
int port = 1502;
char *comname;
int boderate = 115200;
int res_timeout_ms = 500;
int amidemon = 1;

static int parse_command(int argc, char** argv);
static int rtu_redirect(modbus_t *masterctx, int rc, int hlen, unsigned char *query);

int main(int argc, char*argv[])
{
    int server_socket;
    int master_socket;
    modbus_t *ctx;
    modbus_mapping_t *mb_mapping;    
    unsigned char *query = 0;
    int header_length; 
    fd_set refset;
    fd_set rdset;    
    /* Maximum file descriptor number */
    int fdmax;  
    int rc;  
    int fd0, fd1, fd2;
    int i;
    struct rlimit flim; 
	
	if(!parse_command(argc, argv))
	{
		fprintf(stderr, "Error parsing comline\n");
		exit(1);
	}	
	
	if(amidemon)
	{
		// creating demon
		signal(SIGTTOU, SIG_IGN);
		signal(SIGTTIN, SIG_IGN);
		signal(SIGTSTP, SIG_IGN);

		if(fork() != 0)
		{
		  exit(0);
		}

		setsid();

		// close all handles
		getrlimit(RLIMIT_NOFILE, &flim);
		for(i = 0; i < flim.rlim_max; i ++)
		{
		  close(i);
		}

		chdir("/");

		// descriptors num 0, 1, 2 is /dev/null
		fd0 = open("/dev/null", O_RDWR);
		fd1 = dup(0);
		fd2 = dup(0);

		openlog("mbrtutcp_bridge", LOG_CONS, LOG_DAEMON);
		syslog(LOG_INFO,"Starting");
		if(fd0 != 0 || fd1 != 1 || fd2 != 2)
		{
		  syslog(LOG_ERR, "error descriptors");
		  exit(1);
		}

		/* I am the demon */
	}


    ctx = modbus_new_tcp(serverip, port);
    //modbus_set_debug(ctx, TRUE);
    query = malloc(MODBUS_TCP_MAX_ADU_LENGTH);
    header_length = modbus_get_header_length(ctx);

    server_socket = modbus_tcp_listen(ctx, 4);

    FD_ZERO(&refset);
    FD_SET(server_socket, &refset);
    fdmax = server_socket;

    while(1)
    {
        rdset = refset;
        if (select(fdmax+1, &rdset, NULL, NULL, NULL) == -1) 
        {
			if(amidemon){
				syslog(LOG_ERR, "Server select() failure.");
			}
			else{
				perror("Server select() failure.");
			}
            
            exit(1);
        }

        /* Run through the existing connections looking for data to be
         * read */
        for (master_socket = 0; master_socket <= fdmax; master_socket++) 
        {
            if (FD_ISSET(master_socket, &rdset)) 
            {
                if (master_socket == server_socket) 
                {
                    /* A client is asking a new connection */
                    socklen_t addrlen;
                    struct sockaddr_in clientaddr;
                    int newfd;

                    /* Handle new connections */
                    addrlen = sizeof(clientaddr);
                    memset(&clientaddr, 0, sizeof(clientaddr));
                    newfd = accept(server_socket, (struct sockaddr *)&clientaddr, &addrlen);
                    if (newfd == -1) 
                    {
						if(amidemon){
							syslog(LOG_ERR, "Server accept() error");
						}
						else{
							perror("Server accept() error");
						}
            
                    } else 
                    {
                        FD_SET(newfd, &refset);

                        if (newfd > fdmax) 
                        {
                            /* Keep track of the maximum */
                            fdmax = newfd;
                        }
                        if(0 == amidemon){
							printf("New connection from %s:%d on socket %d\n",
                               inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port, newfd);
						   }
                    }
                } else
                {
                    /* An already connected master has sent a new query */
                    uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
                    int hlen;

					//printf("An already connected master has sent a new query\n");

                    modbus_set_socket(ctx, master_socket);
                    
					hlen = modbus_get_header_length(ctx);                                        
                    rc = modbus_receive(ctx, query);
                    
                    if (rc != -1) 
                    {
						// we'v got query ...
						// redirect to rtu server
						rtu_redirect(ctx, rc, hlen, query);
						
                    } else 
                    {
						if(0 == amidemon){
							// Connection closed by the client, end of server 
							printf("Connection closed on socket %d\n", master_socket);
						}
                        close(master_socket);

                        // Remove from reference set 
                        FD_CLR(master_socket, &refset);

                        if (master_socket == fdmax) 
                        {
                            fdmax--;
                        }
                    }
                }
            }
        }
    }

	if(amidemon){
		syslog(LOG_ERR, "Quit the loop: %s\n", modbus_strerror(errno));		
	}
	else{
		printf("Quit the loop: %s\n", modbus_strerror(errno));
	}

    modbus_mapping_free(mb_mapping);
    free(query);
    modbus_free(ctx);

    return 0;
}

int rtu_redirect(modbus_t *tcp_ctx, int qlen, int hlen, unsigned char *query)
{
	int id_server;
	int nfunc;
    modbus_t *ctx;
    int i;
    modbus_mapping_t *mb_mapping;
	struct timeval old_response_timeout;
	struct timeval response_timeout;    
	
	if(hlen <= 0) {
		return 0;
	}		
	
	id_server = query[hlen-1];
	nfunc = query[hlen];
	
    ctx = modbus_new_rtu(comname, boderate, 'N', 8, 1);
    if (ctx == NULL) 
    {
		if(amidemon){
			syslog(LOG_ERR, "Unable to allocate libmodbus context\n");		
		}
		else{
			fprintf(stderr, "Unable to allocate libmodbus context\n");
		}		
        return 0;
    }
    //modbus_set_debug(ctx, TRUE);
    modbus_set_slave(ctx, id_server);
    if (modbus_connect(ctx) == -1) 
    {
		if(amidemon){
			syslog(LOG_ERR, "Connection failed: %s\n", modbus_strerror(errno));		
		}
		else{
			fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
		}		
        
        modbus_free(ctx);
        return 0;
    }    
    
	// Save original timeout
	//modbus_get_response_timeout(ctx, &old_response_timeout);

	// Define a new and too short timeout!
	response_timeout.tv_sec = 0;
	response_timeout.tv_usec = res_timeout_ms * 1000;
	modbus_set_response_timeout(ctx, &response_timeout);        

    // create registers set
    mb_mapping = modbus_mapping_new(N_REGS, 0,N_REGS, 0);
	
	/*modbus_send_raw_request(ctx, &query[6], qlen-6);
	i = modbus_receive_confirmation(ctx, &query[6]);
	modbus_send_raw_request(tcp_ctx, query, i+4);*/

	switch(nfunc)
	{	
		case 0x01:
			// read from some coil registers
			{
				uint16_t reg_addr;
				uint16_t n_regs;
				
				((uint8_t*)(&reg_addr))[1]= query[hlen+1];
				((uint8_t*)(&reg_addr))[0]= query[hlen+2];
				((uint8_t*)(&n_regs))[1]= query[hlen+3];
				((uint8_t*)(&n_regs))[0]= query[hlen+4];
				
				if(n_regs == modbus_read_bits(ctx, reg_addr, n_regs, mb_mapping->tab_bits+reg_addr))
				{
					modbus_reply(tcp_ctx, query, qlen, mb_mapping);
				}
				//for(i=0; i < N_REGS; i++){
				//printf("%d:%d\n",i, mb_mapping->tab_registers[i]);
				//}
								
			}
							
			break;		
		
		case 0x03:
			// read from some holding registers
			{
				uint16_t reg_addr;
				uint16_t n_regs;
				
				((uint8_t*)(&reg_addr))[1]= query[hlen+1];
				((uint8_t*)(&reg_addr))[0]= query[hlen+2];
				((uint8_t*)(&n_regs))[1]= query[hlen+3];
				((uint8_t*)(&n_regs))[0]= query[hlen+4];
				
				if(n_regs == modbus_read_registers(ctx, reg_addr, n_regs, mb_mapping->tab_registers+reg_addr))
				{
					modbus_reply(tcp_ctx, query, qlen, mb_mapping);
				}
				//for(i=0; i < N_REGS; i++){
				//printf("%d:%d\n",i, mb_mapping->tab_registers[i]);
				//}
								
			}
							
			break;

		case 0x0f:
			// write to some coil registers
			{
				uint16_t reg_addr;
				uint16_t n_regs;
				uint8_t regs[N_REGSWR];
				int i;
				int j = hlen+1;
				int nb;
				int k;

				((uint8_t*)(&reg_addr))[1]= query[j++];
				((uint8_t*)(&reg_addr))[0]= query[j++];
				((uint8_t*)(&n_regs))[1]= query[j++];
				((uint8_t*)(&n_regs))[0]= query[j++];
				nb = query[j++];
				
				k = 0;
				
				while(nb--){
					int bc = query[j++];
					for(i = 0; (i < 8) &&  (k < n_regs); i ++)
					{
						regs[k++] = bc & 0x01;
						bc = bc >> 1;
					}
				}				

				printf("k=%d\n", k);
				for(i = 0; i < k; i++){
					printf("%d:", regs[i]);
				}
				printf("\n");
				
				/*regs[0] = 0x0101;
				regs[1] = 0x0101;
				regs[2] = 0x0101;
				regs[3] = 0x0101;
				*/

				if(n_regs == modbus_write_bits(ctx, reg_addr, n_regs, regs))
				{
					memcpy(&(mb_mapping->tab_bits[N_REGS-N_REGSWR]), regs, N_REGSWR);
					modbus_reply(tcp_ctx, query, qlen, mb_mapping);
				}
			}
			break;

		case 0x10:
			// write to some holding registers
			{
				uint16_t reg_addr;
				uint16_t n_regs;
				uint16_t regs[N_REGSWR];
				int i;
				int j = hlen+1;
				int nb;
				
				((uint8_t*)(&reg_addr))[1]= query[j++];
				((uint8_t*)(&reg_addr))[0]= query[j++];
				((uint8_t*)(&n_regs))[1]= query[j++];
				((uint8_t*)(&n_regs))[0]= query[j++];
				nb = query[j++];
							
				for(i = 0; (i < nb) && (i < N_REGSWR); i ++)
				{
					((uint8_t*)(&regs[i]))[1]= query[j++];
					((uint8_t*)(&regs[i]))[0]= query[j++];					
				}	
				
				if(n_regs == modbus_write_registers(ctx, reg_addr, n_regs, regs))
				{
					memcpy(&(mb_mapping->tab_registers[N_REGS-N_REGSWR]), regs, N_REGSWR);
					modbus_reply(tcp_ctx, query, qlen, mb_mapping);
				}
			}
			break;
			
		case 0x06:
			// write to one holding register
			{
				uint16_t reg_addr;
				uint16_t reg_data;
				
				((uint8_t*)(&reg_addr))[1]= query[hlen+1];
				((uint8_t*)(&reg_addr))[0]= query[hlen+2];
				((uint8_t*)(&reg_data))[1]= query[hlen+3];
				((uint8_t*)(&reg_data))[0]= query[hlen+4];
				
				if(1 == modbus_write_register(ctx, reg_addr, reg_data))
				{
					mb_mapping->tab_registers[reg_addr] = reg_data;
					modbus_reply(tcp_ctx, query, qlen, mb_mapping);
				}
				
			}			
			break;						
					
		case 0x05:
			// write to one coil register
			{
				uint16_t reg_addr;
				uint16_t reg_data;
				
				((uint8_t*)(&reg_addr))[1]= query[hlen+1];
				((uint8_t*)(&reg_addr))[0]= query[hlen+2];
				((uint8_t*)(&reg_data))[1]= query[hlen+3];
				((uint8_t*)(&reg_data))[0]= query[hlen+4];
				
				if(1 == modbus_write_bit(ctx, reg_addr, reg_data))
				{
					mb_mapping->tab_bits[reg_addr] = reg_data;
					modbus_reply(tcp_ctx, query, qlen, mb_mapping);
				}
				
			}			
			break;
	}	

	modbus_mapping_free(mb_mapping);
	modbus_close(ctx);
	modbus_free(ctx);
    return 0;	
}

int parse_command(int argc, char** argv)
{
  int i = 0;

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
									
				case 'i':
					serverip = argv[i]+2;
					break;
					
				case 'p':
					port = atoi(&argv[i][2]);
					break;	
					
				case 't':
					res_timeout_ms = atoi(&argv[i][2]);
					break;			
					
				case 'f':
					amidemon = 0;
					break;
															
				default:
					return 0;
			}
	 	}

 	}/* for(i = 1; i < argc; i++) */

  return 1;
}
