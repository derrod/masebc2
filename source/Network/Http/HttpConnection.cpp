#include "HttpConnection.h"
#include "../../Framework/Logger.h"

extern Logger* debug;

HttpConnection::HttpConnection(asio::io_service& io_service, RequestHandler& handler, bool updater_respond) :
				strand_(io_service), socket_(io_service), request_handler_(handler)
{
	this->updater_respond = updater_respond;
}

tcp::socket& HttpConnection::socket()
{
	return socket_;
}

void HttpConnection::start()
{
	socket_.async_read_some(asio::buffer(buffer_), strand_.wrap(
		boost::bind(&HttpConnection::handle_read, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred)));
}

void HttpConnection::handle_read(const boost::system::error_code& e, std::size_t bytes_transferred)
{
	if(!e)
	{
		boost::tribool result;
		boost::tie(result, boost::tuples::ignore) = request_parser_.parse(request_, buffer_.data(), buffer_.data() + bytes_transferred);
		debug->notification(3, HTTP, "<-[%s:%i] %s { %s }", getRemoteIp().c_str(), getRemotePort(), request_.method.c_str(), request_.uri.c_str());
		if(result)
		{
			request_handler_.handle_request(request_, reply_, updater_respond);
			boost::asio::async_write(socket_, reply_.to_buffers(), strand_.wrap(
				boost::bind(&HttpConnection::handle_write, shared_from_this(), asio::placeholders::error)));
		}
		else if(!result)
		{
			reply_ = reply::stock_reply(reply::bad_request);
			boost::asio::async_write(socket_, reply_.to_buffers(), strand_.wrap(
				boost::bind(&HttpConnection::handle_write, shared_from_this(), asio::placeholders::error)));
		}
		else
		{
			socket_.async_read_some(asio::buffer(buffer_), strand_.wrap(
				boost::bind(&HttpConnection::handle_read, shared_from_this(), asio::placeholders::error, asio::placeholders::bytes_transferred)));
		}
	}
	else
		debug->warning(3, HTTP, e.message().c_str());

	// If an error occurs then no new asynchronous operations are started. This
	// means that all shared_ptr references to the connection object will
	// disappear and the object will be destroyed automatically after this
	// handler returns. The connection class's destructor closes the socket.
}

void HttpConnection::handle_write(const boost::system::error_code& e)
{
	if(!e)
	{
		string content = reply_.content.c_str();
		size_t pos = 0;
		while((pos = content.find('\n', pos)) != string::npos)
			content.erase(pos, 1);
		pos = 0;
		while((pos = content.find('\r', pos)) != string::npos)	// many carriage returns are in the installConfig
			content.erase(pos, 1);

		debug->notification(3, HTTP, "->[%s:%i] %i { %s }", getRemoteIp().c_str(), getRemotePort(), reply_.status, content.c_str());
		// Initiate graceful connection closure.
		boost::system::error_code ignored_ec;
		socket_.shutdown(tcp::socket::shutdown_both, ignored_ec);
	}
	else
		debug->warning(3, HTTP, e.message().c_str());

	// No new asynchronous operations are started. This means that all shared_ptr
	// references to the connection object will disappear and the object will be
	// destroyed automatically after this handler returns. The connection class's
	// destructor closes the socket.
}

string HttpConnection::getLocalIp()
{
	return socket().local_endpoint().address().to_string();
}
int HttpConnection::getLocalPort()
{
	return socket().local_endpoint().port();
}

string HttpConnection::getRemoteIp()
{
	return socket().remote_endpoint().address().to_string();
}
int HttpConnection::getRemotePort()
{
	return socket().remote_endpoint().port();
}
