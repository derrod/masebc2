#include "../../main.h"
#include <boost/array.hpp>
#include "Reply.h"
#include "RequestHandler.h"
#include "RequestParser.h"
using asio::ip::tcp;

/// Represents a single connection from a client.
class HttpConnection : public boost::enable_shared_from_this<HttpConnection>, private boost::noncopyable
{
	public:
		/// Construct a connection with the given io_service.
		explicit HttpConnection(asio::io_service& io_service, RequestHandler& handler, bool updater_respond);

		/// Get the socket associated with the connection.
		tcp::socket& socket();

		/// Start the first asynchronous operation for the connection.
		void start();

	private:
		/// Handle completion of a read operation.
		void handle_read(const boost::system::error_code& e,
		std::size_t bytes_transferred);

		/// Handle completion of a write operation.
		void handle_write(const boost::system::error_code& e);

		/// Strand to ensure the connection's handlers are not called concurrently.
		asio::io_service::strand strand_;

		/// Socket for the connection.
		asio::ip::tcp::socket socket_;

		/// The handler used to process the incoming request.
		RequestHandler& request_handler_;

		/// Buffer for incoming data.
		boost::array<char, 8192> buffer_;

		/// The incoming request.
		request request_;

		/// The parser for the incoming request.
		RequestParser request_parser_;

		/// The reply to be sent back to the client.
		reply reply_;

		bool updater_respond;
		string getLocalIp();
		int getLocalPort();
		string getRemoteIp();
		int getRemotePort();
};

typedef boost::shared_ptr<HttpConnection> http_connection_ptr;
