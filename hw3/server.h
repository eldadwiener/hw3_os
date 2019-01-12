/*
 * server.h
 *
 *  Created on: Jan 12, 2019
 *      Author: compm
 */

#ifndef SERVER_H_
#define SERVER_H_

#include  <stdio.h>
#include <unistd.h>
#include <iostream>
#include  <sys/types.h>
#include  <sys/socket.h>
#include  <sys/time.h>
#include  <netinet/ip.h>
#include  <stdlib.h>
#include <string.h>


using namespace std;

static const int WAIT_FOR_PACKET_TIMEOUT = 3;
static const int NUMBER_OF_FAILURES = 7;
static const int PACKET_MAX_SIZE = 512;
static const int DATA_HAEDER_LEN = 4;
static const int WRQ_HAEDER_LEN = 2;
static const int ACK_HAEDER_LEN = 4;
static const int WRQ_HAEDER = 2;
static const int DATA_HAEDER = 3;
static const int ACK_HAEDER = 4;
static const int OPCODE_LEN = 2;
static const int BLOCK_NUM_LEN = 2;

typedef	struct in_addr  s_in_addr;
typedef struct wrq_fields s_wrq_fields;
typedef struct ack_fields s_ack_fields;
typedef struct data_fields s_data_fields;
typedef struct sockadd_in s_sockadd_in;

struct sockadd_in {
	short int 						sin_family; //AF_INET
	unsigned short int 	sin_port;
	s_in_addr 						sin_addr; // internet address (IP)
	unsigned char				sin_zero[8];  // for allignments
};

struct ack_fields{
	char opcode [OPCODE_LEN];
	char blocknum [BLOCK_NUM_LEN];
}__attribute__((packed)); ;

struct wrq_fields{
	char header [WRQ_HAEDER_LEN+1];
	char* file_name;
	char * mode;
} ;

struct data_fields{
	//TODO: maybe do +1 to size of all fields here for the /0 of the strings
	char opcode [OPCODE_LEN+1];
	char block_num [BLOCK_NUM_LEN+1];
	char data [PACKET_MAX_SIZE];
} ;


class server
{
public:
	server(int port);
	~server();
	bool run();
private:
	bool get_buff_data_wrq(int rcv_size);
	bool send_ack();
	bool get_data(int rcv_size);

	int m_sockfd;
	int m_portnum;
	int m_errorsnum;
	int m_acknum;
	int m_newsockfd;
	int m_client_addr_len;
	char buff[PACKET_MAX_SIZE+DATA_HAEDER_LEN];
	FILE* filename;
	bool m_end_of_data;
	s_sockadd_in my_addr;
	s_sockadd_in client_addr;
	bool new_socket_is_open;
	struct timeval tv;
	s_wrq_fields m_wrq_fields;
	s_ack_fields m_ack_fields;
	s_data_fields m_data_fields;

};





#endif /* SERVER_H_ */
