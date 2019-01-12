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
	m_newsockfd(-1),
	m_client_addr_len(0),
	//buff({'0'}),
	filename(NULL),
	m_end_of_data(false),
	new_socket_is_open(false)
{
	memset(buff,0,PACKET_MAX_SIZE+DATA_HAEDER_LEN);
	m_sockfd = socket(AF_INET, SOCK_DGRAM,0);
	if (m_sockfd<0)
	{
		perror("TTFTP_ERROR:   ");
		return;
	}
	else
	my_addr.sin_family = AF_INET;
	my_addr.sin_addr.s_addr  =  INADDR_ANY;
	my_addr.sin_port = htons(m_portnum);
	m_client_addr_len = sizeof(client_addr);
	if (bind(m_sockfd, (struct sockaddr*)&my_addr ,sizeof(my_addr))< 0)
	{
		perror ("TTFTP_ERROR:   ");
		return;
	}
	tv.tv_sec = WAIT_FOR_PACKET_TIMEOUT;
	tv.tv_usec = 0;
	memset(m_wrq_fields.header + WRQ_HAEDER_LEN ,0,1);
	memset(m_data_fields.opcode + OPCODE_LEN  ,0,1);
	memset(m_data_fields.block_num+BLOCK_NUM_LEN,0,1);
}

server::~server()
{
	close(m_sockfd);
	//TODO: delete all[]
	if ( filename != NULL)
	{
		fclose(filename);
	}
	if ( filename != NULL)
	{
		fclose(filename);
	}
	if(new_socket_is_open)
	{
		close(m_newsockfd);
	}
}


bool server::run()
{
	listen (m_sockfd, 1); //blocking
	m_newsockfd = accept (m_sockfd,(struct sockaddr*)&client_addr, (unsigned*)&m_client_addr_len);
	if (m_newsockfd <0)
	{
		perror ("TTFTP_ERROR:   ");
		return false;
	}
	new_socket_is_open = true;
	// get WRQ
	fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(m_newsockfd, &rfds);
	while (!select(m_newsockfd+1, &rfds, NULL, NULL, &tv ))
	{
		cout << "FLOWERROR: " << WAIT_FOR_PACKET_TIMEOUT <<" sec pass without new data -> PACKET_TIMEOUT  occured"<< endl;
		m_errorsnum++;
		if (m_errorsnum >= NUMBER_OF_FAILURES)
		{
			cout << "FLOWERROR: " << NUMBER_OF_FAILURES <<" occured -> FATAL_ERROR, abort"<< endl;
			return false;
		}
	}
	int rcv_size = recv(m_newsockfd, &buff , PACKET_MAX_SIZE+DATA_HAEDER_LEN, 0);
	if (rcv_size<0)
	{
		perror ("TTFTP_ERROR:   ");
		return false;
	}
	if (!get_buff_data_wrq(rcv_size))
	{
		return false;
	}
	cout << "IN:WRQ, "<< m_wrq_fields.file_name <<", "<<m_wrq_fields.mode << endl;
	filename = fopen(m_wrq_fields.file_name , "w");
	delete m_wrq_fields.file_name;
	delete m_wrq_fields.mode;
	if (filename == NULL)
	{
		perror ("TTFTP_ERROR:   ");
		return false;
	}

	if (!send_ack())
	{
		return false;
	}
	m_errorsnum = 0;
	do
	{
		//do
		//{
			do
			{
				int slct_val = select(m_newsockfd+1, &rfds, NULL, NULL, &tv );
				if (slct_val > 0)// if there was something at the socket and we are here not because of a timeout
				{
					// TODO: Read the DATA packet from the socket (at  least we hope this is a DATA packet)
					memset (buff, 0, PACKET_MAX_SIZE+DATA_HAEDER_LEN );
					rcv_size = recv(m_newsockfd, &buff , PACKET_MAX_SIZE+DATA_HAEDER_LEN, 0);
					if (rcv_size<0)
					{
						perror ("TTFTP_ERROR:   ");
						return false;
					}
					break;
				}
				else if (slct_val == -1)
				{
					perror ("TTFTP_ERROR:   ");
					return false;
				}
				else if (slct_val == 0) // TODO: Time out expired while waiting for data to appear at the socket
				{
					cout << "FLOWERROR: " << WAIT_FOR_PACKET_TIMEOUT <<" sec pass without new data -> PACKET_TIMEOUT  occured"<< endl;
					// Send another ACK for the last packet
					m_acknum--;
					send_ack();
					m_errorsnum++;
				}
				if (m_errorsnum >= NUMBER_OF_FAILURES)
				{
					cout << "FLOWERROR: " << NUMBER_OF_FAILURES <<" occured -> FATAL_ERROR, abort"<< endl;
					return false;
				}
			}while (1); // TODO: check it : Continue while some socket was ready but recvfrom somehow failed to read the data
			if (!get_data(rcv_size))
				// We got something else but DATA The incoming block number is not what we have
			{
				// FATAL ERROR BAIL OUT
				return false;
			}
			cout << "IN:DATA, "<< m_data_fields.block_num << ","<<rcv_size - ACK_HAEDER_LEN <<endl;
		//}while (false);
		m_errorsnum = 0;
		int lastWriteSize = fwrite(m_data_fields.data , sizeof(char) , rcv_size-DATA_HAEDER_LEN , filename); // write next bulk of data
		if (lastWriteSize != (rcv_size-DATA_HAEDER_LEN) )
		{
			perror ("TTFTP_ERROR:   ");
			return false;
		}
		cout <<"WRITING: " << lastWriteSize<< endl;
		//  send ACK packet to the client
		send_ack();
	}while (!m_end_of_data); // Have blocks left to be read from client (not end of transmission)
	close(m_newsockfd);
	new_socket_is_open = false;
	fclose(filename);
	filename = NULL;
	return true;
}

bool server::get_buff_data_wrq(int rcv_size) //TODO: switch to fgets
{
	if (rcv_size<6)
	{
		cout << "error"<<endl; //TODO: complete
		return false;
	}
	//char* temp;
	//temp = new char[rcv_size+1];
	//temp [rcv_size]='\0';
	memcpy(m_wrq_fields.header, buff/*temp*/, WRQ_HAEDER_LEN);
	if (atoi(m_wrq_fields.header)!=WRQ_HAEDER)
	{
		cout << "FLOWERROR:  WRQ opcode is incorrect"<<endl; //TODO: complete
		//delete temp[];
		return false;
	}
	int i = WRQ_HAEDER_LEN;
	for (; i <rcv_size-1; i++)
	{
		if (/*temp[i]*/buff[i]=='0') break;
	}
	if ((i==WRQ_HAEDER_LEN)||(i==rcv_size-2))
	{
		cout << "FLOWERROR:  FILENAME is ilegal"<<endl;  //TODO: complete
		//delete temp[];
		return false;
	}
	int j =i+1;
	for (; j <rcv_size; i++)
	{
		if (/*temp[j]*/buff[j]=='0') break;
	}
	if ((j==(i+1)) || (j !=rcv_size-1))
	{
		cout << "FLOWERROR: the MODE is ilegal"<<endl; //TODO: complete
		//delete temp[];
		return false;
	}
	int file_name_size = i - WRQ_HAEDER_LEN +1;
	int mode_size = j - i;
	m_wrq_fields.file_name = new char[file_name_size];
	m_wrq_fields.file_name[file_name_size-1]='\0';
	m_wrq_fields.mode = new char [mode_size];
	m_wrq_fields.mode[mode_size-1]='\0';
	memcpy(m_wrq_fields.file_name, /*temp*/buff+WRQ_HAEDER_LEN, file_name_size-1);
	memcpy(m_wrq_fields.mode, /*temp*/buff+i+1, mode_size-1);
	//delete temp[];
	return true;
}

bool server::send_ack()
{
	m_acknum++;
	memset(m_ack_fields.opcode, ACK_HAEDER, 2);
	memset(m_ack_fields.blocknum, m_acknum, 2);
	if (sendto(m_newsockfd, &m_ack_fields, sizeof(m_ack_fields), 0, NULL, 0 )<0) //TODO: check flags
	{
		perror ("TTFTP_ERROR:   ");
		return false;
	}
	cout <<"OUT:ACK, " <<m_acknum <<endl;
	return true;
}

bool server::get_data(int rcv_size)
{
	memset(m_data_fields.data, 0, PACKET_MAX_SIZE);
	if ((rcv_size<(OPCODE_LEN+BLOCK_NUM_LEN)) || (rcv_size>(PACKET_MAX_SIZE+DATA_HAEDER_LEN)))
	{
		cout << "FLOWERROR: DATA packet size is ilegal"<<endl; //TODO: complete
		return false;
	}
	memcpy(m_data_fields.opcode,buff,OPCODE_LEN);
	memcpy(m_data_fields.block_num,buff+OPCODE_LEN,BLOCK_NUM_LEN);
	memcpy(m_data_fields.data,buff+DATA_HAEDER_LEN,rcv_size-DATA_HAEDER_LEN);
	if (atoi(m_data_fields.opcode) != DATA_HAEDER)
	{
			cout << "FLOWERROR:  DATA opcode is incorrect"<<endl;//TODO: complete
			return false;
	}
	if (atoi(m_data_fields.block_num) != (m_acknum+1))
	{
			cout <<"FLOWERROR:  DATA block_num is incorrect"<<endl;//TODO: complete
			return false;
	}
	if (rcv_size != (PACKET_MAX_SIZE+DATA_HAEDER_LEN))
	{
			m_end_of_data = true;
	}
	return true;
}

