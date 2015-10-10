#pragma once

#if defined (_WIN32)
	#define _CRT_SECURE_NO_WARNINGS		// disable stupid ms warning
	#define WIN32_LEAN_AND_MEAN			// exclude rarely used windows stuff
	#define NOCOMM
	#define _WIN32_WINNT 0x0502	//target WinXP SP2
	//#define WIN_MAIN
#endif

#define PACKET_MAX_LENGTH 8192
#define HEADER_LENGTH 4
#define WINDOW_X 102
#define WINDOW_Y 36

//#define _DEMO_RELEASE

#if defined _DEMO_RELEASE
	#define AGE_LIMIT 18
#else
	#define AGE_LIMIT 0
#endif

enum {
	DEBUG,				// internal stuff
	DATABASE,			// database
	GAME_SERVER,		// Theater
	GAME_SERVER_SSL,	// Plasma
	GAME_CLIENT,		// Theater
	GAME_CLIENT_SSL,	// Plasma
	HTTP,				// Http connection
	MISC,				// Telemetry and Messages
	UNKNOWN				// ???
};

enum {
	NORMAL,			// we receive a packet - we send a packet
	QUEUE,			// we send multiple packets
	SKIP,			// we ignore the packet we get and keep on rreceiving packets (also useful for encoded packets)
	DISCONNECT,		// client got disconnected
	ERROR_STATE		// some error
};

#include <iostream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/lexical_cast.hpp>

using namespace boost;
using namespace std;

struct debugSettings
{
	bool log_create;
	bool log_timestamp;
	int file_notification_level;
	int file_warning_level;
	int console_notification_level;
	int console_warning_level;
	bool display_db_extended;
};

struct connectionSettings
{
	int plasma_client_port;
	int theater_client_port;
	int plasma_server_port;
	int theater_server_port;
	string emulator_ip;
	string emulator_port;
	bool use_http;
	bool updater_respond;
};

struct emulatorSettings
{
	string global_server_version;
	bool enable_server_filters;
	bool all_stats_unlocked;
	bool all_players_are_veterans;
	bool vietnam_for_all;
	bool premium_for_all;
	bool specact_for_all;
};

struct consoleSettings
{
	bool use_color;
	bool use_default_console;
	int buffer_size;
	int message_cut_off_length;
};


struct header
{
	string name;
	string value;
};

/// A request received from a client.
struct request
{
	string method;
	string uri;
	int http_version_major;
	int http_version_minor;
	vector<header> headers;
};
