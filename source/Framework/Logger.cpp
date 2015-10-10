#include <stdarg.h>
#include "Logger.h"
#ifdef WIN_MAIN
	#include <io.h>
	#include <fcntl.h>
#endif
#include "Framework.h"
#if !defined (_WIN32)
	#include <sys/time.h>	// get accurate time for logger
	#include <signal.h>		// to set up a signal handler
#endif
extern Framework* fw;
extern Database* db;
extern Logger* debug;

#if defined (_WIN32)
// is this the only way to properly save any stuff in Windows when the console is closed via the 'X' button?
// for some reason boost::asio's signal_set wouldn't work with the Win32 console so I guess we're stuck with a platform dependent solution
BOOL WINAPI QuitHandler(DWORD dwCtrlType)
{
	switch(dwCtrlType)
	{
		case CTRL_C_EVENT:
		case CTRL_BREAK_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			{
				if(db)
				{
					db->saveDatabase();
					delete db;
					db = NULL;
				}
				if(fw)
				{
					delete fw;
					fw = NULL;
				}
				if(debug)
				{
					delete debug;
					debug = NULL;
				}
				ExitProcess(dwCtrlType);
			}
	}
	return TRUE;
};

const WORD colors[9][2] =
{
	//notifications have black background, warnings have a dark red background
	{0x07, 0x47},		// DEBUG,			Grey (default)
	{0x0F, 0x4F},		// DATABASE,		White
	{0x0B, 0x4B},		// GAME_SERVER,		Green
	{0x0A, 0x4A},		// GAME_SERVER_SSL,	Turquoise
	{0x0D, 0x4D},		// GAME_CLIENT,		Pink
	{0x0E, 0x4E},		// GAME_CLIENT_SSL,	Yellow
	{0x03, 0x43},		// HTTP,			Aqua
	{0x06, 0x46},		// MISC,			Dark Yellow
	{0x0C, 0x40}		// UNKNOWN,			Red (Black when warning due to background)
};

Logger::Logger(debugSettings dbg, consoleSettings con)
{
	useColor = con.use_color;
	useNormalConsole = con.use_default_console;

	lastColor = 0x00;
	currentScreenBuffer.X = WINDOW_X;
	currentScreenBuffer.Y = WINDOW_Y;
	Rect.Top = 0;
	Rect.Left = 0;
	Rect.Bottom = WINDOW_Y-1;
	Rect.Right = WINDOW_X-1;

	screenMsgCount = 0;
	fixedSBSize = con.buffer_size;
	msgCutOffLength	= con.message_cut_off_length;

	fileNotificationLevel		= dbg.file_notification_level;
	fileWarningLevel			= dbg.file_warning_level;
	consoleNotificationLevel	= dbg.console_notification_level;
	consoleWarningLevel			= dbg.console_warning_level;
	logfile						= dbg.log_create;

//#ifdef _DEMO_RELEASE
	if(fileNotificationLevel > 3)
		fileNotificationLevel = 3;
	if(fileWarningLevel > 3)
		fileWarningLevel = 3;
	if(consoleNotificationLevel > 3)
		consoleNotificationLevel = 3;
	if(consoleWarningLevel > 3)
		consoleWarningLevel = 3;
//#endif

	if(!useNormalConsole || useColor)
	{
#if defined WIN_MAIN
		AllocConsole();
		freopen( "CONOUT$", "w", stdout);
#endif
		hstdout = GetStdHandle(STD_OUTPUT_HANDLE);
		if(!useNormalConsole)
		{
			if(fixedSBSize >= 40 && fixedSBSize < SHRT_MAX)
				currentScreenBuffer.Y = fixedSBSize;

			SetConsoleScreenBufferSize(hstdout, currentScreenBuffer);
			SetConsoleWindowInfo(hstdout, TRUE, &Rect);
			if(!useColor)
				SetConsoleTextAttribute(hstdout, 0x07);
		}
	}
	// register signal handler
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)QuitHandler, TRUE);

	notification(0, DEBUG, "Starting new log, initializing MASE:BC2 Beta v0.9 by Triver");
	if(dbg.log_create)
	{
		bool splitFile	= dbg.log_timestamp;
		string filename("logfile");

		if(splitFile)
		{
			time_t rawtime;
			struct tm* lt;
			time (&rawtime);
			lt = localtime(&rawtime);

			char logbuf[24];
			sprintf(logbuf, "[%02d.%02d.%d-%02d_%02d_%02d]", lt->tm_mday, lt->tm_mon+1, lt->tm_year+1900, lt->tm_hour, lt->tm_min, lt->tm_sec);
			filename.append(logbuf);
		}
		filename.append(".log");
		setFile(filename.c_str());
	}
}
#else	// linux

void QuitHandler(int signum)
{
	switch(signum)
	{
		case SIGINT:
		case SIGQUIT:
		case SIGTERM:
			{
				if(db)
				{
					db->saveDatabase();
					delete db;
					db = NULL;
				}
				if(fw)
				{
					delete fw;
					fw = NULL;
				}
				if(debug)
				{
					delete debug;
					debug = NULL;
				}
			}
	}
	exit(signum);
}

const string colors[9][2] =		// linux terminal should use escape characters for coloring, apparently the bold setting (1) also affects color intensity
{
	//notifications have default background, warnings have red background (41)
	{"\033[37m",	"\033[37;41m"},		// DEBUG,			Grey
	{"\033[37;1m",	"\033[37;1;41m"},	// DATABASE,		White
	{"\033[32;1m",	"\033[32;1;41m"},	// GAME_SERVER,		Green
	{"\033[36;1m",	"\033[36;1;41m"},	// GAME_SERVER_SSL,	Turquoise
	{"\033[35;1m",	"\033[35;1;41m"},	// GAME_CLIENT,		Pink
	{"\033[33;1m",	"\033[33;1;41m"},	// GAME_CLIENT_SSL,	Yellow
	{"\033[36m",	"\033[36;41m"},		// HTTP,			Aqua (not available in linux? so use blue instead)
	{"\033[33m",	"\033[33;41m"},		// MISC,			Dark Yellow
	{"\033[31;1m",	"\033[30;41;1m"}	// UNKNOWN,			Red (Black when warning due to background)
};
Logger::Logger(debugSettings dbg, consoleSettings con)
{
	useColor = con.use_color;
	//useNormalConsole = con.use_default_console;

	lastColor = "\033[0m";
	// there's neither console resize nor console buffer required in linux (or at least I don't want to deal with it)
	msgCutOffLength	= con.message_cut_off_length;

	fileNotificationLevel		= dbg.file_notification_level;
	fileWarningLevel			= dbg.file_warning_level;
	consoleNotificationLevel	= dbg.console_notification_level;
	consoleWarningLevel			= dbg.console_warning_level;
	logfile						= dbg.log_create;

//#ifdef _DEMO_RELEASE
	if(fileNotificationLevel > 3)
		fileNotificationLevel = 3;
	if(fileWarningLevel > 3)
		fileWarningLevel = 3;
	if(consoleNotificationLevel > 3)
		consoleNotificationLevel = 3;
	if(consoleWarningLevel > 3)
		consoleWarningLevel = 3;
//#endif
	// register signal handler for relevant signals
	signal(SIGINT, QuitHandler);
	signal(SIGTERM, QuitHandler);
	signal(SIGQUIT, QuitHandler);

	notification(0, DEBUG, "Starting new log, initializing MASE:BC2 Beta v0.9 by Triver");
	if(dbg.log_create)
	{
		bool splitFile	= dbg.log_timestamp;
		string filename("logfile");

		if(splitFile)
		{
			time_t rawtime;
			struct tm* lt;
			time (&rawtime);
			lt = localtime(&rawtime);

			char logbuf[24];
			sprintf(logbuf, "[%02d.%02d.%d-%02d_%02d_%02d]", lt->tm_mday, lt->tm_mon+1, lt->tm_year+1900, lt->tm_hour, lt->tm_min, lt->tm_sec);
			filename.append(logbuf);
		}
		filename.append(".log");
		setFile(filename.c_str());
	}
}
#endif

Logger::~Logger( )
{
	notification(1, DEBUG, "Received quit signal, shutting down...");
	//Close the log file
	if(fp.is_open())
		fp.close();
}

void Logger::notification(int level, int from, const char* message, ...)
{
	if(consoleNotificationLevel >= level || fileNotificationLevel >= level)
	{
		const char* fromName = buildName(from);
		va_list va_alist;
		// is there any way to make a variable number of arguments use a dynamic buffer?
		char logbuf[8192];		// biggest size that will occur is around 96750 (base64 encoded player stats)

		//Empty line
		if(from >= 0)
		{
			//get the time
#if defined(_WIN32)
			SYSTEMTIME st = {0};
			GetLocalTime(&st);
			sprintf(logbuf, "[%02d:%02d:%02d.%03d %s] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, fromName);
#else
			struct timeval time;
			struct timezone zone;
			gettimeofday(&time, &zone);
			int msec = (int)((time.tv_usec / 1000) % 1000);
			int sec = (int)(time.tv_sec % 60);
			int minutes = (int)(time.tv_sec/60) - zone.tz_minuteswest;
			int hours = (int)(minutes/60);
			if(zone.tz_dsttime != 0)
				hours++;
			minutes %= 60;
			hours %= 24;
			sprintf(logbuf, "[%02d:%02d:%02d.%03d %s] ", hours, minutes, sec, msec, fromName);
#endif
			//Generate the message

			va_start(va_alist, message);
			vsnprintf(logbuf+strlen(logbuf), sizeof(logbuf) - strlen(logbuf), message, va_alist);
			va_end(va_alist);
		}
		else
			sprintf(logbuf, "null");

		//Print message into file
		if(logfile && fileNotificationLevel >= level)
			fp << logbuf << endl;

		if(msgCutOffLength > -1 && msgCutOffLength < 96750)
			logbuf[msgCutOffLength] = 0;

		//Print the message in the console
		if(consoleNotificationLevel >= level)
			updateConsole(buildColor(from, true), logbuf, strlen(logbuf));
	}
}

void Logger::simpleNotification(int level, int from, const char* message)
{
	if(consoleNotificationLevel >= level || fileNotificationLevel >= level)
	{
		const char* fromName = buildName(from);
		char logbuf[32];

		//Empty line
		if(from >= 0)
		{
			//get the time
#if defined(_WIN32)
			SYSTEMTIME st = {0};
			GetLocalTime(&st);
			sprintf(logbuf, "[%02d:%02d:%02d.%03d %s] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, fromName);
#else
			struct timeval time;
			struct timezone zone;
			gettimeofday(&time, &zone);
			int msec = (int)((time.tv_usec / 1000) % 1000);
			int sec = (int)(time.tv_sec % 60);
			int minutes = (int)(time.tv_sec/60) - zone.tz_minuteswest;
			int hours = (int)(minutes/60);
			if(zone.tz_dsttime != 0)
				hours++;
			minutes %= 60;
			hours %= 24;
			sprintf(logbuf, "[%02d:%02d:%02d.%03d %s] ", hours, minutes, sec, msec, fromName);
#endif
		}
		else
			sprintf(logbuf, "null");

		string text = logbuf;
		text.append(message);

		//Print message into file
		if(logfile && fileNotificationLevel >= level)
			fp << text << endl;

		//Print the message in the console
		if(consoleNotificationLevel >= level)
			updateConsole(buildColor(from, true), text.c_str(), text.size());
	}
}

void Logger::warning(int level, int from, const char* message, ...)
{
	if(consoleNotificationLevel >= level || fileNotificationLevel >= level)
	{
		const char* fromName = buildName(from);
		va_list va_alist;
		char logbuf[2560];

		//get the time
#if defined(_WIN32)
		SYSTEMTIME st = {0};
		GetLocalTime(&st);
		sprintf(logbuf, "[%02d:%02d:%02d.%03d %s](WARNING) ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, fromName);
#else
		struct timeval time;
		struct timezone zone;
		gettimeofday(&time, &zone);
		int msec = (int)((time.tv_usec / 1000) % 1000);
		int sec = (int)(time.tv_sec % 60);
		int minutes = (int)(time.tv_sec/60) - zone.tz_minuteswest;
		int hours = (int)(minutes/60);
		if(zone.tz_dsttime != 0)
			hours++;
		minutes %= 60;
		hours %= 24;
		sprintf(logbuf, "[%02d:%02d:%02d.%03d %s] ", hours, minutes, sec, msec, fromName);
#endif
		//Generate the message
		va_start(va_alist, message);
		vsnprintf(logbuf+strlen(logbuf), sizeof(logbuf) - strlen(logbuf), message, va_alist);
		va_end(va_alist);

		//Print message into file
		if(logfile && fileWarningLevel >= level)
			fp << logbuf << endl;

		if(msgCutOffLength > -1 && msgCutOffLength < 2560)
			logbuf[msgCutOffLength] = 0;

		//Print the message in the console
		if(consoleWarningLevel >= level)
			updateConsole(buildColor(from, false), logbuf, strlen(logbuf));
	}
}

void Logger::error(int from, const char* message, ...)
{
	const char* fromName = buildName(from);

	va_list va_alist;
	char logbuf[2560];

	//get the time
#if defined(_WIN32)
	SYSTEMTIME st = {0};
	GetLocalTime(&st);
	sprintf(logbuf, "[%02d:%02d:%02d.%03d %s](ERROR) ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, fromName);
#else
	struct timeval time;
	struct timezone zone;
	gettimeofday(&time, &zone);
	int msec = (int)((time.tv_usec / 1000) % 1000);
	int sec = (int)(time.tv_sec % 60);
	int minutes = (int)(time.tv_sec/60) - zone.tz_minuteswest;
	int hours = (int)(minutes/60);
	if(zone.tz_dsttime != 0)
		hours++;
	minutes %= 60;
	hours %= 24;
	sprintf(logbuf, "[%02d:%02d:%02d.%03d %s] ", hours, minutes, sec, msec, fromName);
#endif
	//Generate the message
	va_start(va_alist, message);
	vsnprintf(logbuf+strlen(logbuf), sizeof(logbuf) - strlen(logbuf), message, va_alist);
	va_end(va_alist);

	//Print message into file
	if(logfile)
		fp << logbuf << endl;

	if(msgCutOffLength > -1 && msgCutOffLength < 2560)
		logbuf[msgCutOffLength] = 0;

	updateConsole(buildColor(from, false), logbuf, strlen(logbuf));
}

bool Logger::setFile(const char* filename)
{
	fp.open(filename, ios::out | ios::trunc);
	if(fp.is_open() && fp.good())
		return true;
	else
	{
		warning(1, DEBUG, "Could not open log file \"%s\"", filename);
		return false;
	}
}

const char* Logger::buildName(int from)
{
	const char* name;

	switch(from)
	{
		case DEBUG:
			name = "Debug";
			break;
		case DATABASE:
			name = "Database";
			break;
		case GAME_SERVER:
		case GAME_SERVER_SSL:
			name = "Server";
			break;
		case GAME_CLIENT:
		case GAME_CLIENT_SSL:
		case HTTP:
		case MISC:
			name = "Client";
			break;
		default:
			name = "Unknown";
	}

	return name;
}

#if defined (_WIN32)
unsigned short Logger::buildColor(int from, bool notification)
#else
string Logger::buildColor(int from, bool notification)
#endif
{
	return colors[from][!notification];		//we have to invert the boolean to make use of '0' and '1' properly
}

#if defined(_WIN32)
void Logger::updateConsole(WORD color, const char* msg, int msgLength)
{
	bool change = false;
	screenMsgCount++;

	if(useColor && lastColor != color)
	{
		lastColor = color;
		SetConsoleTextAttribute(hstdout, color);
	}

	if(!useNormalConsole)
	{
		if(msgLength > currentScreenBuffer.X && currentScreenBuffer.X+1 <= SHRT_MAX)
		{
			if(msgLength < SHRT_MAX)
				currentScreenBuffer.X = msgLength+2;
			else
				currentScreenBuffer.X = SHRT_MAX;
			change = true;
		}
		if(fixedSBSize < 40 && screenMsgCount >= currentScreenBuffer.Y && currentScreenBuffer.Y+1 <= SHRT_MAX)
		{
			if(currentScreenBuffer.Y + 100 < SHRT_MAX)
				currentScreenBuffer.Y += 100;			//if we update it every new line he slows down really hard
			else
				currentScreenBuffer.Y = SHRT_MAX;
			change = true;
		}

		if(change)
			SetConsoleScreenBufferSize(hstdout, currentScreenBuffer);
	}
	printf("%s\n", msg);
}

void Logger::updateTitle(char* title)
{
	SetConsoleTitle(title);
}
#else
void Logger::updateConsole(string color, const char* msg, int msgLength)   // msgLength does notthing here
{
	if(useColor)// && lastColor.compare(color) != 0)	// since we reset all text attributes we always set a new color for a new message
	{
		//lastColor = color;
		printf("%s%s\033[0m\n", color.c_str(), msg);	// reset color attributes after message
	}
	else
		printf("%s\n", msg);
}
void Logger::updateTitle(char* title)
{
	// do nothing in linux
}
#endif
