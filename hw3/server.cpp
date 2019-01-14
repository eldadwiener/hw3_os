/*
 * server.cpp
 *
 *  Created on: Jan 12, 2019
 *      Author: compm
 */
#include "server.h"


server::server(int port):
	m_sockfd (-1),
	m_portnum (port),
	m_errorsnum (0),
	m_acknum(-1),
	m_client_addr_len(0),
	filename(NULL),
	m_end_of_data(false)
{
	m_sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_sockfd<0)
	{
		perror("TTFTP_ERROR:1   ");
		return;
	}
	cout <<"socket num is: "<<m_sockfd<<endl; //DEBUG
	my_addr.sin_family = PF_INET;
	my_addr.sin_addr.s_addr  =  INADDR_ANY;
	my_addr.sin_port = htons(m_portnum);
	m_client_addr_len = sizeof(client_addr);
	if (bind(m_sockfd, (struct sockaddr*)&my_addr ,sizeof(my_addr))< 0)
	{
		perror ("TTFTP_ERROR: 2  ");
		return;
	}
	tv.tv_sec = WAIT_FOR_PACKET_TIMEOUT;
	tv.tv_usec = 0;
}

server::~server()
{
	close(m_sockfd);
	if ( filename != NULL)
	{
		fclose(filename);
	}
	if ( filename != NULL)
	{
		fclose(filename);
	}
}


bool server::run()
{
	init_vars();
	// get WRQ
	fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(m_sockfd, &rfds);
	int rcv_size = recvfrom(m_sockfd, &m_wrq_fields, PACKET_MAX_SIZE+DATA_HAEDER_LEN, 0,
														(struct sockaddr*)&client_addr, (unsigned*)&m_client_addr_len);
	if (rcv_size<0)
	{
		perror ("TTFTP_ERROR: 4  ");
		return false;
	}
	if (!wrq_handler(rcv_size))
	{
		return false;
	}
	m_acknum++;
	if (!send_ack())
	{
		return false;
	}
	sleep(2);
	do
	{
			do
			{
				int slct_val = select(m_sockfd+1, &rfds, NULL, NULL, &tv );
				sleep(2);
				if (slct_val > 0)// if there was something at the socket and we are here not because of a timeout
				{
					// TODO: Read the DATA packet from the socket (at  least we hope this is a DATA packet)
					rcv_size = recvfrom(m_sockfd, &m_data_fields , PACKET_MAX_SIZE+DATA_HAEDER_LEN, 0,
																(struct sockaddr*)&client_addr, (unsigned*)&m_client_addr_len);
					if (rcv_size<0)
					{
						perror ("TTFTP_ERROR:  6 ");
						return false;
					}
					m_errorsnum = 0;
					break;
				}
				else if (slct_val == -1)
				{
					perror ("TTFTP_ERROR:  7 ");
					return false;
				}
				else if (slct_val == 0) // Time out expired while waiting for data to appear at the socket
				{
					cout << "FLOWERROR: " << WAIT_FOR_PACKET_TIMEOUT <<" sec pass without new data -> PACKET_TIMEOUT  occured"<< endl;
					// Send another ACK for the last packet
					send_ack();
					m_errorsnum++;
				}
				if (m_errorsnum >= NUMBER_OF_FAILURES)
				{
					cout << "FLOWERROR: " << NUMBER_OF_FAILURES <<" occured -> FATAL_ERROR, abort"<< endl;
					return false;
				}
		}while (1); // TODO: check it : Continue while some socket was ready but recvfrom somehow failed to read the data
		if (!data_handler(rcv_size))// We got something else but DATA The incoming block number is not what we have
		{
			// FATAL ERROR BAIL OUT
			return false;
		}
	}while (!m_end_of_data); // Have blocks left to be read from client (not end of transmission)
	fclose(filename);
	filename = NULL;
	return true;
}

bool server::wrq_handler(int rcv_size) //TODO: switch to fgets
{
	//validity checks
	if (rcv_size<6)
	{
		cout << "FLOWERROR:  WRQ packet size is too small "<<endl; //TODO: complete
		return false;
	}
	if ((ntohs(m_wrq_fields.opcode)) !=WRQ_OPCODE)
	{
		cout <<"WRQ opcode "<<m_wrq_fields.opcode<<endl;
		cout << "FLOWERROR:  WRQ opcode is incorrect"<<endl; //TODO: complete
		return false;
	}
	int file_name_size = strnlen(m_wrq_fields.wrq_data, rcv_size - WRQ_HAEDER_LEN);
	if ((file_name_size==0)||(file_name_size>=rcv_size-WRQ_HAEDER_LEN-2))//2 is min_mode_len
	{
		cout << "FLOWERROR:  FILENAME is ilegal"<<endl;  //TODO: complete
		return false;
	}
	int mode_name_size = strnlen(m_wrq_fields.wrq_data+file_name_size+1, rcv_size - WRQ_HAEDER_LEN-file_name_size-1);
	cout <<"mode size "<<mode_name_size<<"the "<<endl;//DEBUG
	if ((mode_name_size==0)||(mode_name_size>(rcv_size-WRQ_HAEDER_LEN-file_name_size-1-1)))//-1 is terminators
	{
		cout << "FLOWERROR: the MODE is ilegal"<<endl;  //TODO: complete
		return false;
	}
	//get the file name and the mode name
	char file_name [PACKET_MAX_SIZE];
	char mode_name [PACKET_MAX_SIZE];
	memcpy(file_name,m_wrq_fields.wrq_data,file_name_size+1);
	memcpy(mode_name,m_wrq_fields.wrq_data+file_name_size+1,mode_name_size+1);
	cout << "IN:WRQ, "<< file_name <<", "<<mode_name << endl;
	filename = fopen(file_name , "w+");
	if (filename == NULL)
	{
		perror ("TTFTP_ERROR: 5  ");
		return false;
	}
	return true;
}

bool server::send_ack()
{
	m_ack_fields.opcode = htons(ACK_HAEDER);
	m_ack_fields.blocknum = htons(m_acknum);
	cout <<"nice print"<<endl;//DEBUG
	sleep(2);
	if (sendto(m_sockfd, &m_ack_fields, sizeof(&m_ack_fields), 0,
						(struct sockaddr*)&client_addr,  m_client_addr_len)  != sizeof(m_ack_fields)) //TODO: check flags
	{
		perror ("TTFTP_ERROR: 9  ");
		return false;
	}
	cout <<"OUT:ACK, " <<m_acknum <<endl;
	return true;
}

bool server::data_handler(int rcv_size)
{
	cout<<"in handler"<<endl;//DEBUG
	//validity checks
	if ((rcv_size<(OPCODE_LEN+BLOCK_NUM_LEN)) || (rcv_size>(PACKET_MAX_SIZE+DATA_HAEDER_LEN)))
	{
		cout << "FLOWERROR: DATA packet size is ilegal"<<endl;
		return false;
	}
	cout<<"in handler before check DATA_OPCODE"<<endl;//DEBUG
	if (ntohs(m_data_fields.opcode) != DATA_OPCODE)
	{
			cout<<"1"<<endl;//DEBUG
			cout<<"opcode is: "<<m_data_fields.opcode<< "and with ntohs" <<ntohs(m_data_fields.opcode)<<endl;//DEBUG
			cout << "FLOWERROR:  DATA opcode is incorrect"<<endl;//TODO: complete
			return false;
	}
	cout<<"in handler before check block_num"<<endl;//DEBUG
	if (ntohs(m_data_fields.block_num) != (m_acknum+1))
	{
			cout <<"FLOWERROR:  DATA block_num is incorrect"<<endl;//TODO: complete
			return false;
	}
	cout<<"in handler before check m_end_of_data"<<endl;//DEBUG
	if (rcv_size < (PACKET_MAX_SIZE+DATA_HAEDER_LEN)) //last packet, the data size <512
	{
			m_end_of_data = true;
	}
	cout<<"in handler before print IN:DATA"<<endl;//DEBUG
	cout << "IN:DATA, "<<ntohs(m_data_fields.block_num)<< ","<<rcv_size - ACK_HAEDER_LEN <<endl;
	int lastWriteSize = fwrite(m_data_fields.data , sizeof(char) , rcv_size-DATA_HAEDER_LEN , filename); // write next bulk of data
	if (lastWriteSize != (rcv_size-DATA_HAEDER_LEN) )
	{
		perror ("TTFTP_ERROR: 8  ");
		return false;
	}
	cout <<"WRITING: " << lastWriteSize<< endl;
	//  send ACK packet to the client
	m_acknum++;
	send_ack();
	return true;
}

void server::init_vars()
{
	m_client_addr_len = sizeof(client_addr);
	filename = NULL;
	m_end_of_data =false;
	m_errorsnum = 0;
	m_acknum = -1;
	memset(m_wrq_fields.wrq_data  ,0,PACKET_MAX_SIZE);
	memset(m_data_fields.data ,0,PACKET_MAX_SIZE+1);
}




