#include "HttpServer.h"
#include "../Framework/Logger.h"

extern Logger* debug;

HttpServer::HttpServer(asio::io_service& io_service, bool updater_respond) : acceptor_(io_service), new_connection_(), request_handler_("./templates/")
{
	tcp::resolver resolver(io_service);
	tcp::resolver::query query("0.0.0.0", "80");
	tcp::endpoint endpoint = *resolver.resolve(query);

	acceptor_.open(endpoint.protocol());
	acceptor_.set_option(tcp::acceptor::reuse_address(true));
	acceptor_.bind(endpoint);
	acceptor_.listen();

	this->updater_respond = updater_respond;
	start_accept();
	debug->notification(1, HTTP, "Created HTTP Socket (listening on port 80)");
}

void HttpServer::start_accept()
{
	new_connection_.reset(new HttpConnection(acceptor_.get_io_service(), request_handler_, updater_respond));
	acceptor_.async_accept(new_connection_->socket(),
							boost::bind(&HttpServer::handle_accept, this, asio::placeholders::error));
}

void HttpServer::handle_accept(const boost::system::error_code& e)
{
	if (!e)
		new_connection_->start();

	start_accept();
}
