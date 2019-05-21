#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>

#define PING_INTERVAL 200
#define PORT "31337"
#define SIZE 16

bool do_exit = false;

void show_usage(char* bin_name)
{
	fprintf(stderr, "USAGE: %s [-c IP-ADDRESS] [-p PORT] [-i TIMEOUT] [-s PACKET_SIZE]\n", bin_name);
	fprintf(stderr, "-c: Client mode, IP-ADDRESS is address of device running %s in server mode\n", bin_name);
}

void doshutdown(int signal)
{
	do_exit = true;
}

int main(int argc, char** argv)
{
	struct sockaddr_storage *client_addr, nat_addr, *server_addr;
	struct addr_len;
	int sock, err = 0, interval = PING_INTERVAL;
	char* port = PORT;
	struct timeval timeout;
	size_t packetsize = SIZE;
	unsigned char* buffer;
	unsigned long seq = 0, loss_cnt = 0, seq_net, seq_recv;
	ssize_t len;
	char opt;
	socklen_t slen;
	char* client = NULL;
	struct addrinfo* client_addrinfo = NULL;
	size_t client_addr_len;
	struct addrinfo* server_addrinfo = NULL;
	size_t server_addr_len;
	char* listen_addr = "::";
	char host_tmp[NI_MAXHOST] = { 0 }, port_tmp[NI_MAXSERV] = { 0 };
	const struct addrinfo hints_v4 = { .ai_family = AF_INET };
	const struct addrinfo hints_v6 = { .ai_family = AF_INET6 };
	const struct addrinfo* hint = NULL;

	while((opt = getopt(argc, argv, "46c:l:p:i:s:h")) != -1)
	{
		switch(opt)
		{
			case '4':
				hint = &hints_v4;
				break;
			case '6':
				hint = &hints_v6;
				break;
			case 'c':
				client = optarg;
				break;
			case 'l':
				listen_addr = optarg;
				break;
			case 'p':
				port = optarg;
				break;
			case 'i':
				interval = atoi(optarg);
				break;
			case 's':
				packetsize = atoi(optarg);
				break;
			case 'h':
				show_usage(argv[0]);
				goto exit_err;
			default:
				fprintf(stderr, "Invalid parameter %c\n", opt);
				show_usage(argv[0]);
				err = -EINVAL;
				goto exit_err;
		}
	}

	// Parse client endpoint
	if(client) {
		if((err = getaddrinfo(client, port, hint, &client_addrinfo))) {
			fprintf(stderr, "Failed to parse client endpoint [%s]:%s, %s\n", client, port, gai_strerror(err));
			goto exit_err;
		}
		client_addr = (struct sockaddr_storage*)client_addrinfo->ai_addr;
		client_addr_len = client_addrinfo->ai_addrlen;
	}

	// Init packet buffer
	buffer = malloc(packetsize);
	if(!buffer)
	{
		fprintf(stderr, "Failed to allocate packet buffer\n");
		err = -ENOMEM;
		goto exit_addr;
	}
	memset(buffer, 0x55, packetsize);

	// Setup server socket
	if((err = getaddrinfo(listen_addr, port, NULL, &server_addrinfo))) {
			fprintf(stderr, "Failed to parse server endpoint [%s]:%s, %s\n", listen_addr, port, gai_strerror(err));
			goto exit_buffer;
	}
	server_addr = (struct sockaddr_storage*)server_addrinfo->ai_addr;
	server_addr_len = server_addrinfo->ai_addrlen;

	if((sock = socket(server_addr->ss_family, SOCK_DGRAM, 0)) < 0)
	{
		fprintf(stderr, "Failed to set up server socket, %s(%d)\n", strerror(errno), errno);
		err = -errno;
		goto exit_buffer;
	}
	if(bind(sock, (struct sockaddr*)server_addr, server_addr_len))
	{
		fprintf(stderr, "Failed to bind server socket, %s(%d)\n", strerror(errno), errno);
		err = -errno;
		goto exit_sock;
	}

	timeout.tv_sec = 0;
	timeout.tv_usec = interval * 1000;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

	// Set signal handler
	if(signal(SIGINT, doshutdown))
	{
		fprintf(stderr, "Failed to bind SIGINT\n");
		err = errno;
		goto exit_sock;
	}

	while(!do_exit)
	{
		if(client)
		{
			seq_net = htonl(seq);
			if(packetsize < sizeof(seq_net))
			{
				fprintf(stderr, "Packet size %zd < %zd, packet too short\n", len, sizeof(seq_net));
				err = -EINVAL;
				goto exit_sock;
			}
			memcpy(buffer, &seq_net, sizeof(seq_net));
			assert(!getnameinfo((struct sockaddr*)client_addr, client_addr_len, host_tmp, NI_MAXHOST, port_tmp, NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV));
			printf("Sending to %s:%s, buffer size=%zd\n", host_tmp, port_tmp, packetsize);
			if((len = sendto(sock, buffer, packetsize, 0, (struct sockaddr*)client_addr, client_addr_len)) < packetsize)
			{
				if(len < 0)
				{
					fprintf(stderr, "Failed to send data, %s(%d)\n", strerror(errno), errno);
					err = -errno;
					goto exit_sock;
				}
				printf("Only %zd of %zd bytes sent\n", len, packetsize);
			}
recv_again:
			if((len = recvfrom(sock, buffer, packetsize, 0, NULL, NULL)) < 0)
			{
				if(errno == EAGAIN)
				{
					printf("Packet %lu lost\n", seq);
					loss_cnt++;
				}
				printf("Failed to recv data, %s(%d)\n", strerror(errno), errno);
			}
			else
			{
				if(len < sizeof(seq_net))
				{
					printf("Packet length %zd < %zd, packet too short\n", len, sizeof(seq_net));
					goto recv_again;
				}
				memcpy(&seq_net, buffer, sizeof(seq_net));
				seq_recv = ntohl(seq_net);
				if(seq_recv != seq)
				{
					printf("Received data for unexpected seq num %lu\n", seq);
					goto recv_again;
				}
				printf("Received data for seq=%lu\n", seq_recv);
			}
			seq++;
		}
		else
		{
			slen = sizeof(struct sockaddr_storage);
			if((len = recvfrom(sock, buffer, packetsize, 0, (struct sockaddr*) &nat_addr, &slen)) < 0)
			{
				if(errno != EAGAIN)
					printf("Failed to recv data, %s(%d)\n", strerror(errno), errno);
			}
			else
			{
				assert(!getnameinfo((struct sockaddr*)&nat_addr, slen, host_tmp, NI_MAXHOST, port_tmp, NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV));
				memcpy(&seq_net, buffer, sizeof(seq_net));
				seq_recv = ntohl(seq_net);
				printf("Received data from %s for seq=%lu, port=%s\n", host_tmp, seq_recv, port_tmp);
				if((len = sendto(sock, buffer, packetsize, 0, (struct sockaddr*) &nat_addr, slen)) < packetsize)
				{
					if(len < 0)
					{
						printf("Failed to send data, %s(%d)\n", strerror(errno), errno);
						err = -errno;
						goto exit_sock;
					}
					printf("Only %zd of %zd bytes sent\n", len, packetsize);
				}
			}
		}
	}

	if(client)
	{
		// Print statistics
		printf("Packets sent: %lu\n", seq);
		printf("Packets lost: %lu\n", loss_cnt);
		printf("Loss percentage: %.5f%%\n", (double)loss_cnt / seq * 100);
	}

exit_sock:
	close(sock);
exit_buffer:
	free(buffer);
exit_addr:
	if(client_addrinfo) {
		freeaddrinfo(client_addrinfo);
	}
exit_err:
	if(server_addrinfo) {
		freeaddrinfo(server_addrinfo);
	}
	return err;
}
