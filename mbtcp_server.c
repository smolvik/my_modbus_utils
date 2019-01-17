#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <modbus.h>

#define SERVER_ID  1
#define N_REGS 10

int main(int argc, char*argv[])
{
    int server_socket;
    int master_socket;
    modbus_t *ctx;
    modbus_mapping_t *mb_mapping;
    int rc;
    int i;
    unsigned char *query = 0;
    int header_length;
    fd_set refset;
    fd_set rdset;    
    /* Maximum file descriptor number */
    int fdmax;    

    ctx = modbus_new_tcp("127.0.0.1", 1502);
    modbus_set_debug(ctx, TRUE);    
    query = malloc(MODBUS_TCP_MAX_ADU_LENGTH);
    header_length = modbus_get_header_length(ctx);

    mb_mapping = modbus_mapping_new(0, 0, N_REGS, 0);

    if (mb_mapping == NULL) 
    {
        fprintf(stderr, "Failed to allocate the mapping: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return -1;
    }
    
	memset(mb_mapping->tab_registers, 0, N_REGS);
	mb_mapping->tab_registers[0] = 0x0001;
	mb_mapping->tab_registers[1] = 0x0002;
	mb_mapping->tab_registers[2] = 0x0003;
	mb_mapping->tab_registers[3] = 0x0004;
	mb_mapping->tab_registers[4] = 0x0005;
	mb_mapping->tab_registers[5] = 0x0006;
	mb_mapping->tab_registers[6] = 0x0007;
	mb_mapping->tab_registers[7] = 0x0008;
	mb_mapping->tab_registers[8] = 0x0009;
	mb_mapping->tab_registers[9] = 0x000a;
	
	printf("------------------------\n");
	printf("%u\n", mb_mapping->nb_registers);
	for(i = 0; i < N_REGS; i++)
	{
		printf("reg[%u]: %04x\n", i, mb_mapping->tab_registers[i]);
	}
	printf("------------------------\n");

    server_socket = modbus_tcp_listen(ctx, 1);

    FD_ZERO(&refset);
    FD_SET(server_socket, &refset);
    fdmax = server_socket;

    while(1)
    {
        rdset = refset;
        if (select(fdmax+1, &rdset, NULL, NULL, NULL) == -1) 
        {
            perror("Server select() failure.");
            return -1;
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
                        perror("Server accept() error");
                    } else 
                    {
                        FD_SET(newfd, &refset);

                        if (newfd > fdmax) 
                        {
                            /* Keep track of the maximum */
                            fdmax = newfd;
                        }
                        printf("New connection from %s:%d on socket %d\n",
                               inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port, newfd);
                    }
                } else 
                {
                    /* An already connected master has sent a new query */
                    uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];

                    modbus_set_socket(ctx, master_socket);
                    rc = modbus_receive(ctx, query);
                    if (rc != -1) 
                    {
                        modbus_reply(ctx, query, rc, mb_mapping);
                    } else 
                    {
                        /* Connection closed by the client, end of server */
                        printf("Connection closed on socket %d\n", master_socket);
                        close(master_socket);

                        /* Remove from reference set */
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


    printf("Quit the loop: %s\n", modbus_strerror(errno));

    modbus_mapping_free(mb_mapping);
    free(query);
    modbus_free(ctx);

    return 0;
}
