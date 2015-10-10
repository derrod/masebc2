#pragma once
#include "../main.h"
#include "Packet.h"
#include "../Framework/GameClient.h"
#include "../Framework/GameServer.h"

using asio::ip::tcp;

class TcpConnection;
struct serverConnectionInfo
{
	int id;
	boost::shared_ptr<TcpConnection> socket;
};

class TcpConnection : public boost::enable_shared_from_this<TcpConnection>, private noncopyable
{
	public:
		typedef boost::shared_ptr<TcpConnection> pointer;
		tcp::socket& socket();
		void start();
		void handle_invoke(Packet* sendPacket);		// send a one-way packet
		GameServer* getGameServer();
		TcpConnection(asio::io_service& io_service, int type, Database* db);

	private:
		tcp::socket socket_;

		Database* db;
		int type, state, packetCounter;
		const char* send_data;
		enum { max_length = PACKET_MAX_LENGTH };
		unsigned char received_data[max_length];
		serverConnectionInfo serverJoinable;

		int send_length, received_length;
		std::list<Packet*> incomingQueue;
		std::list<Packet*> outgoingQueue;

		asio::deadline_timer* ping_timer;
		asio::deadline_timer* delayed_timer;
		Packet* delayed_packet;
		const char* special_data;
		int special_data_length;
		std::list<Packet*> specialQueue;

		void handle_read(const system::error_code& error, size_t bytes_transferred);	// normal read (also deals with multiple packets)
		void handle_write(const system::error_code& error);			// normal send (also deals with multiple packets)
		void handle_stop();		// cleanup after a disconnect

		void handle_one_write(const system::error_code& error);		// processes the one-way packet queue (special queue)
		void handle_delayed_send(const system::error_code& error);	// send a one-way delayed packet (dependent on delayed_timer)
		void handle_ping(const system::error_code& error);			// send a one-way ping in a constant interval (dependent on ping_timer)
		void update_ping_timer();

		GameServer* server;
		GameClient* client;

		void ProcessGameServer(Packet* receivedPacket);
		void ProcessGameClient(Packet* receivedPacket);

		void StoreIncomingData();
		char* PacketToData(Packet* packet, bool is_special = false);
		unsigned int decode(unsigned char* data, int bytes);
		unsigned int encode(uint8_t *data, uint32_t num, int bytes);

		// need remoteIp and port stored when error occurs to get it because at that time the socket is already destroyed (linux only?)
		string remoteIp;
		int remotePort;

		string getLocalIp();
		int getLocalPort();
		string getRemoteIp();
		int getRemotePort();
};
