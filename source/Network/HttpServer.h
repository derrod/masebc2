#include "../main.h"
#include "Http/HttpConnection.h"
#include "Http/RequestHandler.h"

class HttpServer : private noncopyable
{
	public:
		HttpServer(asio::io_service& io_service, bool updater_respond);
		void handle_stop();

	private:
		tcp::acceptor acceptor_;
		bool updater_respond;

		/// The next connection to be accepted.
		http_connection_ptr new_connection_;
		/// The handler for all incoming requests.
		RequestHandler request_handler_;

		void start_accept();
		void handle_accept(const system::error_code& error);
};
