#include "../main.h"
#include "Packet.h"

using asio::ip::udp;

class UdpServer
{
	public:
		UdpServer(asio::io_service& io_service, int type, int port);

	private:
		udp::socket socket_;
		udp::endpoint remote_endpoint_;

		int type;
		int port;
		enum { max_length = 1024 };
		unsigned char received_data[max_length];
		std::string send_data;

		void start_receive();
		void handle_receive(const system::error_code& error, size_t bytes_transferred);
		void handle_send(const system::error_code& /*error*/, size_t /*bytes_transferred*/);
		void handle_stop();

		void ProcessData(Packet* packet);
		Packet* DataToPacket(size_t bytes_transferred);

		std::string PacketToData(Packet* packet);
		unsigned int decode(unsigned char* data, int bytes);

		std::string getLocalIp();
		int getLocalPort();
		std::string getRemoteIp();
		int getRemotePort();
};
