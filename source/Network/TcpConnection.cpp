#include "TcpConnection.h"
#include "../Framework/Logger.h"
#include "../Framework/Framework.h"

extern Logger* debug;
extern Framework* fw;

TcpConnection::TcpConnection(asio::io_service& io_service, int type, Database* db) : socket_(io_service)
{
	this->type = type;
	this->db = db;
	server = NULL;
	client = NULL;

	packetCounter = 0;
	state = NORMAL;
	send_data = NULL;
	send_length = 0;

	ping_timer = NULL;
	delayed_packet = NULL;
	delayed_timer = NULL;
	special_data = NULL;
	special_data_length = 0;
}

tcp::socket& TcpConnection::socket()
{
	return socket_;
}

void TcpConnection::start()
{
	socket_.async_read_some(asio::buffer(received_data, max_length),
							boost::bind(&TcpConnection::handle_read, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
	remoteIp = getRemoteIp();
	remotePort = getRemotePort();
	if(fw->addConnection(type))
	{
		if(type == GAME_SERVER)
			server = new GameServer(type, db);
		else if(type == GAME_CLIENT)
		{
			debug->notification(1, type, "--[%s:%i] connected", remoteIp.c_str(), remotePort);
			// check if incoming connection is local or public
			string emuIp;
			bool isLocal = fw->isIpLocal(remoteIp);

			if(!isLocal)	// and set the emulator ip accordingly
				emuIp = fw->portsCfg().emulator_ip;
			else
				emuIp = getLocalIp();
			client = new GameClient(type, db, emuIp, isLocal);
		}
		ping_timer = new asio::deadline_timer(socket_.get_io_service());
	}
	else if(type != MISC)
		debug->notification(1, type, "--[%s:%i] Maximum number of allowed connections for this type reached, ignoring connection request...", remoteIp.c_str(), remotePort);
}


//////////////////
// Handle stuff //
//////////////////
void TcpConnection::handle_read(const system::error_code& error, size_t bytes_transferred)
{
	if (!error)
	{
		received_length = bytes_transferred;
		StoreIncomingData();
		if(incomingQueue.size() > 0)
		{
			do{
				if(incomingQueue.size() > 1)
					state = QUEUE;
				else
					state = NORMAL;

				Packet* receivedPacket = incomingQueue.front();	//the next unread packet is the front of the queue

				//here we read the incoming stuff and process it
				if(type == GAME_SERVER)
					ProcessGameServer(receivedPacket);
				else if(type == GAME_CLIENT)
					ProcessGameClient(receivedPacket);
				else	//MISC
				{
					Packet* sendPacket = new Packet("&lgr", 0x00000000);
					outgoingQueue.push_back(sendPacket);
					state = NORMAL;
				}
				delete receivedPacket;
				incomingQueue.pop_front();
			}while(state == QUEUE || incomingQueue.size() > 0);
		}
		else
		{
			debug->warning(1, type, "handle_read() - We dont have a packet in the Queue to process! Setting state to DISCONNECT");
			state = DISCONNECT;
		}

		if(state != SKIP && outgoingQueue.front()->isDelayed())
		{
			if(delayed_packet)
				debug->warning(1, type, "handle_read() - The delayed packet is not NULL! -> %s", delayed_packet->GetType());
			delayed_packet = outgoingQueue.front();
			outgoingQueue.pop_front();

			if(!delayed_timer)
				delayed_timer = new asio::deadline_timer(socket_.get_io_service(), posix_time::seconds(delayed_packet->getDelayTime()));
			else
				delayed_timer->expires_from_now(posix_time::seconds(delayed_packet->getDelayTime()));

			delayed_timer->async_wait(boost::bind(&TcpConnection::handle_delayed_send, shared_from_this(), asio::placeholders::error));
			state = SKIP;
			debug->notification(3, type, "initiating delayed packet, expires in %i seconds...", delayed_timer->expires_from_now().total_seconds());
		}

		switch(state)
		{
			case NORMAL:
				{
					if(send_data)
						debug->warning(1, type, "We are overwriting some packet content here!! -> %s", send_data);
					send_data = PacketToData(outgoingQueue.front());
					async_write(socket_, asio::buffer(send_data, send_length), boost::bind(&TcpConnection::handle_write, shared_from_this(), asio::placeholders::error));
					break;
				}
			case SKIP:
				{
					socket_.async_read_some(asio::buffer(received_data, max_length),
								boost::bind(&TcpConnection::handle_read, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
					break;
				}
			case DISCONNECT:
					handle_stop();
			default:
				break;
		}
	}
	else	//client gets disconnected
	{
		if(error.value() != asio::error::eof)
			debug->warning(1, type, "handle_read() - [%s:%i] error message: %s - error code: %i", remoteIp.c_str(), remotePort, error.message().c_str(), error.value());
		handle_stop();
	}
}

void TcpConnection::handle_write(const system::error_code& error)
{
	if(!error)
	{
		//packet was sent here
		string packetData = outgoingQueue.front()->toString();
		debug->notification(2, type, "->[%s:%i] %s 0x%08x {%s}", remoteIp.c_str(), remotePort, outgoingQueue.front()->GetType(), outgoingQueue.front()->GetType2(), packetData.c_str());
		send_data = NULL;
		delete outgoingQueue.front();
		outgoingQueue.pop_front();

		if(outgoingQueue.size() > 0)
			state = QUEUE;
		else if(type == MISC)	// let connection run out of scope if type is MISC (we just send one packet)
			state = DISCONNECT;
		else
			state = NORMAL;

		if(state != NORMAL && state != DISCONNECT && outgoingQueue.front()->isDelayed())
		{
			if(delayed_packet)
				debug->warning(1, type, "handle_read() - The delayed packet is not NULL! -> %s", delayed_packet->GetType());
			delayed_packet = outgoingQueue.front();
			outgoingQueue.pop_front();

			if(!delayed_timer)
				delayed_timer = new asio::deadline_timer(socket_.get_io_service(), posix_time::seconds(delayed_packet->getDelayTime()));
			else
				delayed_timer->expires_from_now(posix_time::seconds(delayed_packet->getDelayTime()));

			delayed_timer->async_wait(boost::bind(&TcpConnection::handle_delayed_send, shared_from_this(), asio::placeholders::error));
			state = NORMAL;
			debug->notification(5, type, "initiating delayed packet, expires in %i seconds...", delayed_timer->expires_from_now().total_seconds());
		}

		switch(state)
		{
			case NORMAL:
				{
					socket_.async_read_some(asio::buffer(received_data, max_length),
								boost::bind(&TcpConnection::handle_read, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
					break;
				}
			case QUEUE:
				{
					if(send_data)
						debug->warning(1, type, "We are overwriting some packet content here!!! -> %s", send_data);
					send_data = PacketToData(outgoingQueue.front());
					async_write(socket_, asio::buffer(send_data, send_length), boost::bind(&TcpConnection::handle_write, shared_from_this(), asio::placeholders::error));
					break;
				}
			case DISCONNECT:
				// this is only accessed when type == MISC, no need to call handle_stop() there
			default:
				break;
		}
	}
	else
	{
		debug->warning(1, UNKNOWN, "handle_write() - [%s:%i] error message: %s - error code: %i", remoteIp.c_str(), remotePort, error.message().c_str(), error.value());
		handle_stop();
	}
}

void TcpConnection::handle_stop()
{
	debug->notification(1, type, "--[%s:%i] disconnected", remoteIp.c_str(), remotePort);
	system::error_code ec;
	if(ping_timer)
	{
		ping_timer->cancel(ec);
		delete ping_timer;
	}
	if(delayed_timer)
	{
		delayed_timer->cancel(ec);
		delete delayed_timer;
	}

	if(delayed_packet)
		delete delayed_packet;

	fw->removeJoinableServer(serverJoinable.id);
	fw->subtractConnection(type);
	if(server)
		delete server;
	if(client)
		delete client;
}

void TcpConnection::handle_one_write(const system::error_code& error)
{
	if(!error)
	{
		string packetData = specialQueue.front()->toString();
		debug->notification(2, type, "->[%s:%i] %s 0x%08x {%s}", remoteIp.c_str(), remotePort, specialQueue.front()->GetType(), specialQueue.front()->GetType2(), packetData.c_str());
		special_data = NULL;
		delete specialQueue.front();
		specialQueue.pop_front();

		if(!specialQueue.empty())
		{
			debug->notification(4, type, "handle_one_write() - one-way packet sent, there are still packets in the special queue though, calling handle again...");
			special_data = PacketToData(specialQueue.front(), true);
			async_write(socket_, asio::buffer(special_data, special_data_length),
						boost::bind(&TcpConnection::handle_one_write, shared_from_this(), asio::placeholders::error));
		}
		else
			debug->notification(5, type, "handle_one_write() - one-way packet sent, getting out of scope...");
	}
	else
		debug->warning(1, type, "handle_one_write() - disconnected, error message: %s - error code: %i", error.message().c_str(), error.value());
}

void TcpConnection::handle_delayed_send(const system::error_code& error)
{
	if(!error)
	{
		if(state != DISCONNECT && delayed_packet)
		{
			debug->notification(5, type, "sending delayed_packet, %s", delayed_packet->GetType());
			specialQueue.push_back(delayed_packet);
			delayed_packet = NULL;

			if(specialQueue.size() > 1)	//if there is already a packet in the queue we just add this packet to the queue, sooner or later it should be processed
				debug->notification(4, type, "handle_delayed_send() - there is more than one packet in the queue, %i", specialQueue.size());
			else
			{
				if(special_data)
					debug->warning(1, type, "We are overwriting some packet content here!! -> %s", special_data);
				special_data = PacketToData(specialQueue.front(), true);
				async_write(socket_, asio::buffer(special_data, special_data_length),
							boost::bind(&TcpConnection::handle_one_write, shared_from_this(), asio::placeholders::error));
			}
		}
	}
	else
	{
		if(type > UNKNOWN || type < DEBUG)		//this happens when the connection is already closed
			type = DEBUG;
		debug->warning(5, type, "handle_delayed_send() - error: %s - error code: %i", error.message().c_str(), error.value());
	}
}

void TcpConnection::handle_ping(const system::error_code& error)
{
	if(!error)
	{
		if(state != DISCONNECT)
		{
			// the ping packet doesn't actually do anything and was just implemented in an attempt to replicate the original traffic
			Packet* sendPacket = new Packet("PING", 0x00000000);
			specialQueue.push_back(sendPacket);

			if(specialQueue.size() > 1)	//if there is already a packet in the queue we just add this packet to the queue, sooner or later it should be processed
				debug->notification(4, type, "handle_ping() - there is more than one packet in the queue, %i", specialQueue.size());
			else
			{
				if(special_data)
					debug->warning(1, type, "We are overwriting some packet content here!! -> %s", special_data);
				special_data = PacketToData(specialQueue.front(), true);
				async_write(socket_, asio::buffer(special_data, special_data_length),
							boost::bind(&TcpConnection::handle_one_write, shared_from_this(), asio::placeholders::error));
			}
		}
	}
	else
	{
		if(type > UNKNOWN || type < DEBUG)		//this happens when the connection is already closed
			type = DEBUG;
		debug->warning(5, type, "handle_ping() - error: %s - error code: %i", error.message().c_str(), error.value());
	}
}

void TcpConnection::handle_invoke(Packet* sendPacket)
{
	if(state != DISCONNECT && sendPacket != NULL)
	{
		specialQueue.push_back(sendPacket);
		if(specialQueue.size() > 1)	//if there is already a packet in the queue we just add this packet to the queue, sooner or later it should be processed
			debug->notification(4, type, "handle_invoke() - there is more than one packet in the queue, %i", specialQueue.size());
		else
		{
			if(special_data)
				debug->warning(1, type, "We are overwriting some packet content here!! -> %s", special_data);
			special_data = PacketToData(specialQueue.front(), true);
			async_write(socket_, asio::buffer(special_data, special_data_length),
						boost::bind(&TcpConnection::handle_one_write, shared_from_this(), asio::placeholders::error));
		}
	}
}

//////////////////
// Packet stuff //
//////////////////
void TcpConnection::StoreIncomingData()
{
	if(incomingQueue.empty())	// did we process all other packets that we received yet?
	{
		int current_length = 0;
		do{
			char type[HEADER_LENGTH+1];
			memcpy((void*)type, received_data+current_length, HEADER_LENGTH);

			unsigned int type2 = decode(received_data+current_length+4, 4);
			int length = decode(received_data+current_length+8, 4);

			unsigned char* data = new unsigned char[length-12];
			memcpy(data, received_data+current_length+12, length-12);

			//Create the package and add it to the end of the queue
			incomingQueue.push_back(new Packet(type, type2, length, (char*)data));
			string packetData = incomingQueue.back()->toString();
			debug->notification(2, this->type, "<-[%s:%i] %s 0x%08x {%s}", remoteIp.c_str(), remotePort, incomingQueue.back()->GetType(), incomingQueue.back()->GetType2(), packetData.c_str());

			current_length += length;
			delete[] data;
			if(current_length != received_length)
				debug->notification(5, this->type, "StoreIncomingData() - we have multiple packets incoming at once");
		}while(current_length != received_length);

		if(incomingQueue.empty())
			debug->warning(2, type, "StoreIncomingData() - incoming Queue has no content!");
	}
}

char* TcpConnection::PacketToData(Packet* packet, bool is_special)
{
	string packetData = packet->GetData();

	uint8_t *type1	= (uint8_t*)packet->GetType();
	uint32_t type2	= (uint32_t)packet->GetType2();
	uint8_t *fmt	= (uint8_t*)packetData.c_str();

	static uint8_t *buff = NULL;
	int slen, len;

	len = strlen((char*)fmt);
	slen = 12 + len;
	if(slen > 0)
	{
		buff = (uint8_t*)realloc(buff, slen);
	}
	memcpy(buff, type1, 4);

	if(packet->GetValCount() && (type2 & 0x80000000) == 0x80000000) {	// its a regular packet, do we want to count it?
		if((type2 & 0x00ffffff) == 1)
			packetCounter = 0;					// is the packet the first one we are receiving?
		if(packet->GetValFirst())
			packetCounter++;					// packet counter (not needed for linked packets so we stop counting after first one is sent)
		type2 = (type2 & 0xff000000) | packetCounter;
	}
	packet->SetType2((unsigned int)type2);	// update the packet so it is shown correctly in the log (the data that we send here is not affected by this)

	encode(buff + 4, type2, 4);
	encode(buff + 8, slen, 4);
	memcpy(buff + 12, fmt, len);

	// EA uses the final NULL delimiter so we set the last char from the data to NULL
	if(buff[len+12-1] == '\n')	// every self-created packet normally has a newline at the end
		buff[len+12-1] = '\0';

	if(is_special)
		special_data_length = slen;
	else
		send_length = slen;

	return (char*)buff;
}

unsigned int TcpConnection::decode(unsigned char* data, int bytes)
{
	int num, i;
	for(num = i = 0; i < bytes; i++) {
		//num |= (data[i] << (i << 3)); // little
		num |= (data[i] << ((bytes - 1 - i) << 3)); // big
	}
	return(num);
}

unsigned int TcpConnection::encode(uint8_t *data, uint32_t num, int bytes)
{
	for(int i = 0; i < bytes; i++) {
		//data[i] = num >> (i << 3);	// little
		data[i] = num >> ((bytes - 1 - i) << 3);	// big
	}
	return(bytes);
}


//////////////////
// Socket stuff //
//////////////////
string TcpConnection::getLocalIp()
{
	return socket().local_endpoint().address().to_string();
}
int TcpConnection::getLocalPort()
{
	return socket().local_endpoint().port();
}

string TcpConnection::getRemoteIp()
{
	return socket().remote_endpoint().address().to_string();
}
int TcpConnection::getRemotePort()
{
	return socket().remote_endpoint().port();
}


/////////////////////////////////
// GameServer processing stuff //
/////////////////////////////////
void TcpConnection::ProcessGameServer(Packet* receivedPacket)
{
	char* packet_type = receivedPacket->GetType();

	// TODO: pass the user defined ip here if the remote ip turns out to be local (is the parameter really only used when adding the server to the db?)
	state = server->ProcessTheater(&outgoingQueue, receivedPacket, packet_type, remoteIp.c_str());
	//state = server->ProcessTheater(&outgoingQueue, receivedPacket, packet_type, fw->portsCfg().emulator_ip.c_str());

	if(state == UNKNOWN || state == ERROR_STATE)
	{
		string packetData = receivedPacket->toString();
		if(state == UNKNOWN)
			debug->warning(2, this->type, "Could not handle Packet - %s from [%s:%i]", packetData.c_str(), remoteIp.c_str(), remotePort);
		debug->notification(5, this->type, "Continue reading, last recieved packet - %s from [%s:%i]", packetData.c_str(), remoteIp.c_str(), remotePort);
	}

	if(strcmp(packet_type, "CGAM") == 0)
	{
		serverJoinable.id = lexical_cast<int>(outgoingQueue.front()->GetVar("GID"));
		serverJoinable.socket = shared_from_this();	//it would be better to just pass socket_ and do the async operation in the client object where we point it to the handler_invoke
		fw->addJoinableServer(serverJoinable);
	}
	update_ping_timer();
}


/////////////////////////////////
// GameClient processing stuff //
/////////////////////////////////
void TcpConnection::ProcessGameClient(Packet* receivedPacket)
{
	char* packet_type = receivedPacket->GetType();
	do{
		state = client->ProcessTheater(&outgoingQueue, receivedPacket, packet_type, remoteIp.c_str());

		if(state == UNKNOWN || state == ERROR_STATE)
		{
			string packetData = receivedPacket->toString();
			if(state == UNKNOWN)
				debug->warning(2, this->type, "Could not handle Packet - %s from [%s:%i]", packetData.c_str(), remoteIp.c_str(), remotePort);
			debug->notification(5, this->type, "Continue reading, last recieved packet - %s from [%s:%i]", packetData.c_str(), remoteIp.c_str(), remotePort);
		}
	}while(state == QUEUE);

	// TODO: add (proper) queue support
	if(strcmp(packet_type, "ECNL") == 0 && delayed_timer)	// means client is in a queue so we want to delay the packet a little bit?
	{
		if(delayed_timer->expires_from_now().total_seconds() > 0)
		{
			system::error_code ec;
			delayed_timer->cancel(ec);
			if(delayed_packet)
			{
				delete delayed_packet;
				delayed_packet = NULL;
			}
		}
	}
	update_ping_timer();
}

void TcpConnection::update_ping_timer()
{
	if(ping_timer && incomingQueue.size() <= 1)		// all packets are processed, update ping timer (would it be better to just remove the queue check?)
	{
		int duration = ping_timer->expires_from_now().total_seconds();

		if(duration < 0 || duration > 150)
			duration = 150;
		else if(duration >= 0 && duration+10 < 150)		// reset the timer
			duration = 150 + (duration % 10);	// ping intervals seem to be approximately 150 seconds (why did I think it was a good idea to do it like this?)

		ping_timer->expires_from_now(posix_time::seconds(duration));
		ping_timer->async_wait(boost::bind(&TcpConnection::handle_ping, shared_from_this(), asio::placeholders::error));
	}
}

GameServer* TcpConnection::getGameServer()		// needed to get the player count on the server
{
	return server;
}
