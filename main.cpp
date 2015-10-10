#include <main.h>
#include <Framework/Config.h>
#include <Framework/Logger.h>
#include <Framework/Database.h>
#include <Framework/Framework.h>
#include <Network/TcpServer.h>
#include <Network/UdpServer.h>
#include <Network/HttpServer.h>

Logger*		debug = NULL;
Framework*	fw = NULL;
Database*	db = NULL;

#ifndef WIN_MAIN
int main(int argc, char* argv[])
#else
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)	// not sure if using WinMain gives any advantage in Windows...
#endif
{
	Config* cfg = new Config();
	if(cfg->loadConfigFile())
	{
		debugSettings dbg = cfg->getDebugCfg();
		consoleSettings con = cfg->getConCfg();
		connectionSettings ports = cfg->getPortsCfg();
		bool logDatabase = (dbg.console_notification_level > 2) || (dbg.file_notification_level > 2);

		debug = new Logger(dbg, con);
		fw = new Framework(cfg->getEmuCfg(), ports, logDatabase);
		fw->checkFiles();
		debug->simpleNotification(4, DEBUG, "Checked all directories and read all the values from the config.ini, continuing with initialization...");

		db = new Database(dbg.display_db_extended);
		try
		{
			asio::io_service io_service;
			fw->resolveEmuIp(&io_service);

			HttpServer* ClientHTTP = NULL;
			if(ports.use_http)
				ClientHTTP = new HttpServer(io_service, ports.updater_respond);

			int misc_port = lexical_cast<int>(ports.emulator_port);
			TcpServer* ClientMisc = NULL;
			if(misc_port > 0)
				ClientMisc = new TcpServer(io_service, MISC, misc_port, NULL);

			TcpServer GameServerSSL(io_service, GAME_SERVER_SSL, ports.plasma_server_port, db);	// Plasma
			TcpServer GameServer(io_service, GAME_SERVER, ports.theater_server_port, db);		// Theater
			UdpServer GameServerUDP(io_service, GAME_SERVER, ports.theater_server_port);		// Theater

			TcpServer GameClientSSL(io_service, GAME_CLIENT_SSL, ports.plasma_client_port, db);	// Plasma
			TcpServer GameClient(io_service, GAME_CLIENT, ports.theater_client_port, db);		// Theater
			UdpServer GameClientUDP(io_service, GAME_CLIENT, ports.theater_client_port);		// Theater

			delete cfg;
			debug->simpleNotification(0, DEBUG, "Finished initialization, ready for receiving incoming connections");
			io_service.run();

			if(ClientHTTP)
				delete ClientHTTP;
			if(ClientMisc)
				delete ClientMisc;
		}
		catch(std::exception& e)
		{
			//std::cerr << e.what() << std::endl;
			debug->error(DEBUG, "Error: %s", e.what());
			cout << "\n\n...Press ENTER to quit..." << endl;
			cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
		}
		delete db;
		delete debug;
		delete fw;
	}
	else
		delete cfg;

	return 0;
}
