#include "TcpConnectionSSL.h"
#include "../Base64.h"
#include "../Framework/Logger.h"
#include "../Framework/Framework.h"

extern Logger* debug;
extern Framework* fw;

TcpConnectionSSL::TcpConnectionSSL(asio::io_service& io_service, int type, asio::ssl::context& context, Database* db) : socket_(io_service, context)
{
	this->type = type;
	this->db = db;
	server = NULL;
	client = NULL;

	self_created_packet = false;
	encoded_data = "";
	packetCounter = 0;
	state = NORMAL;

	send_data = NULL;
	send_length = 0;

	special_data = NULL;
	special_data_length = 0;
	ping_timer = NULL;
	memcheck_timer = NULL;
}

socketSSL::lowest_layer_type& TcpConnectionSSL::socket()
{
	return socket_.lowest_layer();
}

void TcpConnectionSSL::start()
{
	//before reading stuff we have to do a handshake
	socket_.async_handshake(asio::ssl::stream_base::server,
								boost::bind(&TcpConnectionSSL::handle_handshake, shared_from_this(), asio::placeholders::error));
}

//////////////////
// Handle stuff //
//////////////////
void TcpConnectionSSL::handle_handshake(const boost::system::error_code& error)
{
	if (!error)
	{
		remoteIp = getRemoteIp();
		remotePort = getRemotePort();

		if(fw->addConnection(type))
		{
			socket_.async_read_some(asio::buffer(received_data, max_length),
								boost::bind(&TcpConnectionSSL::handle_read, shared_from_this(),
									asio::placeholders::error, asio::placeholders::bytes_transferred));
			debug->notification(1, type, "--[%s:%i] connected", remoteIp.c_str(), remotePort);

			// check if incoming connection is local or public
			if(!fw->isIpLocal(remoteIp))	// and set the emulator ip accordingly
				emuIp = fw->portsCfg().emulator_ip;
			else
				emuIp = getLocalIp();
		}
		else
			debug->notification(1, type, "Maximum number of allowed connections for this type reached, ignoring [%s:%i]", remoteIp.c_str(), remotePort);
	}
	else
	{
		// getting the IP/Port here seems to work in Windows but apparently the connection is already destroyed here in Linux so just print the error
		debug->warning(1, type, "handle_handshake() - disconnected, error message: %s - error code: %i", error.message().c_str(), error.value());
	}
}

void TcpConnectionSSL::handle_read(const system::error_code& error, size_t bytes_transferred)
{
	if (!error)
	{
		state = NORMAL;		//is it safe to always set this to NORMAL?
		received_length = bytes_transferred;
		StoreIncomingData();
		while(incomingQueue.size() > 0)
		{
			Packet* receivedPacket = incomingQueue.front();		//the next unread packet is the front of the queue
			ProcessPacket(receivedPacket);
			if(!self_created_packet)
			{
				delete receivedPacket;
				incomingQueue.pop_front();
			}
			else
				self_created_packet = false;
		}
		if(outgoingQueue.size() < 1 && state != SKIP && state != DISCONNECT)
		{
			debug->warning(1, type, "handle_read() - We dont have a packet in the Queue to process! Setting state to DISCONNECT");
			state = DISCONNECT;
		}

		switch(state)
		{
			case NORMAL:
				{
					if(send_data)
						debug->warning(1, type, "We are overwriting some packet content here!!! -> %s", send_data);
					send_data = PacketToData(outgoingQueue.front());
					async_write(socket_, asio::buffer(send_data, send_length),
								boost::bind(&TcpConnectionSSL::handle_write, shared_from_this(), asio::placeholders::error));
					break;
				}
			case SKIP:
				{
					socket_.async_read_some(asio::buffer(received_data, max_length),
								boost::bind(&TcpConnectionSSL::handle_read, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
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
		debug->warning(1, type, "handle_read() - [%s:%i] error message: %s - error code: %i", remoteIp.c_str(), remotePort, error.message().c_str(), error.value());
		handle_stop();
	}
}

void TcpConnectionSSL::handle_write(const system::error_code& error)
{
	if (!error)
	{
		//packet was sent here
		string packetData = outgoingQueue.front()->toString();
		debug->notification(2, type, "->[%s:%i] %s 0x%08x {%s}", remoteIp.c_str(), remotePort, outgoingQueue.front()->GetType(), outgoingQueue.front()->GetType2(), packetData.c_str());
		send_data = NULL;

		delete outgoingQueue.front();
		outgoingQueue.pop_front();

		if(outgoingQueue.size() > 0)
			state = QUEUE;
		else
			state = NORMAL;

		switch(state)
		{
			case NORMAL:
				{
					socket_.async_read_some(asio::buffer(received_data, max_length),
								boost::bind(&TcpConnectionSSL::handle_read, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred));
					break;
				}
			case QUEUE:
				{
					if(send_data)
						debug->warning(1, type, "We are overwriting some packet content here!! -> %s", send_data);
					send_data = PacketToData(outgoingQueue.front());
					async_write(socket_, asio::buffer(send_data, send_length),
								boost::bind(&TcpConnectionSSL::handle_write, shared_from_this(), asio::placeholders::error));
					break;
				}
			default:
				break;
		}
	}
	else
	{
		debug->warning(1, type, "handle_write() - [%s:%i] error message: %s - error code: %i", remoteIp.c_str(), remotePort, error.message().c_str(), error.value());
		handle_stop();
	}
}

void TcpConnectionSSL::handle_stop()
{
	debug->notification(1, type, "--[%s:%i] disconnected", remoteIp.c_str(), remotePort);
	system::error_code ec;
	if(ping_timer)
	{
		ping_timer->cancel(ec);
		delete ping_timer;
	}
	if(memcheck_timer)
	{
		memcheck_timer->cancel(ec);
		delete memcheck_timer;
	}

	fw->subtractConnection(type);
	if(server)
		delete server;
	if(client)
		delete client;
}

void TcpConnectionSSL::handle_one_write(const system::error_code& error)
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
						boost::bind(&TcpConnectionSSL::handle_one_write, shared_from_this(), asio::placeholders::error));
		}
		else
			debug->notification(5, type, "handle_one_write() - one-way packet sent, getting out of scope...");
	}
	else
		debug->warning(1, type, "handle_one_write() - [%s:%i] - error message: %s - error code: %i", remoteIp.c_str(), remotePort, error.message().c_str(), error.value());
}

void TcpConnectionSSL::handle_ping(const system::error_code& error)
{
	if(!error)
	{
		if(state != DISCONNECT)
		{
			// the ping packet doesn't actually do anything and was just implemented in an attempt to replicate the original traffic
			Packet* sendPacket = new Packet("fsys", 0x80000000, false);
			sendPacket->SetVar("TXN", "Ping");
			specialQueue.push_back(sendPacket);

			if(specialQueue.size() > 1)	//if there is already a packet in the queue we just add this packet to the queue, sooner or later it should be processed
				debug->notification(4, type, "handle_ping() - there is more than one packet in the queue, %i", specialQueue.size());
			else
			{
				if(special_data)
					debug->warning(1, type, "We are overwriting some packet content here!!! -> %s", special_data);
				special_data = PacketToData(specialQueue.front(), true);
				async_write(socket_, asio::buffer(special_data, special_data_length),
							boost::bind(&TcpConnectionSSL::handle_one_write, shared_from_this(), asio::placeholders::error));
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

void TcpConnectionSSL::handle_memcheck(const system::error_code& error)
{
	if(!error)
	{
		if(state != DISCONNECT)
		{
			// the memcheck packet doesn't actually do anything and was just implemented in an attempt to replicate the original traffic
			string salt = fw->randomString(9, SEED_NUMBERS);
			Packet* sendPacket = new Packet("fsys", 0x80000000, false);
			sendPacket->SetVar("TXN", "MemCheck");
			sendPacket->SetVar("memcheck.[]", "0");
			sendPacket->SetVar("type", "0");
			sendPacket->SetVar("salt", salt);
			specialQueue.push_back(sendPacket);

			if(specialQueue.size() > 1)	//if there is already a packet in the queue we just add this packet to the queue, sooner or later it should be processed
				debug->notification(4, type, "handle_memcheck() - there is more than one packet in the queue, %i", specialQueue.size());
			else
			{
				if(special_data)
					debug->warning(1, type, "We are overwriting some packet content here!!! -> %s", special_data);
				special_data = PacketToData(specialQueue.front(), true);
				async_write(socket_, asio::buffer(special_data, special_data_length),
							boost::bind(&TcpConnectionSSL::handle_one_write, shared_from_this(), asio::placeholders::error));
			}
		}
	}
	else
	{
		if(type > UNKNOWN || type < DEBUG)		//this happens when the connection is already closed
			type = DEBUG;
		debug->warning(4, type, "handle_memcheck() - error: %s - error code: %i", error.message().c_str(), error.value());
	}
}


//////////////////
// Packet stuff //
//////////////////
void TcpConnectionSSL::StoreIncomingData()
{
	if(incomingQueue.empty())
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
				debug->warning(4, this->type, "StoreIncomingData() - we have multiple packets incoming at once");
		}while(current_length != received_length);
	}
	if(incomingQueue.empty())
		debug->warning(2, type, "StoreIncomingData() - incoming Queue has no content!");
}

char* TcpConnectionSSL::PacketToData(Packet* packet, bool is_special)
{
	string packetData = packet->GetData();
	if(packetData.size() > 8096 && !packet->isEncoded())	// packet exceeds max size, base64 encode it and send it in chunks (can only happen in ssl connection?)
	{
		Packet* sendPacket = NULL;
		bool first = true;
		packetData.pop_back();	// delete last newline (safe to do since it should be always there in a self-created packet)

		size_t pos = 0, decoded_size = packetData.size();
		packetData = base64_encode(reinterpret_cast<const unsigned char*>(packetData.c_str()), decoded_size);
		int encoded_size = packetData.size();

		while((pos = packetData.find('=', packetData.size()-9)) != string::npos)		// bring string into proper format again
			packetData.replace(pos, 1, "%3d");

		// split up the data in multiple packets
		std::list<Packet*> tempList;
		for(int i = 0, new_size = packetData.size(); new_size > 0; i++, new_size -= 8096)
		{
			sendPacket = new Packet(packet->GetType(), 0xb0000000, true, first);
			sendPacket->SetVar("decodedSize", decoded_size);
			sendPacket->SetVar("size", encoded_size);

			string buffer;
			if (new_size < 8096)
				buffer = packetData.substr(i*8096, new_size);
			else
				buffer = packetData.substr(i*8096, 8096);

			sendPacket->SetVar("data", buffer);
			sendPacket->isEncoded(true);
			tempList.push_back(sendPacket);
			if(first)
				first = false;
		}

		// this is only for logging and not actually important
		uint32_t packet_type = (packet->GetType2() & 0xff000000) | (packetCounter+1);
		debug->notification(4, this->type, "=>[%s:%i] %s 0x%08x {%s}", remoteIp.c_str(), remotePort, packet->GetType(), packet_type, packet->toString().c_str());

		// get rid of the original uncoded packet
		delete outgoingQueue.front();
		outgoingQueue.pop_front();
		packetData.clear();

		// we want this series of packets at the beginning of outgoingQueue but in order to do that we have to reverse their order
		while(!tempList.empty())
		{
			outgoingQueue.push_front(tempList.back());
			tempList.pop_back();
		}
		return PacketToData(outgoingQueue.front());	// call the packet processing function again with the first of the split packets
	}

	uint8_t *type1	= (uint8_t*)packet->GetType();
	uint32_t type2	= (uint32_t)packet->GetType2();
	uint8_t *fmt	= (uint8_t*)packetData.c_str();

	static uint8_t *buff = NULL;
	int slen, len;
	len = strlen((char*)fmt);
	slen = 12 + len;

	if(slen > 0)
		buff = (uint8_t*)realloc(buff, slen);

	memcpy(buff, type1, 4);

	if(packet->GetValCount() && (type2 & 0x80000000) == 0x80000000) {	// its a regular packet, do we want to count it?
		if((type2 & 0x00ffffff) == 1)
			packetCounter = 0;					// is the packet the first one we are recieving?
		if(packet->GetValFirst())
			packetCounter++;					// packet counter (not needed for linked packets so we stop counting after first one is sent)
		type2 = (type2 & 0xff000000) | packetCounter;
	}
	packet->SetType2((unsigned int)type2);	//update the packet so it is shown correctly in the log (the data that we send here is not affected by this)

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

unsigned int TcpConnectionSSL::decode(unsigned char* data, int bytes)
{
	int num, i;
	for(num = i = 0; i < bytes; i++) {
		//num |= (data[i] << (i << 3)); // little
		num |= (data[i] << ((bytes - 1 - i) << 3)); // big
	}
	return(num);
}

unsigned int TcpConnectionSSL::encode(uint8_t *data, uint32_t num, int bytes)
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
string TcpConnectionSSL::getLocalIp()
{
	return socket().local_endpoint().address().to_string();
}
int TcpConnectionSSL::getLocalPort()
{
	return socket().local_endpoint().port();
}

string TcpConnectionSSL::getRemoteIp()
{
	return socket().remote_endpoint().address().to_string();
}
int TcpConnectionSSL::getRemotePort()
{
	return socket().remote_endpoint().port();
}


/////////////////////////////
// Packet processing stuff //
/////////////////////////////
void TcpConnectionSSL::ProcessPacket(Packet* receivedPacket)
{
	string txn = receivedPacket->GetVar("TXN");
	if(!txn.empty())
	{
		bool unknown = false;
		char* packet_type = receivedPacket->GetType();

		do{
//fsys//
			if(strcmp(packet_type, "fsys") == 0)
			{
				Packet* sendPacket;
				if(txn.compare("Hello") == 0)
				{
					sendPacket = new Packet(packet_type, 0x80000000);
					sendPacket->SetVar("domainPartition.domain", "eagames");
					//sendPacket->SetVar("messengerIp", "messaging.ea.com");
					//sendPacket->SetVar("messengerPort", "13505");
					sendPacket->SetVar("messengerIp", emuIp);
					sendPacket->SetVar("messengerPort", fw->portsCfg().emulator_port);
					sendPacket->SetVar("domainPartition.subDomain", "BFBC2");
					sendPacket->SetVar("TXN", txn);
					sendPacket->SetVar("activityTimeoutSecs", "0");	// we could let idle clients disconnect here automatically?
					sendPacket->SetVar("curTime", fw->getTime());
					sendPacket->SetVar("theaterIp", emuIp);
					if(this->type == GAME_SERVER_SSL)
					{
						//sendPacket->SetVar("theaterIp", "bfbc2-pc-server.theater.ea.com");
						//sendPacket->SetVar("theaterPort", 19026);
						sendPacket->SetVar("theaterPort", fw->portsCfg().theater_server_port);
						server = new GameServer(this->type, this->db);
					}
					else if(this->type == GAME_CLIENT_SSL)
					{
						//sendPacket->SetVar("theaterIp", "bfbc2-pc.theater.ea.com");
						//sendPacket->SetVar("theaterPort", 18395);
						sendPacket->SetVar("theaterPort", fw->portsCfg().theater_client_port);
						client = new GameClient(this->type, this->db, this->emuIp, false);	// last parameter has no meaning here
					}
					outgoingQueue.push_back(sendPacket);

					string salt = fw->randomString(9, SEED_NUMBERS);
					sendPacket = new Packet("fsys", 0x80000000, false);
					sendPacket->SetVar("TXN", "MemCheck");
					sendPacket->SetVar("memcheck.[]", "0");
					sendPacket->SetVar("type", "0");
					sendPacket->SetVar("salt", salt);
					outgoingQueue.push_back(sendPacket);
				}
				else if(txn.compare("MemCheck") == 0)
				{
					if(!ping_timer && !memcheck_timer)	// activate both ping and memcheck timers when we receive this
					{
						ping_timer = new asio::deadline_timer(socket_.get_io_service());//, posix_time::seconds(150)); // only create the object here, it'll be updated below anyway
						memcheck_timer = new asio::deadline_timer(socket_.get_io_service(), posix_time::seconds(500));
						memcheck_timer->async_wait(boost::bind(&TcpConnectionSSL::handle_memcheck, shared_from_this(), asio::placeholders::error));
					}
					else	// update memcheck timer (was approximately 300 seconds in captured traffics)
					{
						memcheck_timer->expires_from_now(posix_time::seconds(300));
						memcheck_timer->async_wait(boost::bind(&TcpConnectionSSL::handle_memcheck, shared_from_this(), asio::placeholders::error));
					}
					state = SKIP;
				}
				else if(txn.compare("Ping") == 0)
				{
					// should we do this when its done below for practically every packet we receive anyway?
					//ping_timer->expires_from_now(posix_time::seconds(150));
					//ping->async_wait(boost::bind(&TcpConnectionSSL::handle_ping, this, asio::placeholders::error));
					state = SKIP;
				}
				else if(txn.compare("GetPingSites") == 0)
				{
					//if everything goes well we should never get here (on the server side)
					sendPacket = new Packet(packet_type, 0x80000000);
					sendPacket->SetVar("TXN", txn);
					sendPacket->SetVar("pingSite.[]", "4");
					sendPacket->SetVar("pingSite.0.addr", emuIp);
					sendPacket->SetVar("pingSite.0.type", "0");
					sendPacket->SetVar("pingSite.0.name", "gva");
					sendPacket->SetVar("pingSite.1.addr", emuIp);
					sendPacket->SetVar("pingSite.1.type", "0");
					sendPacket->SetVar("pingSite.1.name", "nrt");
					sendPacket->SetVar("pingSite.2.addr", emuIp);
					sendPacket->SetVar("pingSite.2.type", "0");
					sendPacket->SetVar("pingSite.2.name", "iad");
					sendPacket->SetVar("pingSite.3.addr", emuIp);
					sendPacket->SetVar("pingSite.3.type", "0");
					sendPacket->SetVar("pingSite.3.name", "sjc");
					sendPacket->SetVar("minPingSitesToPing", "0");
					outgoingQueue.push_back(sendPacket);
				}
				else if(txn.compare("Goodbye") == 0)
				{
					state = DISCONNECT;
				}
				else
					unknown = true;
			}
//acct//
			else if(strcmp(packet_type, "acct") == 0)
			{
				if(server)
					unknown = server->acct(&outgoingQueue, receivedPacket, txn);
				else // must be client
					unknown = client->acct(&outgoingQueue, receivedPacket, txn, remoteIp.c_str());
			}
//asso//
			else if(strcmp(packet_type, "asso") == 0)
			{
				if(server)
					unknown = server->asso(&outgoingQueue, receivedPacket, txn);
				else // must be client
					unknown = client->asso(&outgoingQueue, receivedPacket, txn);
			}
//xmsg// Client only?
			else if(strcmp(packet_type, "xmsg") == 0)
			{
				if(client)
					unknown = client->xmsg(&outgoingQueue, receivedPacket, txn);
				else
					debug->warning(2, type, "xmsg - unexpected msg for server");
			}
//pres
			else if(strcmp(packet_type, "pres") == 0)
			{
				if(server)
					unknown = server->pres(&outgoingQueue, receivedPacket, txn);
				else // must be client
					unknown = client->pres(&outgoingQueue, receivedPacket, txn);
			}
//rank//
			else if(strcmp(packet_type, "rank") == 0)
			{
				if(server)
					state = server->rank(&outgoingQueue, receivedPacket, txn);
				else // must be client
					state = client->rank(&outgoingQueue, receivedPacket, txn);
			}
//recp// Client only?
			else if(strcmp(packet_type, "recp") == 0)
			{
				if(client)
					unknown = client->recp(&outgoingQueue, receivedPacket, txn);
				else
					debug->warning(2, type, "recp - unexpected packet for server");
			}
//pnow// Client only?
			else if(strcmp(packet_type, "pnow") == 0)
			{
				if(client)
					unknown = client->pnow(&outgoingQueue, receivedPacket, txn);
				else
					debug->warning(2, type, "pnow - unexpected packet for server");
			}
//fltr// Server only?
			else if(strcmp(packet_type, "fltr") == 0)
			{
				if(server)
					unknown = server->fltr(&outgoingQueue, receivedPacket, txn);
				else
					debug->warning(2, type, "fltr - unexpected packet for client");
			}
			else
				unknown = true;
		}while(state == QUEUE);

		if(ping_timer && incomingQueue.size() <= 1)		// all packets are processed, update ping timer (would it be better to just remove the queue check?)
		{
			int duration = ping_timer->expires_from_now().total_seconds();

			if(duration < 0 || duration > 150)
				duration = 150;
			else if(duration >= 0 && duration+10 < 150)		// reset the timer
				duration = 150 + (duration % 10);	// ping intervals seem to be approximately 150 seconds (why did I think it was a good idea to do it like this?)

			ping_timer->expires_from_now(posix_time::seconds(duration));
			ping_timer->async_wait(boost::bind(&TcpConnectionSSL::handle_ping, shared_from_this(), asio::placeholders::error));
		}

		if(unknown)
		{
			debug->warning(1, type, "Unknown packet! Setting state to DISCONNECT");
			state = DISCONNECT;		// should we really be that restrictive?
		}
	}
	else	// no TXN was included in this packet so this one has to be part of a large base64 encoded packet?
	{
		string buffer = receivedPacket->GetVar("size");
		string data = receivedPacket->GetVar("data");
		if(!buffer.empty() && !data.empty())
		{
			// is it safe to assume that the encoded packets are ordered and that there won't be different encoded packets exchanged at the same time?
			size_t size = lexical_cast<int>(buffer);
			encoded_data.append(receivedPacket->GetVar("data"));

			if(encoded_data.size() >= size)		// all data should be received, decode it
			{
				if(!self_created_packet)
					self_created_packet = true;

				size_t pos = encoded_data.size()-9;
				while((pos = encoded_data.find("%3d", pos)) != string::npos)	// see if there are any '=' in string and convert it back
				{
					encoded_data.replace(pos, 3, 1, '=');
					pos = encoded_data.size()-6;
				}

				string decoded_data = base64_decode(encoded_data);
				Packet* packet = new Packet(receivedPacket->GetType(), receivedPacket->GetType2(), decoded_data.size(), decoded_data.c_str());

				delete receivedPacket;		// first we have to pop the last packet we got (which is the last of the series of encoded packets)
				incomingQueue.pop_front();

				//we dont want this to be added at the back because there could be another packet that we received between the gathering and therefore messing up the packet_count
				incomingQueue.push_front(packet);
				debug->notification(4, this->type, "<=[%s:%i] %s 0x%08x {%s}", remoteIp.c_str(), remotePort, packet->GetType(), packet->GetType2(), packet->toString().c_str());
				encoded_data.clear();	// reset storage for data
			}
			else
				state = SKIP;
		}
		else
			debug->warning(2, type, "Unknown packet here!");
	}
}
