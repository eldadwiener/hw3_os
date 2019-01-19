/*
 *
 *
 *  Created on: Jan 12, 2019
 *      Author: compm
 */
#include  <stdio.h>
#include <unistd.h>
#include  <sys/types.h>
#include  <sys/socket.h>
#include  <sys/time.h>
#include <sys/select.h>
#include  <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <stdbool.h>

#define PACKET_MAX_SIZE  512


static const int WAIT_FOR_PACKET_TIMEOUT = 3;
static const int NUMBER_OF_FAILURES = 7;

static const int DATA_HEADER_LEN = 4;
static const int WRQ_HEADER_LEN = 2;
static const int ACK_HEADER_LEN = 4;
static const uint16_t WRQ_OPCODE = 2;
static const uint16_t DATA_OPCODE = 3;
static const uint16_t ACK_HEADER = 4;
static const int OPCODE_LEN = 2;
static const int BLOCK_NUM_LEN = 2;

typedef struct wrq_fields s_wrq_fields;
typedef struct ack_fields s_ack_fields;
typedef struct data_fields s_data_fields;
typedef struct sockaddr_in s_sockadd_in;

struct ack_fields{
	uint16_t opcode;
	uint16_t blocknum;
}__attribute__((packed));

struct wrq_fields{
	uint16_t opcode;
	char wrq_data [PACKET_MAX_SIZE];
}__attribute__((packed));

struct data_fields{
	uint16_t opcode;
	uint16_t block_num;
	char data [PACKET_MAX_SIZE];
}__attribute__((packed));

void init_vars(s_sockadd_in*, socklen_t*  , bool* ,int* , uint16_t* ,  s_wrq_fields* );
bool data_handler(int , s_sockadd_in* , uint16_t*,	int , s_data_fields* , bool* , FILE*);
bool send_ack(int , s_sockadd_in* , uint16_t);
bool wrq_handler(int , s_wrq_fields* , FILE**);
bool run (int, FILE*);
bool init_server (int* , s_sockadd_in* ,  uint16_t );


int main(int argc,char * argv[])
{
		if (argc != 2)
		{
			printf("wrong num args\n");
			exit(1);
		}
		uint16_t port_num = htons(atoi(argv[1]));
		printf("port num is: %d\n",port_num);//DEBUG
		int sockfd;
		s_sockadd_in my_addr;
		if(!init_server ( &sockfd, &my_addr,port_num))
		{
			exit(1);
		}
		while(true)
		{
			FILE* filename = NULL;
			if (!run(sockfd, filename))
			{//error
				printf("RECVFAIL\n");
			}
			else
			{
				printf("RECVOK\n");
			}
			if ( filename != NULL)
			{
				fclose(filename);
			}
		}
		close (sockfd);
		exit(0);
}


bool init_server (int* sockfd, s_sockadd_in* my_addr,  uint16_t port)
{
		*sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (*sockfd<0)
		{
			perror("TTFTP_ERROR:1   ");
			return false;
		}
		memset(my_addr, 0, sizeof(*my_addr));
		my_addr->sin_family = AF_INET;
		my_addr->sin_addr.s_addr  =  INADDR_ANY;
		my_addr->sin_port = port;
		if (bind(*sockfd, (struct sockaddr*) my_addr , sizeof(*my_addr)) < 0)
		{
			close (*sockfd);
			perror ("TTFTP_ERROR: 2  ");
			return false;
		}
		return true;
}

bool run (int sockfd, FILE* filename)
{
	s_sockadd_in client_addr;
	socklen_t  client_addr_len;
	bool end_of_data;
	int errorsnum;
	uint16_t acknum;
	s_wrq_fields wrq_fields;
	// get WRQ
	struct timeval tv;
	fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);
	init_vars(&client_addr, &client_addr_len, &end_of_data, &errorsnum, &acknum,  & wrq_fields);
	int rcv_size = recvfrom(sockfd, &wrq_fields, PACKET_MAX_SIZE+DATA_HEADER_LEN, 0,
														(struct sockaddr*)&client_addr, (socklen_t*)&client_addr_len);
	if (rcv_size<0)
	{
		perror ("TTFTP_ERROR: 4  ");
		return false;
	}
	if (!wrq_handler(rcv_size, &wrq_fields, &filename))
	{
		return false;
	}
	if (!send_ack(sockfd, &client_addr, acknum))
	{
		return false;
	}
	do
	{
		do
		{
			s_data_fields data_fields;
			memset(&data_fields ,0,sizeof(data_fields));
			do //try to get packet
			{
				tv.tv_sec = WAIT_FOR_PACKET_TIMEOUT;
				tv.tv_usec = 0;
				int slct_val = select(sockfd+1, &rfds, NULL, NULL, &tv);
				if (slct_val > 0)// if there was something at the socket and we are here not because of a timeout
				{
					// Read the DATA packet from the socket (at  least we hope this is a DATA packet)
					rcv_size = recvfrom(sockfd, &data_fields , PACKET_MAX_SIZE+DATA_HEADER_LEN, 0,
																(struct sockaddr*)&client_addr, (socklen_t*)&client_addr_len);
					if (rcv_size<0)
					{
						perror ("TTFTP_ERROR:  6 ");
						return false;
					}
					errorsnum = 0;
					break;
				}
				else if (slct_val == -1)
				{
					perror ("TTFTP_ERROR:  7 ");
					return false;
				}
				else if (slct_val == 0) // Time out expired while waiting for data to appear at the socket
				{
					printf("FLOWERROR: %d sec pass without new data -> PACKET_TIMEOUT  occured\n", WAIT_FOR_PACKET_TIMEOUT );
					// Send another ACK for the last packet
					send_ack(sockfd,&client_addr, acknum);
					errorsnum++;
				}
				if (errorsnum >= NUMBER_OF_FAILURES)
				{
					printf( "FLOWERROR: %d occured -> FATAL_ERROR, abort\n",NUMBER_OF_FAILURES);
					return false;
				}
			}while (1);
			if (!data_handler(sockfd,&client_addr,&acknum, rcv_size, &data_fields, &end_of_data, filename))// We got something else but DATA The incoming block number is not what we have
			{
				// FATAL ERROR BAIL OUT
				return false;
			}
		}while(false);
		usleep(600);//DEBUG
	}while (!end_of_data); // Have blocks left to be read from client (not end of transmission)
	fclose(filename);
	filename = NULL;
	return true;
}

bool wrq_handler(int rcv_size, s_wrq_fields* wrq_fields, FILE** filename) //TODO: switch to fgets
{
	//validity checks
	if (rcv_size<6)
	{
		printf ("FLOWERROR:  WRQ packet size is too small \n");
		return false;
	}
	if ((ntohs(wrq_fields->opcode)) !=WRQ_OPCODE)
	{
		printf("FLOWERROR:  WRQ opcode is incorrect\n");
		return false;
	}
	int file_name_size = strnlen(wrq_fields->wrq_data, rcv_size - WRQ_HEADER_LEN);
	if ((file_name_size==0)||(file_name_size>=rcv_size-WRQ_HEADER_LEN-2))//2 is min_mode_len
	{
		printf("FLOWERROR:  FILE*NAME is ilegal\n");
		return false;
	}
	int mode_name_size = strnlen(wrq_fields->wrq_data+file_name_size+1, rcv_size - WRQ_HEADER_LEN-file_name_size-1);
	if ((mode_name_size==0)||(mode_name_size>(rcv_size-WRQ_HEADER_LEN-file_name_size-1-1)))//-1 is terminators
	{
		printf("FLOWERROR: the MODE is ilegal\n");
		return false;
	}
	//get the file name and the mode name
	char file_name [PACKET_MAX_SIZE]={0};
	char mode_name [PACKET_MAX_SIZE]={0};
	memcpy(file_name,wrq_fields->wrq_data,file_name_size+1);
	memcpy(mode_name,wrq_fields->wrq_data+file_name_size+1,mode_name_size+1);
	printf( "IN:WRQ, %s, %s\n",file_name ,mode_name );
	*filename = fopen(file_name , "w");
	if (*filename == NULL)
	{
		perror ("TTFTP_ERROR: 5  ");
		return false;
	}
	return true;
}

bool send_ack(int sockfd, s_sockadd_in* client_addr, uint16_t  acknum)
{
	s_ack_fields ack_fields;
	memset(&ack_fields, 0, sizeof(ack_fields));
	ack_fields.opcode =htons(ACK_HEADER);
	ack_fields.blocknum =htons(acknum);
	if (sendto(sockfd, &ack_fields, sizeof(ack_fields), 0,
						(struct sockaddr*)client_addr,  sizeof(*client_addr))  != sizeof(ack_fields))
	{
		perror ("TTFTP_ERROR: 9  ");
		return false;
	}
	printf("OUT:ACK, %d\n",acknum);
	return true;
}

bool data_handler(int sockfd, s_sockadd_in* client_addr, uint16_t*  acknum,
									int rcv_size, s_data_fields* data_fields, bool* end_of_data, FILE* filename)
{
	//validity checks
	if ((rcv_size<(OPCODE_LEN+BLOCK_NUM_LEN)) || (rcv_size>(PACKET_MAX_SIZE+DATA_HEADER_LEN)))
	{
		printf( "FLOWERROR: DATA packet size is ilegal\n");
		return false;
	}
	if (ntohs(data_fields->opcode) != DATA_OPCODE)
	{
			printf( "FLOWERROR:  DATA opcode is incorrect\n");
			return false;
	}
	if (ntohs(data_fields->block_num) != ((*acknum)+1))
	{
			printf( "FLOWERROR:  DATA block_num is incorrect\n");
			return false;
	}
	if (rcv_size < (PACKET_MAX_SIZE+DATA_HEADER_LEN)) //last packet, the data size <512
	{
			*end_of_data = true;
	}
	printf(  "IN:DATA, %d,%d\n",ntohs(data_fields->block_num),rcv_size - ACK_HEADER_LEN);
	size_t lastWriteSize = fwrite(data_fields->data , sizeof(char) , rcv_size-DATA_HEADER_LEN , filename); // write next bulk of data
	if (lastWriteSize != (rcv_size-DATA_HEADER_LEN) )
	{
		perror ("TTFTP_ERROR: 8  ");
		return false;
	}
	printf( "WRITING: %d\n", (int)lastWriteSize);
	(*acknum)++;
	if (send_ack (sockfd, client_addr, *acknum)==false) {return false;}
	return true;
}

void init_vars(s_sockadd_in* client_addr, socklen_t*  client_addr_len,
											bool* end_of_data,int* errorsnum, uint16_t* acknum,  s_wrq_fields* wrq_fields)
{
	memset(client_addr, 0, sizeof(*client_addr));
	memset(wrq_fields ,0, sizeof(*wrq_fields));
	*client_addr_len = sizeof(*client_addr);
	*end_of_data =false;
	*errorsnum = 0;
	*acknum =  0;
}




