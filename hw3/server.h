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
static const unsigned short WRQ_OPCODE = 2;
static const unsigned short DATA_OPCODE = 3;
static const unsigned short ACK_HAEDER = 4;
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
	unsigned short opcode;
	unsigned short blocknum;
}__attribute__((packed));

struct wrq_fields{
	unsigned short opcode;
	char wrq_data [PACKET_MAX_SIZE];
}__attribute__((packed));

struct data_fields{
	unsigned short opcode;
	unsigned short block_num;
	char data [PACKET_MAX_SIZE];
}__attribute__((packed));


class server
{
public:
	server(int port);
	~server();
	bool run();
private:
	bool wrq_handler(int rcv_size);
	bool send_ack();
	bool data_handler(int rcv_size);
	void init_vars();
	int m_sockfd;
	int m_portnum;
	int m_errorsnum;
	unsigned short m_acknum;
	int m_client_addr_len;
	FILE* filename;
	bool m_end_of_data;
	s_sockadd_in my_addr;
	s_sockadd_in client_addr;
	struct timeval tv;
	s_wrq_fields m_wrq_fields;
	s_ack_fields m_ack_fields;
	s_data_fields m_data_fields;
};





#endif /* SERVER_H_ */
