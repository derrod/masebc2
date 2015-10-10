#pragma once
#include "../main.h"
#include "../Network/Packet.h"
#include "Database.h"

const string ServerCoreInfo[GAMES_SIZE] =
{
	"IP",
	"PORT",
	"INT-IP",
	"INT-PORT",

	"NAME",
	"B-U-level",
	"ACTIVE_PLAYERS",		//there is never a packet that indicates what name this has
	"MAX-PLAYERS",
	"B-U-QueueLength",		//what about packet->GetVar("QLEN")?
	"JOINING_PLAYERS",		//there is never a packet that indicates what name this has

	"B-U-gamemode",
	"B-U-Softcore",
	"B-U-Hardcore",
	"B-U-HasPassword",
	"B-U-EA",
	"B-U-gameMod",
	"B-U-Time",

	"B-U-Punkbuster",
	"B-U-PunkBusterVersion",
	"PLAT",				//not sure if this is the right name but this is never update anyway
	"B-U-region",
	"B-version",
	"VERS",				//not sure if this is the right name but this is never update anyway

	"B-U-public",
	"B-U-elo",
	"B-numObservers",
	"B-maxObservers",

	"B-U-sguid",
	"B-U-hash",
	"B-U-Provider",
	"UGID",
	"TYPE",
	"JOIN"
};

const string ServerGdetInfo[GDET_SIZE] =
{
	"D-AutoBalance",			//AUTOBALANCE,
	"D-BannerUrl",				//BANNERURL,
	"D-Crosshair",				//CROSSHAIR,
	"D-FriendlyFire",			//FRIENDLY_FIRE,
	"D-KillCam",				//KILLCAM,
	"D-Minimap",				//MINIMAP,
	"D-MinimapSpotting",		//MINIMAP_SPOTTING,
	"D-ServerDescriptionCount",		//SERVER_DESCRIPTION,	(we temporarily save the description count here so we can get the whole description later)
	"D-ThirdPersonVehicleCameras",	//THIRD_PERSON_VEHICLE_CAMERAS,
	"D-ThreeDSpotting",			//THREE_D_SPOTTING,

	"D-pdat00",		//PDAT00,
	"D-pdat01",		//PDAT01,
	"D-pdat02",		//PDAT02,
	"D-pdat03",		//PDAT03,
	"D-pdat04",		//PDAT04,
	"D-pdat05",		//PDAT05,
	"D-pdat06",		//PDAT06,
	"D-pdat07",		//PDAT07,
	"D-pdat08",		//PDAT08,
	"D-pdat09",		//PDAT09,
	"D-pdat10",		//PDAT10,
	"D-pdat11",		//PDAT11,
	"D-pdat12",		//PDAT12,
	"D-pdat13",		//PDAT13,
	"D-pdat14",		//PDAT14,
	"D-pdat15",		//PDAT15,
	"D-pdat16",		//PDAT16,
	"D-pdat17",		//PDAT17,
	"D-pdat18",		//PDAT18,
	"D-pdat19",		//PDAT19,
	"D-pdat20",		//PDAT20,
	"D-pdat21",		//PDAT21,
	"D-pdat22",		//PDAT22,
	"D-pdat23",		//PDAT23,
	"D-pdat24",		//PDAT24,
	"D-pdat25",		//PDAT25,
	"D-pdat26",		//PDAT26,
	"D-pdat27",		//PDAT27,
	"D-pdat28",		//PDAT28,
	"D-pdat29",		//PDAT29,
	"D-pdat30",		//PDAT30,
	"D-pdat31"		//PDAT31
};

class GameServer
{
	private:
		int connectionType;
		Database* database;
		bool logDatabase;
		list_entry user;
		list_entry persona;
		GamesEntry game;

		//serverInfoPlasma->
		int		sock_id;
		string	user_lkey;
		string	persona_lkey;
		//<-serverInfoPlasma

		//serverInfoTheater->
		int		sock_tid;
		string	clientVersion;
		string	platform;
		int		gid;
		int		player_count;
		//<-serverInfoTheater
		string	secret;

	public:
		GameServer(int connectionType, Database* db);
		~GameServer();
		bool acct(std::list<Packet*>* queue, Packet* packet, string txn);
		bool asso(std::list<Packet*>* queue, Packet* packet, string txn);
		int rank(std::list<Packet*>* queue, Packet* packet, string txn);
		bool pres(std::list<Packet*>* queue, Packet* packet, string txn);
		bool fltr(std::list<Packet*>* queue, Packet* packet, string txn);
		//do we need pres here?

		int ProcessTheater(std::list<Packet*>* queue, Packet* packet, char* type, const char* ip);
		int addPlayerId();	//returns player_id
};
