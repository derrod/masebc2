#include "UdpServer.h"
#include "../Framework/Logger.h"

extern Logger* debug;

UdpServer::UdpServer(asio::io_service& io_service, int type, int port) : socket_(io_service, udp::endpoint(udp::v4(), port))
{
	this->type = type;
	this->port = port;

	start_receive();
	debug->notification(1, type, "Created UDP Socket (listening on port %i)", port);
}

void UdpServer::start_receive()
{
	socket_.async_receive_from( asio::buffer(received_data, max_length), remote_endpoint_,
								boost::bind(&UdpServer::handle_receive, this,
											asio::placeholders::error, asio::placeholders::bytes_transferred));
}

void UdpServer::handle_receive(const system::error_code& error, size_t bytes_transferred)
{
	if(!error && bytes_transferred > 0)
	{
		ProcessData(DataToPacket(bytes_transferred));
		socket_.async_send_to(asio::buffer(send_data, send_data.size()), remote_endpoint_,
								boost::bind(&UdpServer::handle_send, this, asio::placeholders::error, asio::placeholders::bytes_transferred));
		start_receive();
	}
}

void UdpServer::handle_send(const system::error_code& /*error*/, size_t /*bytes_transferred*/)
{
	//done sending, do we need to do something more here? (normally we don't specifically wait for incoming udp packets)
}

void UdpServer::handle_stop()
{
	socket_.close();
}

Packet* UdpServer::DataToPacket(size_t bytes_transferred)
{
	size_t current_length = 0;
	Packet* packet;
	do{
		char type[HEADER_LENGTH+1];
		memcpy((void*)type, received_data+current_length, 4);

		unsigned int type2 = decode(received_data+current_length+4, 4);
		int length = decode(received_data+current_length+8, 4);

		unsigned char* data = new unsigned char[length-12];
		memcpy(data, received_data+current_length+12, length-12);

		//Create the package and add it to the end of the queue
		packet = new Packet(type, type2, length, (char*)data);
		string packetData = packet->toString();
		debug->notification(3, this->type, "<<[%s] %s 0x%08x {%s}", getRemoteIp().c_str(), packet->GetType(), packet->GetType2(), packetData.c_str());

		current_length += length;
		delete[] data;
		if(current_length != bytes_transferred)
			debug->warning(2, this->type, "UDP - We received a group of packets! Keep the log to investigate the packet, need to change implementation for this");
	}while(current_length != bytes_transferred);

	return packet;
}


string UdpServer::PacketToData(Packet* packet)
{
	string buffer = packet->GetType();
	buffer.append(7, '\0');		// upd packets have zero filled header

	int len = 12 + packet->GetData().size() + 1;	//header + data + final null char
	buffer.append(1, (char)len);
	buffer.append(packet->GetData());
	buffer.push_back('\0');

	string packetData = packet->toString();
	debug->notification(3, type, ">>[%s] %s 0x%08x {%s}", getRemoteIp().c_str(), packet->GetType(), packet->GetType2(), packetData.c_str());
	delete packet;

	return buffer;
}

unsigned int UdpServer::decode( unsigned char* data, int bytes )
{
	int num, i;
	for(num = i = 0; i < bytes; i++) {
		//num |= (data[i] << (i << 3)); // little
		num |= (data[i] << ((bytes - 1 - i) << 3)); // big
	}
	return(num);
}

string UdpServer::getLocalIp()
{
	return socket_.local_endpoint().address().to_string();
}
int UdpServer::getLocalPort()
{
	return socket_.local_endpoint().port();
}

string UdpServer::getRemoteIp()
{
	return remote_endpoint_.address().to_string();
}
int UdpServer::getRemotePort()
{
	return remote_endpoint_.port();
}

void UdpServer::ProcessData(Packet* packet)
{
	Packet* sendPacket = NULL;
	if(packet != NULL)
	{
		//debug->notification(2, type, "local port: %i, remote port: %i", getLocalPort(), getRemotePort());
		string port = lexical_cast<string>(getRemotePort());
		sendPacket = new Packet("ECHO", 0x00000000);
		sendPacket->SetVar("TXN", "ECHO");
		sendPacket->SetVar("IP", getRemoteIp());
		sendPacket->SetVar("PORT", port);
		sendPacket->SetVar("ERR", "0");
		sendPacket->SetVar("TYPE", "1");
		sendPacket->SetVar("TID", packet->GetVar("TID"));
		delete packet;
	}
	else
		debug->warning(2, type, "UDP - Packet is NULL");

	send_data = PacketToData(sendPacket);
}
