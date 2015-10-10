#pragma once
#include "../main.h"
#include <vector>
#ifndef _WIN32
	#include <list> // linux
#endif
#include "Logger.h"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
using namespace boost::multi_index;

//////////////////////////////////////////
// Database tables represented as enums //
//////////////////////////////////////////

// lobbies
enum {
	LOBBY_ID,
	LOBBY_NAME,
	LOBBY_LOCALE,
	//we get num_games from the database object, lobby should remain constant
	LOBBY_MAX_GAMES,

	LOBBY_SIZE
};

// games
enum {
	//ID,
	//LOBBY_ID,		//make it constant
	//PERSONA_ID,	//make it constant (only determine platform of server)
	//PERSONA_NAME,	//make it constant (only determine platform of server)

	IP,
	PORT,
	INTERNAL_IP,
	INTERNAL_PORT,

	SERVER_NAME,
	SERVER_LEVEL,
	ACTIVE_PLAYERS,
	MAX_PLAYERS,
	QUEUE_LENGTH,
	JOINING_PLAYERS,

	GAMEMODE,
	SOFTCORE,
	HARDCORE,
	HAS_PASSWORD,
	EA,
	GAME_MOD,
	TIME,

	PUNKBUSTER,
	PUNKBUSTER_VERSION,
	PLATFORM,
	REGION,
	VERSION,
	CLIENT_VERSION,

	IS_PUBLIC,
	ELO,
	NUM_OBSERVERS,
	MAX_OBSERVERS,

	SGUID,
	HASH,
	PROVIDER,
	UGID,
	TYPE,
	JOIN,
	//GAME_ONLINE,		//no longer needed if we ensure that only active games get filtered

	GAMES_SIZE
};
const string games_text[GAMES_SIZE] =
{
	"ip", "port", "int_ip", "int_port", "name", "level", "active_players", "max_players", "queue_length", "joining_players",
	"game_mode", "softcore", "hardcore", "has_password", "ea", "game_mod", "time", "punkbuster", "punkbuster_version", "platform",
	"region", "version", "client_version", "public", "elo", "num_observers", "max_observers", "sguid", "hash", "provider",
	"ugid", "type", "join"
};

// gdet
enum {
	//GAME_ID,	// not needed if we store the server values in class objects
	//LOBBY_ID,	// make it constant
	AUTOBALANCE,
	BANNERURL,
	CROSSHAIR,
	FRIENDLY_FIRE,
	KILLCAM,
	MINIMAP,
	MINIMAP_SPOTTING,
	SERVER_DESCRIPTION,
	THIRD_PERSON_VEHICLE_CAMERAS,
	THREE_D_SPOTTING,

	PDAT00,
	PDAT01,
	PDAT02,
	PDAT03,
	PDAT04,
	PDAT05,
	PDAT06,
	PDAT07,
	PDAT08,
	PDAT09,
	PDAT10,
	PDAT11,
	PDAT12,
	PDAT13,
	PDAT14,
	PDAT15,
	PDAT16,
	PDAT17,
	PDAT18,
	PDAT19,
	PDAT20,
	PDAT21,
	PDAT22,
	PDAT23,
	PDAT24,
	PDAT25,
	PDAT26,
	PDAT27,
	PDAT28,
	PDAT29,
	PDAT30,
	PDAT31,

	GDET_SIZE
};
const string gdet_text[GDET_SIZE] =
{
	"auto_balance", "banner_url", "crosshair", "friendly_fire", "killcam", "minimap", "minimap_spotting", "server_description", "3rd_person_vehicle_cam", "3d_spotting",
	"pdat00", "pdat01", "pdat02", "pdat03", "pdat04", "pdat05", "pdat06", "pdat07", "pdat08", "pdat09", "pdat10", "pdat11", "pdat12", "pdat13", "pdat14", "pdat15",
	"pdat16", "pdat17", "pdat18", "pdat19", "pdat20", "pdat21", "pdat22", "pdat23", "pdat24", "pdat25", "pdat26", "pdat27", "pdat28", "pdat29", "pdat30", "pdat31"
};

// personas
enum{
	//ID,			//is represented as index number in vector
	//PERSONA_NAME,	//is unique and seperated from rest of the data
	USER_ID,
	USER_EMAIL,
	PLAYER_ID,		//save playerid as class object (init with 0, set it as needed during connection)
	//LKEY,			//save lkey as class object
	PERSONA_ONLINE,

	PERSONA_SIZE
};
const string personas_text[PERSONA_SIZE] = {"user_id", "user_email", "player_id", "online"};

// users
enum{
	//ID,			//is represented as index number in vector
	//EMAIL,		//is unique and seperated from rest of the data
	PASSWORD,
	//LKEY,			//save lkey as class object
	COUNTRY,
	BIRTHDAY,
	USER_ONLINE,

	USER_SIZE
};
const string users_text[USER_SIZE] = {"password", "country", "birthday", "online"};

// types
enum{
	USERS,
	PERSONAS,
	GAMES
};

struct list_entry{
	int id;
	string name;
	string* data;
};
struct linked_key{
	int id;
	string key;
};
struct GamesEntry{
	string* key;	//[GAMES_SIZE]
	string* gdet;	//[GDET_SIZE]
};

typedef multi_index_container<list_entry,
	indexed_by<
		ordered_unique< tag<int>, BOOST_MULTI_INDEX_MEMBER(list_entry, int, id)>,				//id
		ordered_unique< tag<string>, BOOST_MULTI_INDEX_MEMBER(list_entry, string, name)>,		//name
		ordered_non_unique< tag<string*>, BOOST_MULTI_INDEX_MEMBER(list_entry, string*, data)>	//data
	>
>table;

class Database
{
	private:
		string personasData, usersData;
		vector<GamesEntry> games;
		list<int> availableGameSlots;

		int lobby_num_games;	//this is not games.size() since there could be free entries, this is the real number of games
		string* lobby;			//we only have one lobby and we keep its values constant

		table personas, users;
		typedef table::index<int>::type list_id;
		typedef table::index<string>::type list_string;
		list<int> availableUserSlots;
		list<int> availablePersonaSlots;
		int highestUserId, highestPersonaId;

		bool extended_info;
		void initializeData();
		list<linked_key> lkeys_assigned;
		bool loadFileData(const char* file, int struct_type);
		bool saveFileData(string file, string data);

	public:
		Database(bool extended_info);
		~Database();
		void saveDatabase();

		int addUser(list_entry user);			// no need to return id, just return if successful
		bool getUser(int id, list_entry* user);
		bool getUser(string name, list_entry* user);

		int addPersona(list_entry persona);		// no need to return id, just return if successful
		void removePersona(string name);
		bool getPersona(int id, list_entry* persona);
		bool getPersona(string name, list_entry* persona);

		int addGame(GamesEntry game);
		void removeGame(int id);
		bool isValidGid(int id);
		string* getGameData(int id, bool gdet);

		void personaLogin(linked_key persona);
		int theaterLogin(string lkey);

		//will we ever need these?
		/*bool setUserData(int id, int index, string data);
		bool setUserData(string name, int index, string data);
		bool setPersonaData(int id, int index, string data);
		bool setPersonaData(string name, int index, string data);
		bool setGameData(int id, int index, string data, bool gdet);*/

		void listMatchingPersonas(int index, string searchTerm, list<list_entry>* filtered);
		void listMatchingGames(list<linked_key>* filters, list<int>* filtered, bool returnFirst = false);
		void listAllGames(list<int>* allGames);

		string gameToString(int id, const char* state, GamesEntry* game, bool gdet);	//dont really need id but its nice to be able to log it (don't want to use games list too much)
		string listEntryToString(list_entry* entry, const char* state, bool user);

		string getLobbyInfo(int index);
		int getLobbyGames();
};
