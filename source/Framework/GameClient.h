#pragma once
#include "../main.h"
#include "../Network/Packet.h"
#include "Stats.h"
#include "Database.h"

class GameClient
{
	private:
		int connectionType;
		Database* database;
		bool logDatabase;
		list_entry persona;
		list_entry user;

		//clientInfoPlasma->
		string	user_lkey;
		string	persona_lkey;
		Stats*	stats_data;
		//<-clientInfoPlasma

		//clientInfoTheater->
		int		sock_tid;
		int		queue_position;
		int		original_queue_length;
		//<-clientInfoTheater

		bool isLocal;	// is the client in the same network as the emulator?
		string emuIp;
		bool alreadyLoggedIn;
		list<string> playerList;
		void filterGames(Packet* packet, list<int>* filtered);

	public:
		GameClient(int connectionType, Database* db, string emuIp, bool isLocal);
		~GameClient();

		bool acct(list<Packet*>* queue, Packet* packet, string txn, const char* ip);
		bool asso(list<Packet*>* queue, Packet* packet, string txn);
		bool xmsg(list<Packet*>* queue, Packet* packet, string txn);
		bool pres(list<Packet*>* queue, Packet* packet, string txn);
		bool recp(list<Packet*>* queue, Packet* packet, string txn);
		bool pnow(list<Packet*>* queue, Packet* packet, string txn);
		int rank(list<Packet*>* queue, Packet* packet, string txn);

		int ProcessTheater(std::list<Packet*>* queue, Packet* packet, char* type, const char* ip);
};
