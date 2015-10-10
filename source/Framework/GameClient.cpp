#include "GameClient.h"
#include "Framework.h"
#include "../Base64.h"
#include <boost/filesystem/operations.hpp>
extern Logger* debug;
extern Framework* fw;

GameClient::GameClient(int connectionType, Database* db, string emuIp, bool isLocal)
{
	this->connectionType = connectionType;
	this->logDatabase = fw->DatabaseLogging();
	database = db;
	user.data = NULL;
	persona.data = NULL;

	this->isLocal = isLocal;
	this->emuIp = emuIp;
	alreadyLoggedIn = false;
	stats_data = NULL;

	sock_tid = 0;
	queue_position = 0;
}

GameClient::~GameClient()
{
	if(connectionType == GAME_CLIENT_SSL)
	{
		if(user.data && (user.data[USER_ONLINE].compare("1") == 0) && !alreadyLoggedIn)
		{
			user.data[USER_ONLINE] = "0";
			if(persona.data)
				persona.data[PERSONA_ONLINE] = "0";
		}
	}
	playerList.clear();

	if(connectionType == GAME_CLIENT_SSL && stats_data)
		delete stats_data;

}

bool GameClient::acct(list<Packet*>* queue, Packet* packet, string txn, const char* ip)
{
	Packet* sendPacket = NULL;

	if(txn.compare("GetCountryList") == 0)		// User wants to create a new account
	{
		string countryData("");
		ifstream statsFile("./templates/countryList", ifstream::in);
		if(statsFile.is_open() && statsFile.good())
		{
			stringstream buffer;
			buffer << statsFile.rdbuf();
			countryData = buffer.str();
			statsFile.close();
		}

		size_t count = 0, start = 0;
		while((start = countryData.find("\n", start)) != string::npos)
		{
			count++;
			start++;
		}
		sendPacket = new Packet("acct", 0x80000000);
		sendPacket->SetVar("TXN", txn);
		sendPacket->SetVar("countryList.[]", count);
		start = 0;
		for(size_t i = 0; i < count; i++)
		{
			char buffer[48];
			size_t end = countryData.find("=", start);
			if(end != string::npos)
			{
				string data = countryData.substr(start, end-start);
				sprintf(buffer, "countryList.%i.ISOCode", i);
				sendPacket->SetVar(buffer, data);

				start = end+1;
				end = countryData.find("\n", start);
				if(end == string::npos)
					end = countryData.size();		//we are at the last line

				data = countryData.substr(start, end-start);
				sprintf(buffer, "countryList.%i.description", i);
				sendPacket->SetVar(buffer, data);

				if(end != countryData.size())
					start = end+1;
			}
			else
			{
				debug->warning(2, connectionType, "countryData is corrupted, check the file!");
				break;
			}
		}
	}

	else if(txn.compare("NuGetTos") == 0)
	{
		sendPacket = new Packet("acct", 0x80000000);
		sendPacket->SetVar("TXN", txn);
		sendPacket->SetVar("version", "20426_20.20426_20");

		string copiedData = "";
		ifstream sourceFile("./templates/termsOfService", ifstream::in);
		if(sourceFile.is_open() && sourceFile.good())
		{
			stringstream buffer;
			buffer << sourceFile.rdbuf();
			copiedData = buffer.str();
			sourceFile.close();
		}
		else
			debug->warning(2, DEBUG, "NuGetTos, failed to open termsOfService template, check if the directory and the files exist.");

		if(copiedData.size() > 1)
		{
			size_t pos = 0;
			while((pos = copiedData.find_first_of('%', pos+1)) != string::npos)
				copiedData.replace(pos, 1, "%25");
			pos = 0;
			while((pos = copiedData.find_first_of('\n', pos+1)) != string::npos)
				copiedData.replace(pos, 1, "%0a");
			pos = 0;
			while((pos = copiedData.find_first_of(':', pos+1)) != string::npos)
				copiedData.replace(pos, 1, "%3a");
			pos = 0;
			while((pos = copiedData.find_first_of('\"', pos+1)) != string::npos)
				copiedData.replace(pos, 1, "%22");
			pos = 0;
			while((pos = copiedData.find_first_of('\t', pos+1)) != string::npos)
				copiedData.replace(pos, 1, "%09");
		}
		copiedData.append("%0a%0a%09MASE%3aBC2 v0.9 made by Triver");
		copiedData.insert(copiedData.begin(), '\"');
		copiedData.append("\"");
		sendPacket->SetVar("tos", copiedData);
	}

	else if(txn.compare("NuAddAccount") == 0)
	{
		list_entry tempUser;
		tempUser.name = packet->GetVar("nuid");
		int realSize = tempUser.name.size();
		int pos = -1;
		while((pos = tempUser.name.find_first_of('%', pos+1)) != (int)string::npos)
			realSize -= 2;

		if(realSize > 32 || realSize < 3)		// entered user name length is out of bounds
		{
			sendPacket = new Packet("acct", 0x80000000);
			sendPacket->SetVar("TXN", txn);
			sendPacket->SetVar("errorContainer.[]", "1");
			sendPacket->SetVar("errorCode", "21");
			sendPacket->SetVar("localizedMessage", "\"The required parameters for this call are missing or invalid\"");
			sendPacket->SetVar("errorContainer.0.fieldName", "displayName");
			if(realSize > 32)
			{
				sendPacket->SetVar("errorContainer.0.fieldError", "3");
				sendPacket->SetVar("errorContainer.0.value", "TOO_LONG");
			}
			else
			{
				sendPacket->SetVar("errorContainer.0.fieldError", "2");
				sendPacket->SetVar("errorContainer.0.value", "TOO_SHORT");
			}
		}	//forbidden chars due to file creation incompatibilities
		else if(tempUser.name.find_first_of('/') != string::npos || tempUser.name.find_first_of('\\') != string::npos || tempUser.name.find_first_of('?') != string::npos ||
				tempUser.name.find_first_of('*') != string::npos || tempUser.name.find_first_of('\"') != string::npos || tempUser.name.find_first_of('|') != string::npos ||
				tempUser.name.find_first_of(':') != string::npos || tempUser.name.find_first_of('<') != string::npos || tempUser.name.find_first_of('>') != string::npos)
		{
			sendPacket = new Packet("acct", 0x80000000);
			sendPacket->SetVar("TXN", txn);
			sendPacket->SetVar("errorContainer.[]", "1");
			sendPacket->SetVar("errorCode", "21");
			sendPacket->SetVar("localizedMessage", "\"The required parameters for this call are missing or invalid\"");
			sendPacket->SetVar("errorContainer.0.fieldName", "displayName");
			sendPacket->SetVar("errorContainer.0.fieldError", "6");
			sendPacket->SetVar("errorContainer.0.value", "NOT_ALLOWED");
		}
		else		//first check the age then do database stuff
		{
			time_t rawtime;
			struct tm* lt;
			time (&rawtime);
			lt = localtime(&rawtime);

			int bd_year = lexical_cast<int>(packet->GetVar("DOBYear"));
			int bd_month = lexical_cast<int>(packet->GetVar("DOBMonth"));
			int bd_day = lexical_cast<int>(packet->GetVar("DOBDay"));
			bool isValid = false;

			//have to do age check manually because boost::posix_time is too painful to use
			if((lt->tm_year+1900)-bd_year > AGE_LIMIT)
				isValid = true;
			else if((lt->tm_year+1900)-bd_year == AGE_LIMIT)
			{
				if((lt->tm_mon+1) > bd_month)
					isValid = true;
				else if((lt->tm_mon+1) == bd_month)
				{
					if(lt->tm_mday >= bd_day)
						isValid = true;
				}
			}

			if(isValid)
			{
				tempUser.data = new string[USER_SIZE];
				tempUser.data[PASSWORD] = packet->GetVar("password");
				tempUser.data[COUNTRY] = packet->GetVar("country");
				char buffer[12];
				sprintf(buffer, "%i-%i-%i", bd_year, bd_month, bd_day);
				tempUser.data[BIRTHDAY] = buffer;
				tempUser.data[USER_ONLINE] = "0";

				if((tempUser.id = database->addUser(tempUser)) < 0)
				{
					delete[] tempUser.data;
					sendPacket = new Packet("acct", 0x80000000);
					sendPacket->SetVar("TXN", txn);
					sendPacket->SetVar("localizedMessage", "\"That account name is already taken\"");
					sendPacket->SetVar("errorContainer.[]", "0");
					sendPacket->SetVar("errorCode", "160");
				}
				else	// returned id is valid so create folder for user
				{
					string directory = "./database/";
					directory.append(tempUser.name);
					filesystem::path user_dir(directory);
					if(filesystem::create_directory(user_dir))
					{
						if(logDatabase)
							debug->simpleNotification(3, DATABASE, database->listEntryToString(&tempUser, "added", true).c_str());
					}
					else
						debug->notification(2, connectionType, "Failed to create user directory %s, check if you have access permissions for your database folder", directory.c_str());

					sendPacket = new Packet("acct", 0x80000000);
					sendPacket->SetVar("TXN", txn);
				}
			}
			else	// new user is not old enough
			{
				sendPacket = new Packet("acct", 0x80000000);
				sendPacket->SetVar("localizedMessage", "\"The required parameters for this call are missing or invalid\"");
				sendPacket->SetVar("TXN", txn);
				sendPacket->SetVar("errorContainer.[]", "1");
				sendPacket->SetVar("errorContainer.0.fieldName", "dob");
				sendPacket->SetVar("errorContainer.0.fieldError", "15");
				sendPacket->SetVar("errorCode", "21");
			}
		}
	}

	else if(txn.compare("NuLogin") == 0)		// User is logging in with email and password
	{
		sendPacket = new Packet("acct", 0x80000000);
		sendPacket->SetVar("TXN", txn);

		string buffer = packet->GetVar("nuid"), password;
		if(!buffer.empty())
		{
			database->getUser(buffer, &user);
			if(user.id > 0)
				password = packet->GetVar("password");
		}
		else
		{
			string decryptedInfo = packet->GetVar("encryptedInfo");
			if(!decryptedInfo.empty() && decryptedInfo.find("Ciyvab0tregdVsBtboIpeChe4G6uzC1v5_-SIxmvSL") != string::npos)
			{
				decryptedInfo = decryptedInfo.substr(42, decryptedInfo.size()-42);	//42 is the size of that first chunk
				size_t pos = 0;
				while(	(pos = decryptedInfo.find('-', decryptedInfo.size()-3)) != string::npos ||
						(pos = decryptedInfo.find('_', decryptedInfo.size()-3)) != string::npos)		// bring string into proper format again
					decryptedInfo.at(pos) = '=';

				decryptedInfo = base64_decode(decryptedInfo);
				if((pos = decryptedInfo.find_first_of('\f')) != string::npos)
				{
					buffer = decryptedInfo.substr(0, pos);
					password = decryptedInfo.substr(pos+1, decryptedInfo.size()-1);		//skip the '\f' and dont go outside string
					database->getUser(buffer, &user);
				}
				else
					user.id = -1;
			}
			else
				user.id = -1;
		}

		if(user.id > 0)
		{
			if(user.data[PASSWORD].compare(password) == 0)
			{
				if(user.data[USER_ONLINE].compare("0") == 0)
				{
					user_lkey = fw->randomString(27, SEED_DEFAULT);
					user_lkey.append(".");
					user.data[USER_ONLINE] = "1";

					sendPacket->SetVar("lkey", user_lkey);
					sendPacket->SetVar("nuid", user.name);
					string encryptedInfo = packet->GetVar("returnEncryptedInfo");
					if(!encryptedInfo.empty() && (encryptedInfo.compare("1") == 0))		//client wants to store login information
					{
						//Ciyvab0tregdVsBtboIpeChe4G6uzC1v5_-SIxmvSL
						//Ciyvab0tregdVsBtboIpeChe4G6uzC1v5_-SIxmvSLIJIU9Vh4C77cH52ArVStN4rKMtWGByi2mOVDuM05Wj31Vit91nAbmg23z2IV4GfubCi9Owt1eBqxmXS6KrceaR

						// store the user name and password as a base64 encoded string and put the chunk in front of it (so we have a similar format to the original one)
						string encrypted("Ciyvab0tregdVsBtboIpeChe4G6uzC1v5_-SIxmvSL");
						size_t pos = 0;
						buffer.append("\f");
						buffer.append(password);
						buffer = base64_encode(reinterpret_cast<const unsigned char*>(buffer.c_str()), buffer.size());

						pos = buffer.find('=', buffer.size()-3);
						if(pos != string::npos)
						{
							buffer.at(pos) = '-';
							pos = buffer.find('=', buffer.size()-3);
							if(pos != string::npos)
								buffer.at(pos) = '_';
						}

						encrypted.append(buffer);
						sendPacket->SetVar("encryptedLoginInfo", encrypted);
						//sendPacket->SetVar("encryptedLoginInfo", "Ciyvab0tregdVsBtboIpeChe4G6uzC1v5_-SIxmvSLJubxCaMWOxD1iabTkdIjA6nFyrA9nvKvtbovnovBD99T8hIHEGPkl-I9xeEXAXN9ICCmuaWcMHP6q33WjvASgI");	// is this required?
					}
					sendPacket->SetVar("profileId", user.id);
					sendPacket->SetVar("userId", user.id);
					if(logDatabase)
						debug->simpleNotification(3, DATABASE, database->listEntryToString(&user, "logged in", true).c_str());
				}
				else		// user is already online so prevent login (yes, fsys is correct here)
				{
					debug->notification(3, connectionType, "User %s with ID %i is already logged in, disconnecting this user...", user.name.c_str(), user.id);
					delete sendPacket;
					alreadyLoggedIn = true;
					sendPacket = new Packet("fsys", 0x00000000);
					sendPacket->SetVar("TXN", "Goodbye");
					sendPacket->SetVar("reason", "2");
				}
			}
			else
			{
				sendPacket->SetVar("localizedMessage", "\"The password the user specified is incorrect\"");
				sendPacket->SetVar("errorContainer.[]", "0");
				sendPacket->SetVar("errorCode", "122");
				/*sendPacket->SetVar("localizedMessage", "\"The user is not entitled to access this game\"");
				sendPacket->SetVar("errorCode", "120");*/
			}
		}
		else
		{
			sendPacket->SetVar("localizedMessage", "\"The user was not found\"");
			sendPacket->SetVar("errorContainer.[]", "0");
			sendPacket->SetVar("errorCode", "101");
		}
	}

	else if(txn.compare("NuGetPersonas") == 0)		// Display all Personas to the User
	{
		list<list_entry> personas;
		database->listMatchingPersonas(USER_ID, lexical_cast<string>(user.id), &personas);

		sendPacket = new Packet("acct", 0x80000000);
		sendPacket->SetVar("TXN", txn);
		sendPacket->SetVar("personas.[]", (int)personas.size());
		int i = 0;
		while(!personas.empty())
		{
			string buffer = "personas.";
			buffer.append(lexical_cast<string>(i));
			sendPacket->SetVar(buffer, personas.front().name);
			personas.pop_front();
			i++;
		}
	}

	else if(txn.compare("NuLoginPersona") == 0)		// User logs in with selected Persona
	{
		sendPacket = new Packet("acct", 0x80000000);
		sendPacket->SetVar("TXN", txn);
		if(!database->getPersona(packet->GetVar("name"), &persona))
		{
			sendPacket->SetVar("localizedMessage", "\"The user was not found\"");
			sendPacket->SetVar("errorContainer.[]", "0");
			sendPacket->SetVar("errorCode", "101");
		}
		else
		{
			persona_lkey = fw->randomString(27, SEED_DEFAULT);
			persona_lkey.append(".");
			persona.data[PERSONA_ONLINE] = "1";

			sendPacket->SetVar("lkey", persona_lkey);
			sendPacket->SetVar("profileId", persona.id);
			sendPacket->SetVar("userId", persona.id);

			linked_key lkey_id = {persona.id, persona_lkey};
			database->personaLogin(lkey_id);
			if(logDatabase)
				debug->simpleNotification(3, DATABASE, database->listEntryToString(&persona, "logged in", false).c_str());
		}
	}

	else if(txn.compare("NuAddPersona") == 0)		// User wants to add a Persona
	{
		string name = packet->GetVar("name");
		int realSize = name.size();
		int pos = -1;
		while((pos = name.find_first_of('%', pos+1)) != (int)string::npos)
			realSize -= 2;

		if(realSize > 16 || realSize < 3)		// entered persona name length is out of bounds (old: 4 <= name <= 16)
		{
			sendPacket = new Packet("acct", 0x80000000);
			sendPacket->SetVar("TXN", txn);
			sendPacket->SetVar("errorContainer.[]", "1");
			sendPacket->SetVar("errorCode", "21");
			sendPacket->SetVar("localizedMessage", "\"The required parameters for this call are missing or invalid\"");
			sendPacket->SetVar("errorContainer.0.fieldName", "displayName");
			if(realSize > 16)
			{
				sendPacket->SetVar("errorContainer.0.fieldError", "3");
				sendPacket->SetVar("errorContainer.0.value", "TOO_LONG");
			}
			else
			{
				sendPacket->SetVar("errorContainer.0.fieldError", "2");
				sendPacket->SetVar("errorContainer.0.value", "TOO_SHORT");
			}
		}	//forbidden chars due to file creation incompatibilities
		else if(name.find_first_of('/') != string::npos || name.find_first_of('\\') != string::npos || name.find_first_of('?') != string::npos ||
				name.find_first_of('*') != string::npos || name.find_first_of('\"') != string::npos || name.find_first_of('|') != string::npos ||
				name.find_first_of(':') != string::npos ||name.find_first_of('<') != string::npos || name.find_first_of('>') != string::npos)
		{
			sendPacket = new Packet("acct", 0x80000000);
			sendPacket->SetVar("TXN", txn);
			sendPacket->SetVar("errorContainer.[]", "1");
			sendPacket->SetVar("errorCode", "21");
			sendPacket->SetVar("localizedMessage", "\"The required parameters for this call are missing or invalid\"");
			sendPacket->SetVar("errorContainer.0.fieldName", "displayName");
			sendPacket->SetVar("errorContainer.0.fieldError", "6");
			sendPacket->SetVar("errorContainer.0.value", "NOT_ALLOWED");
		}
		else		//valid data, check if name is unique
		{
			sendPacket = new Packet("acct", 0x80000000);
			sendPacket->SetVar("TXN", txn);

			list_entry tempPersona;
			tempPersona.name = name;
			tempPersona.data = new string[PERSONA_SIZE];
			tempPersona.data[USER_ID] = lexical_cast<string>(user.id);
			tempPersona.data[USER_EMAIL] = user.name;
			tempPersona.data[PLAYER_ID] = "0";
			tempPersona.data[PERSONA_ONLINE] = "0";

			if((tempPersona.id = database->addPersona(tempPersona)) < 0)		//name already exists
			{
				delete[] tempPersona.data;
				sendPacket->SetVar("errorContainer.[]", "0");
				sendPacket->SetVar("localizedMessage", "\"That account name is already taken\"");
				sendPacket->SetVar("errorCode", "160");
			}
			else	// persona added, create a default stats file for it
			{
					// will tempStats be "deleted" when we leave this function? I sure hope so!
				Stats tempStats = Stats(user.name, name, true);
				if(logDatabase)
					debug->simpleNotification(3, DATABASE, database->listEntryToString(&tempPersona, "added", false).c_str());
			}
		}
	}

	else if(txn.compare("NuDisablePersona") == 0)	// User wants to remove a Persona
	{
		list_entry temp;
		sendPacket = new Packet("acct", 0x80000000);
		sendPacket->SetVar("TXN", txn);

		bool valid = false;
		string name = packet->GetVar("name");
		if(!database->getPersona(name, &temp))
		{
			sendPacket->SetVar("localizedMessage", "\"The data necessary for this transaction was not found\"");
			sendPacket->SetVar("errorContainer.[]", "0");
			sendPacket->SetVar("errorCode", "104");
		}
		else
		{
			if(user.id != lexical_cast<int>(temp.data[USER_ID]))
			{
				sendPacket->SetVar("localizedMessage", "\"The data necessary for this transaction was not found\"");
				sendPacket->SetVar("errorContainer.[]", "0");
				sendPacket->SetVar("errorCode", "104");
			}
			else
				valid = true;
		}

		if(valid)
		{
			database->removePersona(name);
			Stats tempStats = Stats(user.name, name, false, true);
			tempStats.deleteStats();
		}
	}

	else if(txn.compare("GetTelemetryToken") == 0)
	{
		//TODO: make the telemetry token database
		sendPacket = new Packet("acct", 0x80000000);
		sendPacket->SetVar("TXN", txn);

		//sendPacket->SetVar("telemetryToken", "MTU5LjE1My4yMzUuMjYsOTk0NixlblVTLF7ZmajcnLfGpKSJk53K/4WQj7LRw9asjLHvxLGhgoaMsrDE3bGWhsyb4e6woYKGjJiw4MCBg4bMsrnKibuDppiWxYKditSp0amvhJmStMiMlrHk4IGzhoyYsO7A4dLM26rTgAo%3d");
		//changed ip to ip of the emulator, port remains the same (9946?)
		size_t pos = 0;
		string token = emuIp;
		if(lexical_cast<int>(fw->portsCfg().emulator_port) <= 0)
			token = "127.0.0.1";
		token.append(",");
		token.append(fw->portsCfg().emulator_port);
		token.append(",enUS,^Ÿô®‹ú∑∆§§âìù ˇÖêè≤—√÷¨å±Ôƒ±°ÇÜå≤∞ƒ›±ñÜÃõ·Ó∞°ÇÜåò∞‡¿ÅÉÜÃ≤π âªÉ¶òñ≈Çùä‘©—©ØÑôí¥»åñ±‰‡Å≥Üåò∞Ó¿·“Ã€™”Ä");
		token = base64_encode(reinterpret_cast<const unsigned char*>(token.c_str()), token.size());
		while((pos = token.find('=', token.size()-9)) != string::npos)		// bring string into proper format again
			token.replace(pos, 1, "%3d");

		sendPacket->SetVar("telemetryToken", token);
		sendPacket->SetVar("enabled", "CA,MX,PR,US,VI,AD,AF,AG,AI,AL,AM,AN,AO,AQ,AR,AS,AW,AX,AZ,BA,BB,BD,BF,BH,BI,BJ,BM,BN,BO,BR,BS,BT,BV,BW,BY,BZ,CC,CD,CF,CG,CI,CK,CL,CM,CN,CO,CR,CU,CV,CX,DJ,DM,DO,DZ,EC,EG,EH,ER,ET,FJ,FK,FM,FO,GA,GD,GE,GF,GG,GH,GI,GL,GM,GN,GP,GQ,GS,GT,GU,GW,GY,HM,HN,HT,ID,IL,IM,IN,IO,IQ,IR,IS,JE,JM,JO,KE,KG,KH,KI,KM,KN,KP,KR,KW,KY,KZ,LA,LB,LC,LI,LK,LR,LS,LY,MA,MC,MD,ME,MG,MH,ML,MM,MN,MO,MP,MQ,MR,MS,MU,MV,MW,MY,MZ,NA,NC,NE,NF,NG,NI,NP,NR,NU,OM,PA,PE,PF,PG,PH,PK,PM,PN,PS,PW,PY,QA,RE,RS,RW,SA,SB,SC,clntSock,SG,SH,SJ,SL,SM,SN,SO,SR,ST,SV,SY,SZ,TC,TD,TF,TG,TH,TJ,TK,TL,TM,TN,TO,TT,TV,TZ,UA,UG,UM,UY,UZ,VA,VC,VE,VG,VN,VU,WF,WS,YE,YT,ZM,ZW,ZZ");
		sendPacket->SetVar("filters", "");
		sendPacket->SetVar("disabled", "");
	}

	else if(txn.compare("NuGetEntitlements") == 0)
	{
		string groupName = packet->GetVar("groupName");
		if(groupName.compare("BFBC2PC") == 0)
		{
			//TODO: make the BFBC2 entitlements database
			sendPacket = new Packet("acct", 0x80000000);
			sendPacket->SetVar("TXN", txn);
			int count = 0;
			char buffer[36];
			if(fw->emuCfg().vietnam_for_all)
			{
				sendPacket->SetVar("entitlements.0.grantDate", "2010-12-21T22%3a34Z");
				sendPacket->SetVar("entitlements.0.groupName", groupName);
				sendPacket->SetVar("entitlements.0.userId", user.id);
				sendPacket->SetVar("entitlements.0.entitlementTag", "BFBC2%3aPC%3aVIETNAM_ACCESS");
				sendPacket->SetVar("entitlements.0.version", "0");
				sendPacket->SetVar("entitlements.0.terminationDate", "");
				sendPacket->SetVar("entitlements.0.productId", "DR%3a219316800");
				sendPacket->SetVar("entitlements.0.entitlementId", "1");
				sendPacket->SetVar("entitlements.0.status", "ACTIVE");

				sendPacket->SetVar("entitlements.1.grantDate", "2010-12-21T22%3a34Z");
				sendPacket->SetVar("entitlements.1.statusReasonCode", "");
				sendPacket->SetVar("entitlements.1.groupName", groupName);
				sendPacket->SetVar("entitlements.1.userId", user.id);
				sendPacket->SetVar("entitlements.1.entitlementTag", "BFBC2%3aPC%3aVIETNAM_PDLC");
				sendPacket->SetVar("entitlements.1.version", "0");
				sendPacket->SetVar("entitlements.1.terminationDate", "");
				sendPacket->SetVar("entitlements.1.productId", "DR%3a219316800");
				sendPacket->SetVar("entitlements.1.entitlementId", "2");
				sendPacket->SetVar("entitlements.1.status", "ACTIVE");

				count += 2;
			}
			if(fw->emuCfg().premium_for_all)
			{
				// need a buffer here to write the entitlement index number because this can vary
				sprintf(buffer, "entitlements.%i.grantDate", count);
				sendPacket->SetVar(buffer, "2010-12-21T22%3a34Z");
				sprintf(buffer, "entitlements.%i.statusReasonCode", count);
				sendPacket->SetVar(buffer, "");
				sprintf(buffer, "entitlements.%i.groupName", count);
				sendPacket->SetVar(buffer, groupName);
				sprintf(buffer, "entitlements.%i.userId", count);
				sendPacket->SetVar(buffer, user.id);
				sprintf(buffer, "entitlements.%i.entitlementTag", count);
				sendPacket->SetVar(buffer, "BFBC2%3aPC%3aLimitedEdition");
				sprintf(buffer, "entitlements.%i.version", count);
				sendPacket->SetVar(buffer, "1");
				sprintf(buffer, "entitlements.%i.terminationDate", count);
				sendPacket->SetVar(buffer, "");
				sprintf(buffer, "entitlements.%i.productId", count);
				sendPacket->SetVar(buffer, "OFB-BFBC%3a19120");
				sprintf(buffer, "entitlements.%i.entitlementId", count);
				sendPacket->SetVar(buffer, "3");
				sprintf(buffer, "entitlements.%i.status", count);
				sendPacket->SetVar(buffer, "ACTIVE");
				count++;
			}
			if(fw->emuCfg().specact_for_all)
			{
				sprintf(buffer, "entitlements.%i.grantDate", count);
				sendPacket->SetVar(buffer, "2010-12-21T22%3a34Z");
				sprintf(buffer, "entitlements.%i.statusReasonCode", count);
				sendPacket->SetVar(buffer, "");
				sprintf(buffer, "entitlements.%i.groupName", count);
				sendPacket->SetVar(buffer, groupName);
				sprintf(buffer, "entitlements.%i.userId", count);
				sendPacket->SetVar(buffer, user.id);
				sprintf(buffer, "entitlements.%i.entitlementTag", count);
				sendPacket->SetVar(buffer, "BFBC2%3aPC%3aALLKIT");
				sprintf(buffer, "entitlements.%i.version", count);
				sendPacket->SetVar(buffer, "0");
				sprintf(buffer, "entitlements.%i.terminationDate", count);
				sendPacket->SetVar(buffer, "");
				sprintf(buffer, "entitlements.%i.productId", count);
				sendPacket->SetVar(buffer, "DR%3a192365600");
				sprintf(buffer, "entitlements.%i.entitlementId", count);
				sendPacket->SetVar(buffer, "4");
				sprintf(buffer, "entitlements.%i.status", count);
				sendPacket->SetVar(buffer, "ACTIVE");
				count++;
			}
			sendPacket->SetVar("entitlements.[]", count);
		}
		else if(groupName.compare("AddsVetRank") == 0)
		{
			//TODO: make the AddsVetRank entitlements database
			sendPacket = new Packet("acct", 0x80000000);
			sendPacket->SetVar("TXN", txn);
			sendPacket->SetVar("entitlements.0.statusReasonCode", "");
			sendPacket->SetVar("entitlements.0.groupName", "AddsVetRank");
			sendPacket->SetVar("entitlements.0.grantDate", "2011-07-30T0%3a11Z");
			sendPacket->SetVar("entitlements.0.version", "0");
			sendPacket->SetVar("entitlements.0.entitlementId", "1114495162");
			sendPacket->SetVar("entitlements.0.terminationDate", "");
			sendPacket->SetVar("entitlements.0.productId", "");
			sendPacket->SetVar("entitlements.0.entitlementTag", "BFBC2%3aPC%3aADDSVETRANK");
			sendPacket->SetVar("entitlements.0.status", "ACTIVE");
			sendPacket->SetVar("entitlements.0.userId", user.id);
			if(fw->emuCfg().all_players_are_veterans)
			{
				sendPacket->SetVar("entitlements.1.statusReasonCode", "");
				sendPacket->SetVar("entitlements.1.groupName", "AddsVetRank");
				sendPacket->SetVar("entitlements.1.grantDate", "2011-10-27T6%3a49Z");
				sendPacket->SetVar("entitlements.1.version", "0");
				sendPacket->SetVar("entitlements.1.entitlementId", "1272849755");
				sendPacket->SetVar("entitlements.1.terminationDate", "");
				sendPacket->SetVar("entitlements.1.productId", "OFB-EAST%3a40873");
				sendPacket->SetVar("entitlements.1.entitlementTag", "BF3%3aPC%3aADDSVETRANK");
				sendPacket->SetVar("entitlements.1.status", "ACTIVE");
				sendPacket->SetVar("entitlements.1.userId", user.id);
				sendPacket->SetVar("entitlements.[]", "2");
			}
			else
				sendPacket->SetVar("entitlements.[]", "1");
		}
		else
		{
			sendPacket = new Packet("acct", 0x80000000);
			sendPacket->SetVar("TXN", txn);
			sendPacket->SetVar("entitlements.[]", "0");
		}
		/*else if(groupName.compare("BattlefieldBadCompany2") == 0)
		{
			//TODO: make the BattlefieldBadCompany2 entitlements database
			sendPacket = new Packet("acct", 0x80000000);
			sendPacket->SetVar("TXN", txn);
			sendPacket->SetVar("entitlements.[]", "0");
		}
		else if(groupName.compare("NoVetRank") == 0)
		{
			sendPacket = new Packet("acct", 0x80000000);
			sendPacket->SetVar("TXN", txn);
			sendPacket->SetVar("entitlements.[]", "0");
		}
		else
		{
			debug->warning(1, connectionType, "Could not handle TXN=%s group=%s", txn.c_str(), groupName.c_str());
			return true;
		}*/
	}

	else if(txn.compare("NuGrantEntitlement") == 0)		//is this ever called from the client?
	{
		sendPacket = new Packet("acct", 0x80000000);
		sendPacket->SetVar("TXN", txn);
	}

	else if(txn.compare("GetLockerURL") == 0)
	{
		//URL=http%3a//bfbc2.gos.ea.com/fileupload/locker2.jsp
		string url = "http%3a//";
		if(fw->portsCfg().use_http)
			url.append(emuIp);
		else
			url.append("127.0.0.1");
		url.append("/fileupload/locker2.jsp");

		sendPacket = new Packet("acct", 0x80000000);
		sendPacket->SetVar("TXN", txn);
		sendPacket->SetVar("URL", url);
	}

	else if(txn.compare("NuSearchOwners") == 0)			// User searches for Persona name
	{
		string screenName = packet->GetVar("screenName");
		list_entry tempPersona;

		sendPacket = new Packet("acct", 0x80000000);
		sendPacket->SetVar("TXN", txn);
		if(!database->getPersona(screenName, &tempPersona))
		{
			sendPacket->SetVar("errorContainer.[]", "0");
			sendPacket->SetVar("errorCode", "104");
			sendPacket->SetVar("localizedMessage", "\"The data necessary for this transaction was not found\"");
		}
		else
		{
			// todo: check what to do here if persona was found (above msg is correct)
			/*
<-TXN=NuSearchOwners
screenName=ugahbugah
searchType=1
retrieveUserIds=0

->users.[]=1
users.0.id=838297533
TXN=NuSearchOwners
users.0.name=UgaHBugaH
nameSpaceId=battlefield
users.0.type=1*/
		}
		//sendPacket->SetVar("retrieveUserIds", (int)result->rowsCount());
	}
	else if(txn.compare("NuEntitleGame") == 0)			// User wants to register himself
	{
		sendPacket = new Packet("acct", 0x80000000);
		sendPacket->SetVar("TXN", txn);
		/*localizedMessage="The user is not entitled to access this game"
		errorContainer.[]=0
		errorCode=120*/
		//todo: capture this packet (with a valid code) and implement it
	}
	else if(txn.compare("NuEntitleUser") == 0)			// User wants to redeem a code
	{
		sendPacket = new Packet("acct", 0x80000000);
		sendPacket->SetVar("TXN", txn);
		sendPacket->SetVar("errorContainer.[]", "0");
		sendPacket->SetVar("errorCode", "181");
		sendPacket->SetVar("localizedMessage", "\"The code is not valid for registering this game\"");
		//sendPacket->SetVar("TXN", "NuGrantEntitlement");
		//todo: capture this packet (with a valid code) and implement it
	}
	else if(txn.compare("NuLookupUserInfo") == 0)
	{
		string playerPersonaName = packet->GetVar("userInfo.0.userName");
		list_entry result;
		sendPacket = new Packet("acct", 0x80000000);
		sendPacket->SetVar("TXN", txn);

		if(database->getPersona(playerPersonaName, &result))
		{
			sendPacket->SetVar("userInfo.[]", "1");
			sendPacket->SetVar("userInfo.0.userName", playerPersonaName);
			sendPacket->SetVar("userInfo.0.namespace", "battlefield");
			sendPacket->SetVar("userInfo.0.userId", result.id);		//persona id
			sendPacket->SetVar("userInfo.0.masterUserId", result.data[USER_ID]);
		}
		else
		{
			sendPacket->SetVar("userInfo.[]", "1");
			sendPacket->SetVar("userInfo.0.userName", playerPersonaName);
		}
	}
	else
		return true;

	if(sendPacket)
		queue->push_back(sendPacket);

	return false;
}


bool GameClient::asso(list<Packet*>* queue, Packet* packet, string txn)
{
	Packet* sendPacket = NULL;

	if(txn.compare("AddAssociations") == 0)
	{
		sendPacket = new Packet("asso", 0x80000000);
		sendPacket->SetVar("TXN", txn);
		sendPacket->SetVar("type", packet->GetVar("type"));
		sendPacket->SetVar("result.[]", "0");
		sendPacket->SetVar("domainPartition.domain", "eagames");
		sendPacket->SetVar("domainPartition.subDomain", "BFBC2");
		sendPacket->SetVar("maxListSize", "100");
	}

	else if(txn.compare("GetAssociations") == 0)
	{
		string type = packet->GetVar("type");
		sendPacket = new Packet("asso", 0x80000000);
		sendPacket->SetVar("TXN", txn);
		sendPacket->SetVar("type", type);
		sendPacket->SetVar("domainPartition.domain", packet->GetVar("domainPartition.domain"));
		sendPacket->SetVar("domainPartition.subDomain", packet->GetVar("domainPartition.subDomain"));
		sendPacket->SetVar("owner.id", persona.id);
		sendPacket->SetVar("owner.name", persona.name);
		sendPacket->SetVar("owner.type", "1");

		if(type.compare("PlasmaMute") == 0)
		{
			//TODO: make the mute list database
			sendPacket->SetVar("maxListSize", "20");
			sendPacket->SetVar("members.[]", "0");
		}
		else if(type.compare("PlasmaBlock") == 0)
		{
			//TODO: make the block list database
			sendPacket->SetVar("maxListSize", "20");
			sendPacket->SetVar("members.[]", "0");
		}
		else if(type.compare("PlasmaFriends") == 0)
		{
			//TODO: make the friends list database
			sendPacket->SetVar("maxListSize", "20");
			sendPacket->SetVar("members.[]", "0");
			/*members.[]=6
			members.0.id=243057189
			members.0.name=Ragetank
			members.0.type=1
			members.0.created="Aug-03-2011 09%3a38%3a08 UTC"
			members.0.modified="Aug-03-2011 09%3a38%3a08 UTC"
			members.1.id=245421566
			members.1.name=-MrFrost.-
			members.1.type=1
			members.1.created="Aug-03-2011 09%3a38%3a18 UTC"
			members.1.modified="Aug-03-2011 09%3a38%3a18 UTC"
			members.2.name=Mr.Mustang
			members.2.id=239645767
			members.2.type=1
			members.2.created="Dec-22-2010 00%3a48%3a19 UTC"
			members.2.modified="Dec-22-2010 00%3a48%3a19 UTC"
			members.3.id=329836197
			members.3.name=Drnegative16
			members.3.type=1
			members.3.created="Jul-30-2011 00%3a11%3a06 UTC"
			members.3.modified="Jul-30-2011 00%3a11%3a06 UTC"
			members.4.id=286375350
			members.4.name=Gen.RoadRunner
			members.4.type=1
			members.4.created="Dec-27-2010 10%3a44%3a31 UTC"
			members.4.modified="Dec-27-2010 10%3a44%3a32 UTC"
			members.5.id=335897299
			members.5.name=BaaldEagle
			members.5.type=1
			members.5.created="Jan-26-2012 10%3a18%3a49 UTC"
			members.5.modified="Jan-26-2012 10%3a18%3a49 UTC"*/
		}
		else if(type.compare("PlasmaRecentPlayers") == 0)
		{
			//TODO: make the recent list database
			sendPacket->SetVar("maxListSize", "100");
			sendPacket->SetVar("members.[]", "0");
		}
		else
			debug->warning(1, connectionType, "Could not handle TXN=%s type=%s", txn.c_str(), type.c_str());
	}
	else
		return true;

	if(sendPacket)
		queue->push_back(sendPacket);

	return false;
}

bool GameClient::xmsg(list<Packet*>* queue, Packet* packet, string txn)
{
	Packet* sendPacket = NULL;

	if(txn.compare("ModifySettings") == 0)
	{
		//TODO: modify settings in database
		sendPacket = new Packet("xmsg", 0x80000000);
		sendPacket->SetVar("TXN", txn);
	}

	else if(txn.compare("SendMessage") == 0)	//friend request is being sent
	{
		sendPacket = new Packet("xmsg", 0x80000000);
		sendPacket->SetVar("TXN", txn);
		sendPacket->SetVar("status.0.status", "0");
		sendPacket->SetVar("status.[]", "1");
		sendPacket->SetVar("status.0.userid", packet->GetVar("to.0"));
		sendPacket->SetVar("messageId", "1");	//need to make unique id's for the message?
		/*localizedMessage="Message recipients/to invalid"
		errorContainer.[]=0
		TXN=SendMessage
		errorCode=22006

		<-TXN=SendMessage
messageType=invite/friend
to.[]=1
to.0=838297533
attachments.0.key=response
attachments.0.type=text/plain
attachments.0.data="27,1"
attachments.[]=1
expires=2592000
deliveryType=standard
purgeStrategy=all-recipients

->status.0.status=0
TXN=SendMessage
status.[]=1
status.0.userid=838297533
messageId=289421781.
		*/
	}

	else if(txn.compare("GetMessages") == 0)
	{
		//are there any friend requests?
		sendPacket = new Packet("xmsg", 0x80000000);
		sendPacket->SetVar("TXN", txn);
		sendPacket->SetVar("messages.[]", "0");
		/*xmsg..."...^messages.0.from.type=1
messages.0.attachments.0.type=text/plain
messages.0.messageType=inviteresp/friend
messages.0.from.name=StonySniper
messages.0.purgeStrategy=all-recipients
messages.[]=1
messages.0.deliveryType=standard
messages.0.messageId=284890756
messages.0.timeSent="Jan-17-2013 21%3a26%3a22 UTC"
messages.0.attachments.0.key=response
messages.0.to.0.name=-%3dRick%3d-
messages.0.to.0.id=272593863
messages.0.to.[]=1
messages.0.expiration=2592000
messages.0.from.id=791406346
TXN=GetMessages
messages.0.attachments.[]=1
messages.0.to.0.type=1
messages.0.attachments.0.data=accepted*/
	}
	else
		return true;

	if(sendPacket)
		queue->push_back(sendPacket);

	return false;
}

bool GameClient::pres(list<Packet*>* queue, Packet* packet, string txn)
{
	Packet* sendPacket = NULL;

	if(txn.compare("PresenceSubscribe") == 0)	//is called when client has friends - one packet request for each friend
	{
		sendPacket = new Packet("pres", 0x80000000);
		sendPacket->SetVar("TXN", txn);
		sendPacket->SetVar("responses.[]", "1");
		sendPacket->SetVar("responses.0.outcome", "0");
		sendPacket->SetVar("responses.0.owner.type", "0");
		sendPacket->SetVar("responses.0.owner.id", packet->GetVar("requests.0.userId"));
		sendPacket->SetVar("responses.0.owner.name", "player");
		//(maybe multiple responses if client has multiple personas from same user as friend?)
	}

	else if(txn.compare("SetPresenceStatus") == 0)
	{
		//TODO: make the Presence Status database
		sendPacket = new Packet("pres", 0x80000000);
		sendPacket->SetVar("TXN", txn);
	}
	else
		return true;

	if(sendPacket)
		queue->push_back(sendPacket);

	return false;
}

bool GameClient::recp(list<Packet*>* queue, Packet* packet, string txn)
{
	Packet* sendPacket = NULL;

	if(txn.compare("GetRecord") == 0)
	{
		//TODO: find out what it is, and what to do with it - no difference to logged packets
		//	recordName=clan
		sendPacket = new Packet("recp", 0x80000000);
		sendPacket->SetVar("TXN", txn);
		sendPacket->SetVar("localizedMessage", "\"Record not found\"");
		sendPacket->SetVar("errorContainer.[]", "0");
		sendPacket->SetVar("errorCode", "5000");
	}

	else if(txn.compare("GetRecordAsMap") == 0)			// get all dogtags the persona possesses
	{
		stats_data = new Stats(user.name, persona.name, false, true);
		int count = stats_data->loadDogtags();

		sendPacket = new Packet("recp", 0x80000000);
		sendPacket->SetVar("TXN", txn);
		sendPacket->SetVar("TTL", "0");
		sendPacket->SetVar("state", "1");

		if(count > 0)
		{
			sendPacket->SetVar("values.{}", count);
			sendPacket->SetVar("lastModified", "2011-03-08 09%3a43%3a35.0");	// todo: access the last modified time of the dogtag file and use it here
			Stats::dogtag entry;
			for(int i = 1; i <= count; i++)		// loadDogtags() returns a 1 based value
			{
				string buffer;
				entry = stats_data->getDogtag(i);
				buffer = "values.{";
				buffer.append(entry.id);
				buffer.append("}");
				sendPacket->SetVar(buffer, entry.data);
			}
		}
		else
			sendPacket->SetVar("values.{}", "0");

		delete stats_data;
		stats_data = NULL;
	}

	else if(txn.compare("AddRecord") == 0)	//is called when player gets an enemy dogtag that he hasn't yet?...packet response should be fine though
	{										//or maybe this adds a new record for a player that isn't in the database yet? I got this when I wanted to play with my new persona
		//todo: implement dogtags
		/*TXN=AddRecord
		recordName=dogtags
		values.[]=1
		values.0.key=298916317
		values.0.value=S0lMTEVSQk9XIChOTCkAABaS5kQAAAEAAAAjAA%3d%3d
		TTL=0*/
		sendPacket = new Packet("recp", 0x80000000);
		sendPacket->SetVar("TXN", txn);
	}
	else if(txn.compare("UpdateRecord") == 0)	//is called when a player gets an enemy dogtag but already has some of them (then why was this called during testing)? (thats at least one action that triggers this) - packet response should be fine though
	{
		//todo: implement dogtags
		/*TXN=UpdateRecord
		recordName=dogtags
		values.[]=1
		values.0.key=334048277
		values.0.value=U3phbWFuMTg1X1BMAAAAAC+S5kQAAAEAAAAdAA%3d%3d
		TTL=0
		remove.[]=0*/
		Stats::dogtag newDogtag;
		newDogtag.id = packet->GetVar("values.0.key");
		newDogtag.data = packet->GetVar("values.0.value");

		stats_data = new Stats(user.name, persona.name, false, true);
		stats_data->loadDogtags();
		stats_data->addDogtag(newDogtag);
		stats_data->saveDogtags();
		delete stats_data;
		stats_data = NULL;

		sendPacket = new Packet("recp", 0x80000000);
		sendPacket->SetVar("TXN", txn);
	}
	else
		return true;

	if(sendPacket)
		queue->push_back(sendPacket);

	return false;
}

bool GameClient::pnow(list<Packet*>* queue, Packet* packet, string txn)
{

	Packet* sendPacket = NULL;
	if(txn.compare("Start") == 0)		// User wants us to select a server for him to automatically join
	{
		sendPacket = new Packet("pnow", 0x80000000);
		sendPacket->SetVar("TXN", txn);
		//int id = rand() % 5000 + 1;
		string id = "1";
		sendPacket->SetVar("id.id", id);	// what id is this? for now put constant number
		sendPacket->SetVar("id.partition", "/eagames/BFBC2");
		queue->push_back(sendPacket);

		sendPacket = new Packet("pnow", 0x80000000, false);
		sendPacket->SetVar("TXN", "Status");
		sendPacket->SetVar("id.id", id);
		sendPacket->SetVar("id.partition", "/eagames/BFBC2");
		sendPacket->SetVar("sessionState", "COMPLETE");
		sendPacket->SetVar("props.{}", "2");
		sendPacket->SetVar("props.{resultType}", "JOIN");

		list<linked_key> filters;
		linked_key tempFilter;
		list<int> filtered;

		string temp = packet->GetVar("players.0.props.{filter-gameMod}");
		if(!temp.empty())
		{
			tempFilter.key = temp;
			tempFilter.id = GAME_MOD;
			filters.push_back(tempFilter);
		}
		temp = packet->GetVar("players.0.props.{filter-gamemode}");
		if(!temp.empty() && temp.find_first_of('|') == string::npos)	// if a '|' is in the string then all game modes are allowed so we dont need to filter
		{
			tempFilter.key = temp;
			tempFilter.id = GAMEMODE;
			filters.push_back(tempFilter);
		}
		temp = packet->GetVar("players.0.props.{pref-level}");
		if(!temp.empty())
		{
			tempFilter.key = temp;
			tempFilter.id = SERVER_LEVEL;
			filters.push_back(tempFilter);
		}
		database->listMatchingGames(&filters, &filtered);

		if(!filtered.empty())
		{
			sendPacket->SetVar("props.{games}.0.fit", "1001");
			sendPacket->SetVar("props.{games}.0.gid", filtered.front());
			sendPacket->SetVar("props.{games}.0.lid", database->getLobbyInfo(LOBBY_ID));
			sendPacket->SetVar("props.{games}.[]", "1");	// we only filter one game out
		}
		else
			sendPacket->SetVar("props.{games}.[]", "0");
	}
	else
		return true;

	if(sendPacket)
		queue->push_back(sendPacket);

	return false;
}

int GameClient::rank(list<Packet*>* queue, Packet* packet, string txn)
{
	Packet* sendPacket = NULL;
	int state = NORMAL;

	if(txn.compare("GetStats") == 0)		// get Stats for current persona
	{
		stats_data = new Stats(user.name, persona.name);
		sendPacket = new Packet("rank", 0x80000000);
		sendPacket->SetVar("TXN", txn);

		string buffer = packet->GetVar("keys.[]");
		int count = lexical_cast<int>(buffer);
		sendPacket->SetVar("stats.[]", buffer);

		char iterBuf[24];
		for(int i = 0; i < count; i++)
		{
			sprintf(iterBuf, "keys.%i", i);
			buffer = packet->GetVar(iterBuf);
			string value = stats_data->getKey(buffer);

			sprintf(iterBuf, "stats.%i.key", i);
			sendPacket->SetVar(iterBuf, buffer);

			sprintf(iterBuf, "stats.%i.value", i);
			sendPacket->SetVar(iterBuf, value);
		}
		delete stats_data;
		stats_data = NULL;
	}

	else if(txn.compare("UpdateStats") == 0)
	{
		//this is coming sometimes from the server (not on EA though?) so we have to accept only u.0.ot=1 to ignore that case
		//rank 0xc000001c {TXN=UpdateStats, u.0.o=100, u.0.ot=11, u.0.s.[]=1, u.0.s.0.ut=3, u.0.s.0.k=veteran, u.0.s.0.v=1.0000, u.0.s.0.pt=0, u.[]=1}
		if(packet->GetVar("u.0.ot").compare("1") == 0)// && !fw->emuCfg().override_stats_with_template)
		{
			int playerPersonaId = lexical_cast<int>(packet->GetVar("u.0.o"));
			list_entry result;

			if(database->getPersona(playerPersonaId, &result))
			{
				Stats* stats_data = new Stats(result.data[USER_EMAIL], result.name);
				int count = lexical_cast<int>(packet->GetVar("u.0.s.[]"));
				double temp_score = 0.0f;
				char buffer[16];

				for(int i = 0; i < count; i++)
				{
					sprintf(buffer, "u.0.s.%i.k", i);		//is now key name
					string key = packet->GetVar(buffer);
					sprintf(buffer, "u.0.s.%i.ut", i);
					int updateMethod = lexical_cast<int>(packet->GetVar(buffer));
					sprintf(buffer, "u.0.s.%i.v", i);		//is now value of key
					string value = packet->GetVar(buffer);
					string old = stats_data->getKey(key);

					if(updateMethod == 3 && !old.empty() && !value.empty())			//1 = absolute value, 3 = relative value, 0 = absolute value (should round it)
					{
						double old_value = atof(old.c_str());
						double new_value = atof(value.c_str());

						if(key.compare("sc_assault") == 0 || key.compare("sc_award") == 0 || key.compare("sc_demo") == 0 || key.compare("sc_recon") == 0 || key.compare("sc_support") == 0 || key.compare("sc_vehicle") == 0)
						{
							temp_score += new_value;
							//debug->notification(3, connectionType, "Score relevant key \"%s\", value=%.4f, temp_score=%.4f", key.c_str(), new_value, temp_score);
						}

						new_value += old_value;
						sprintf(buffer, "%.4f", new_value);
						value = buffer;
					}
					strcpy(buffer, old.c_str());
					debug->notification(3, connectionType, "Updated key %s with value \"%s\", old value=\"%s\", updateMethod=%i", key.c_str(), value.c_str(), buffer, updateMethod);
					stats_data->setKey(key, value, true);
					// normally only webstats related keys are not found
					/*if(!stats_data->setKey(key, value, true))		// store which keys were not found
					{
						fstream file("./templates/ignoredKeys", fstream::in | fstream::out);
						if(file.is_open() && file.good())
						{
							stringstream strmbuf;
							strmbuf << file.rdbuf();
							string unknownKeys = strmbuf.str();
							key.insert(0, "\"");
							key.append("\"\n");
							int result = unknownKeys.find(key, 0);
							if(result == string::npos)
								file.write(key.c_str(), key.size());

							file.close();
							key.pop_back();		//we dont want the newline at the end to be printed
							//key.erase(key.size()-1, 1);
							if(result != string::npos)
								debug->notification(4, connectionType, "Key %s is already in ignoredKeys file", key.c_str());
							else
								debug->notification(3, connectionType, "Added key %s to ignoredKeys file", key.c_str());
						}
					}*/
				}

				if(temp_score != 0.0f)
				{
					double score = atof(stats_data->getKey("score").c_str());
					score += temp_score;
					sprintf(buffer, "%.4f", score);
					stats_data->setKey("score", buffer);
				}
				stats_data->saveStats();
				delete stats_data;
			}
			else
				debug->warning(1, connectionType, "Couldn't find persona id %i in database!!", playerPersonaId);
		}

		sendPacket = new Packet("rank", 0x80000000);
		sendPacket->SetVar("TXN", txn);
	}

	else if(txn.compare("GetRankedStats") == 0)				// User wants to see worldwide Stats
	{
		sendPacket = new Packet("rank", 0x80000000);
		sendPacket->SetVar("TXN", txn);
		// not sure why this is being sent but it's like this in the logged traffic
		sendPacket->SetVar("errorCode", "99");
		sendPacket->SetVar("errorContainer.[]", "0");
		sendPacket->SetVar("localizedMessage", "\"System Error\"");
		/*
<-TXN=GetRankedStats
owner=230518387
ownerType=1
periodId=0
periodPast=0
keys.0=score
rankOrders.0=0
keys.1=rank
rankOrders.1=0
keys.2=form
rankOrders.2=0
keys.3=level
rankOrders.3=0
keys.4=kills
rankOrders.4=0
keys.5=deaths
rankOrders.5=0
keys.6=sc_squad
rankOrders.6=0
keys.7=time
rankOrders.7=0
keys.8=veteran
rankOrders.8=0
keys.9=elo
rankOrders.9=0
keys.10=score
rankOrders.10=0
keys.[]=11
rankOrders.[]=11

->stats.[]=9
stats.3.text=20100316
stats.1.value=28.0
stats.0.value=851985.0
stats.5.value=2993.0
stats.7.value=372294.71
stats.4.key=kills
stats.5.key=deaths
stats.2.rank=220437
stats.8.key=score
stats.1.rank=250001
stats.6.key=sc_squad
stats.0.key=score
stats.1.key=rank
stats.4.rank=250001
stats.2.value=1.0
stats.6.rank=250001
stats.7.key=time
stats.0.rank=250001
stats.5.rank=250001
stats.6.value=42955.0
stats.8.rank=250001
stats.3.rank=187362
stats.8.value=851985.0
TXN=GetRankedStats
stats.2.key=form
stats.4.value=3088.0
stats.7.rank=250001
stats.3.value=145.0
stats.3.key=level*/
	}

	else if(txn.compare("GetRankedStatsForOwners") == 0)	// User wants to see himself on worldwide stats?
	{
		sendPacket = new Packet("rank", 0x80000000);
		sendPacket->SetVar("TXN", txn);
		// not sure why this is being sent but it's like this in the logged traffic
		sendPacket->SetVar("errorCode", "99");
		sendPacket->SetVar("errorContainer.[]", "0");
		sendPacket->SetVar("localizedMessage", "\"System Error\"");
		/*
<-TXN=GetRankedStatsForOwners
owners.[]=1
owners.0.ownerId=230518387
owners.0.ownerType=1
periodId=0
keys.0=score
keys.1=rank
keys.2=form
keys.3=level
keys.4=kills
keys.5=deaths
keys.6=sc_squad
keys.7=time
keys.8=veteran
keys.9=elo
keys.10=" score"
keys.[]=11

->rankedStats.0.ownerType=1
rankedStats.0.rankedStats.5.value=2993.0
rankedStats.[]=1
rankedStats.0.rankedStats.3.text=20100316
rankedStats.0.rankedStats.6.rank=250001
rankedStats.0.rankedStats.3.value=145.0
rankedStats.0.rankedStats.6.key=sc_squad
rankedStats.0.rankedStats.5.key=deaths
rankedStats.0.rankedStats.7.value=372294.71
rankedStats.0.rankedStats.3.rank=187362
rankedStats.0.rankedStats.2.rank=220437
rankedStats.0.rankedStats.2.key=form
rankedStats.0.rankedStats.1.rank=250001
rankedStats.0.rankedStats.0.value=851985.0
rankedStats.0.ownerId=230518387
rankedStats.0.rankedStats.4.value=3088.0
rankedStats.0.rankedStats.4.key=kills
rankedStats.0.rankedStats.4.rank=250001
rankedStats.0.rankedStats.3.key=level
rankedStats.0.rankedStats.[]=8
rankedStats.0.rankedStats.0.rank=250001
rankedStats.0.rankedStats.0.key=score
rankedStats.0.rankedStats.1.key=rank
rankedStats.0.rankedStats.5.rank=250001
rankedStats.0.rankedStats.6.value=42955.0
rankedStats.0.rankedStats.2.value=1.0
rankedStats.0.rankedStats.7.rank=250001
TXN=GetRankedStatsForOwners
rankedStats.0.rankedStats.7.key=time
rankedStats.0.rankedStats.1.value=28.0*/
	}

	else if(txn.compare("GetTopNAndStats") == 0)
	{
		sendPacket = new Packet("rank", 0x80000000);
		sendPacket->SetVar("TXN", txn);
		/*
<-TXN=GetTopNAndStats
key=score
ownerType=1
minRank=250001
maxRank=250100
periodId=0
periodPast=0
rankOrder=0
keys.0=score
keys.1=rank
keys.2=form
keys.3=level
keys.4=kills
keys.5=deaths
keys.6=sc_squad
keys.7=time
keys.8=veteran
keys.9=elo
keys.[]=10

->stats.[]=0
TXN=GetTopNAndStats

=================
<-TXN=GetTopNAndStats
key=score
ownerType=1
minRank=1
maxRank=100
periodId=0
periodPast=0
rankOrder=0
keys.0=score
keys.1=rank
keys.2=form
keys.3=level
keys.4=kills
keys.5=deaths
keys.6=sc_squad
keys.7=time
keys.8=veteran
keys.9=elo
keys.[]=10

->stats.26.addStats.6.key=sc_squad
stats.1.addStats.8.value=7.0
stats.73.addStats.5.key=deaths
stats.50.addStats.7.value=8821996.97
stats.50.addStats.8.key=veteran
stats.8.addStats.3.value=0.0
stats.59.addStats.0.key=score
stats.49.addStats.3.text=20100315
stats.83.addStats.1.key=rank
stats.61.addStats.3.value=0.0
stats.97.addStats.2.key=form
stats.0.addStats.[]=10
stats.85.addStats.4.key=kills
stats.1.addStats.0.value=6.8625345E7
stats.5.addStats.4.key=kills
stats.52.name=leytenant
...
*/
	}
	else
		state = UNKNOWN;

	if(sendPacket)
		queue->push_back(sendPacket);

	return state;
}


int GameClient::ProcessTheater(list<Packet*>* queue, Packet* packet, char* type, const char* ip)
{
	Packet* sendPacket = NULL;
	int state = NORMAL;

	if(strcmp(type, "CONN") == 0)
	{
		sendPacket = new Packet("CONN", 0x00000000);
		sendPacket->SetVar("TID", ++sock_tid);
		sendPacket->SetVar("TIME", (int)time(NULL));
		sendPacket->SetVar("activityTimeoutSecs", "240");
		sendPacket->SetVar("PROT", packet->GetVar("PROT"));
	}

	else if(strcmp(type, "PING") == 0)
	{
		state = SKIP;
	}

	else if(strcmp(type, "USER") == 0)
	{
		persona_lkey = packet->GetVar("LKEY");
		int id = database->theaterLogin(persona_lkey);

		if(id > 0)
		{
			database->getPersona(id, &persona);
			sendPacket = new Packet("USER", 0x00000000);
			sendPacket->SetVar("TID", ++sock_tid);
			sendPacket->SetVar("NAME", persona.name);
		}
		else
			state = SKIP;
	}

	else if(strcmp(type, "LLST") == 0)		//Lobby List
	{
		sendPacket = new Packet("LLST", 0x00000000);
		sendPacket->SetVar("TID", ++sock_tid);
		sendPacket->SetVar("NUM-LOBBIES", "1");
		queue->push_back(sendPacket);

		sendPacket = new Packet("LDAT", 0x00000000);
		sendPacket->SetVar("TID", sock_tid);
		sendPacket->SetVar("LID", database->getLobbyInfo(LOBBY_ID));
		sendPacket->SetVar("PASSING", database->getLobbyGames());
		sendPacket->SetVar("NAME", database->getLobbyInfo(LOBBY_NAME));
		sendPacket->SetVar("LOCALE", database->getLobbyInfo(LOBBY_LOCALE));
		sendPacket->SetVar("MAX-GAMES", database->getLobbyInfo(LOBBY_MAX_GAMES));
		sendPacket->SetVar("FAVORITE-GAMES", "0");	//TODO: find out how it works
		sendPacket->SetVar("FAVORITE-PLAYERS", "0");	//TODO: find out how it works
		sendPacket->SetVar("NUM-GAMES", database->getLobbyGames());
	}

	else if(strcmp(type, "GLST") == 0)		//Game List
	{
		list<int> filtered;
		int limit = lexical_cast<int>(packet->GetVar("COUNT"));
		if(limit < 0)
			limit = 500;

		string gid = packet->GetVar("GID");
		if(!gid.empty())	// client is joining a specific server, get information about it
		{
			// there were filters in here before but why should we need them if there is an id?
			/*int tempId = lexical_cast<int>(gid);
			if(database->isValidGid(tempId))
				filtered.push_back(tempId);*/
			// actually I'm not sure if we should do anything here, normally the given gid is invalid or not part of the wanted games
		}
		else
		{
			if(fw->emuCfg().enable_server_filters)
				filterGames(packet, &filtered);
			else
				database->listAllGames(&filtered);
		}

		//todo: make filters
		sendPacket = new Packet("GLST", 0x00000000);
		sendPacket->SetVar("TID", ++sock_tid);
		sendPacket->SetVar("LID", packet->GetVar("LID"));
		sendPacket->SetVar("LOBBY-NUM-GAMES", database->getLobbyGames());
		sendPacket->SetVar("LOBBY-MAX-GAMES", database->getLobbyInfo(LOBBY_MAX_GAMES));
		sendPacket->SetVar("NUM-GAMES", (int)filtered.size());

		while(!filtered.empty() && limit > 0)
		{
			queue->push_back(sendPacket);

			int id = filtered.front();
			string* game = database->getGameData(filtered.front(), false);
			sendPacket = new Packet("GDAT", 0x00000000);
			sendPacket->SetVar("TID", sock_tid);
			sendPacket->SetVar("LID", database->getLobbyInfo(LOBBY_ID));	// id of lobby
			sendPacket->SetVar("GID", id);									// id of game/server

			string persona_name, persona_id;
			if(game[PLATFORM].compare("PC") == 0)	// I know these checks are consistent but it's better than nothing
			{
				persona_name = "bfbc2.server.p";
				persona_id = "1";
			}
			else if(game[PLATFORM].compare("ps3") == 0)
			{
				persona_name = "bfbc.server.ps";
				persona_id = "2";
			}
			else
			{
				persona_name = "bfbc.server.xe";
				persona_id = "3";
			}
			sendPacket->SetVar("HN", persona_name);			// account name of server (host name)
			sendPacket->SetVar("HU", persona_id);			// account id of server (host user)
			sendPacket->SetVar("N", game[SERVER_NAME]);		// name of server in list

			if(!fw->isEmuLocal())	// a public IP is in the config so we have to determine in what network the server is
			{
				string serverIp = game[IP];

				if(!isLocal && serverIp.compare(ip) != 0)			// client is outside and remote server ip is not equal to the client ip
					sendPacket->SetVar("I", serverIp);
				else if(!isLocal && serverIp.compare(ip) == 0)		// client is outside but server has the same ip as the client so they must be in the same network
					sendPacket->SetVar("I", game[INTERNAL_IP]);
				else if(isLocal && serverIp.compare(fw->portsCfg().emulator_ip) == 0)	// client is local but server has same public ip as emulator so they must be in the same network
					sendPacket->SetVar("I", game[INTERNAL_IP]);
				else	// client is local so check in what network the server is
				{
					if(!fw->isIpLocal(serverIp))		// server is in another network so send public ip to client
						sendPacket->SetVar("I", serverIp);				// IP (probably outside server ip)
					else					// everyone is local
						sendPacket->SetVar("I", game[INTERNAL_IP]);		// we want this if the server is in the same network as the client
				}
			}
			else
				sendPacket->SetVar("I", game[IP]);				// IP (probably outside server ip)

			sendPacket->SetVar("P", game[PORT]);			// Port
			sendPacket->SetVar("JP", game[JOINING_PLAYERS]);// players that are joining the server right now?
			sendPacket->SetVar("QP", game[QUEUE_LENGTH]);	// something with the queue...lets just set this equal to B-U-QueueLength
			sendPacket->SetVar("AP", game[ACTIVE_PLAYERS]);	// current number of players on server
			sendPacket->SetVar("MP", game[MAX_PLAYERS]);	// Maximum players on server
			sendPacket->SetVar("PL", game[PLATFORM]);		// Platform - PC / XENON / PS3

			// constants (at least I never saw any other values in the logged traffic)
			sendPacket->SetVar("F", "0");					// ???
			sendPacket->SetVar("NF", "0");					// ???
			sendPacket->SetVar("J", game[JOIN]);			// ??? constant value - "O"
			sendPacket->SetVar("TYPE", game[TYPE]);			// what type?? constant value - "G"
			sendPacket->SetVar("PW", "0");					// ??? - its certainly not something like "hasPassword"

			// other server specific values
			sendPacket->SetVar("B-U-Softcore", game[SOFTCORE]);			// Game is softcore - what does that mean?
			sendPacket->SetVar("B-U-Hardcore", game[HARDCORE]);			// Game is hardcore
			sendPacket->SetVar("B-U-HasPassword", game[HAS_PASSWORD]);	// Game has password
			sendPacket->SetVar("B-U-Punkbuster", game[PUNKBUSTER]);		// Game has punkbuster?
			sendPacket->SetVar("B-U-EA", game[EA]);						// is server EA Orginal?

			sendPacket->SetVar("B-version", game[VERSION]);		// Version of the server (exact version) - TRY TO CONNECT TO ACTUAL VERSION OF SERVER
			sendPacket->SetVar("V", game[CLIENT_VERSION]);		// "clientVersion" of server (shows up in server log on startup)
			sendPacket->SetVar("B-U-level", game[SERVER_LEVEL]);// current map of server
			sendPacket->SetVar("B-U-gamemode", game[GAMEMODE]);	// Gameplay Mode (Conquest, Rush, SQDM,  etc)
			sendPacket->SetVar("B-U-sguid", game[SGUID]);		// Game PB Server GUID?
			sendPacket->SetVar("B-U-Time", game[TIME]);			// uptime of server?
			sendPacket->SetVar("B-U-hash", game[HASH]);			// Game hash?
			sendPacket->SetVar("B-U-region", game[REGION]);		// Game region
			sendPacket->SetVar("B-U-public", game[IS_PUBLIC]);	// Game is public
			sendPacket->SetVar("B-U-elo", game[ELO]);			// value that determines how good the players on the server are?

			// also constant but could be changed? (is not implemented in the game anyway)
			sendPacket->SetVar("B-numObservers", game[NUM_OBSERVERS]);	// observers = spectators? or admins?
			sendPacket->SetVar("B-maxObservers", game[MAX_OBSERVERS]);	// Game max observers

			sendPacket->SetVar("B-U-Provider", game[PROVIDER]);			// provider id, figured out by server
			sendPacket->SetVar("B-U-gameMod", game[GAME_MOD]);			// maybe different value for vietnam here?
			sendPacket->SetVar("B-U-QueueLength", game[QUEUE_LENGTH]);	// players in queue or maximum queue length? (sometimes smaller than QP (-1?))

			if(game[PUNKBUSTER].compare("1") == 0)
				sendPacket->SetVar("B-U-PunkBusterVersion", game[PUNKBUSTER_VERSION]);

			filtered.pop_front();
			limit--;
		}
	}

	else if(strcmp(type, "GDAT") == 0)
	{
		string lobbyID = packet->GetVar("LID");
		string gameID = packet->GetVar("GID");
		if(lobbyID.empty() || gameID.empty())
		{
			sendPacket	= new Packet("GDAT", 0x00000000);
			debug->warning(3, connectionType, "GDAT - GID or LID is null");
			sendPacket->SetVar("TID", ++sock_tid);
		}
		else
		{
			GamesEntry server;
			int gid = lexical_cast<int>(gameID);
			server.key = database->getGameData(gid, false);
			server.gdet = database->getGameData(gid, true);

			if(server.key && server.gdet)
			{
				// TODO: implement queues (properly)
				if(queue_position > 0)
				{
					int cur_queue_length = lexical_cast<int>(server.key[QUEUE_LENGTH]);
					debug->notification(3, connectionType, "Client is still in queue, prev_queue_length=%i, cur_queue_length=%i, queue_position=%i", original_queue_length, cur_queue_length, queue_position);
					if(original_queue_length != cur_queue_length)
					{
						if(cur_queue_length < original_queue_length)
							queue_position -= original_queue_length - cur_queue_length;
						else
							debug->notification(3, connectionType, "Someone joined the queue, don't update pos");
						original_queue_length = cur_queue_length;
						debug->notification(3, connectionType, "Queue length has changed, queue_position=%i", original_queue_length, queue_position);
					}
					// if queue_length is smaller than queue_pos then that means a player got it (we were at the end of the queue previously right?
					// why was QPOS=0 and QLEN=1 when I tried it?
					sendPacket = new Packet("QLEN", 0x00000000);
					sendPacket->SetVar("QPOS", queue_position-1);		// is this one off??
					sendPacket->SetVar("QLEN", original_queue_length);
					sendPacket->SetVar("LID", lobbyID);
					sendPacket->SetVar("GID", gameID);
					queue->push_back(sendPacket);
				}
				sendPacket = new Packet("GDAT", 0x00000000);// interesting...no 'F' or 'NF' required here or 'B-U-PunkBusterVersion'
				sendPacket->SetVar("TID", ++sock_tid);
				sendPacket->SetVar("LID", lobbyID);			// is the LID always in this specific packet? if not take the LID from the database entry
				sendPacket->SetVar("GID", gameID);

				string persona_name, persona_id;
				if(server.key[PLATFORM].compare("PC") == 0)	// I know this check is anything but consistent but it's better than nothing
				{
					persona_name = "bfbc2.server.p";
					persona_id = "1";
				}
				else if(server.key[PLATFORM].compare("ps3") == 0)
				{
					persona_name = "bfbc.server.ps";
					persona_id = "2";
				}
				else
				{
					persona_name = "bfbc.server.xe";
					persona_id = "3";
				}

				sendPacket->SetVar("HU", persona_id);					// account id of server (host user)
				sendPacket->SetVar("HN", persona_name);					// account name of server (host name)
				if(!fw->isEmuLocal())   // a public IP is in the config so we have to determine in what network the server is
				{
					string serverIp = server.key[IP];

					if(!isLocal && serverIp.compare(ip) != 0)			// client is outside and remote server ip is not equal to the client ip
						sendPacket->SetVar("I", serverIp);
					else if(!isLocal && serverIp.compare(ip) == 0)		// client is outside but server has the same ip as the client so they must be in the same network
						sendPacket->SetVar("I", server.key[INTERNAL_IP]);	// we want this if the server is in the same network as the client
					else if(isLocal && serverIp.compare(fw->portsCfg().emulator_ip) == 0)	// client is local but server has same public ip as the emulator so they must be in the same network
						sendPacket->SetVar("I", server.key[INTERNAL_IP]);
					else	// client is local so check in what network the server is
					{
						if(!fw->isIpLocal(serverIp))		// server is in another network so send public ip to client
							sendPacket->SetVar("I", serverIp);				// IP (probably outside server ip)
						else					// everyone is local
							sendPacket->SetVar("I", server.key[INTERNAL_IP]);	// we want this if the server is in the same network as the client
					}
				}
				else
					sendPacket->SetVar("I", server.key[IP]);				// IP (probably outside server ip)
				sendPacket->SetVar("P", server.key[PORT]);				// server Port
				sendPacket->SetVar("N", server.key[SERVER_NAME]);		// name of server in list
				sendPacket->SetVar("AP", server.key[ACTIVE_PLAYERS]);	// current number of players on server(Active players)
				sendPacket->SetVar("MP", server.key[MAX_PLAYERS]);		// Maximum players on server
				sendPacket->SetVar("QP", server.key[QUEUE_LENGTH]);		// something with the queue...lets just set this equal to B-U-QueueLength
				sendPacket->SetVar("JP", server.key[JOINING_PLAYERS]);	// players that are joining the server right now?
				sendPacket->SetVar("PL", server.key[PLATFORM]);			// Platform - PC / XENON / PS3

				// constant:
				sendPacket->SetVar("PW", "0");					// ???
				sendPacket->SetVar("TYPE", server.key[TYPE]);	// always "G"?
				sendPacket->SetVar("J", server.key[JOIN]);		// always "O"?

				// Userdata
				sendPacket->SetVar("B-U-Softcore", server.key[SOFTCORE]);		// Game is softcore - what does that mean?
				sendPacket->SetVar("B-U-Hardcore", server.key[HARDCORE]);		// Game is hardcore
				sendPacket->SetVar("B-U-HasPassword", server.key[HAS_PASSWORD]);// Game has password
				sendPacket->SetVar("B-U-Punkbuster", server.key[PUNKBUSTER]);	// Game has punkbuster?
				sendPacket->SetVar("B-U-EA", server.key[EA]);					// is server EA Orginal

				sendPacket->SetVar("B-version", server.key[VERSION]);		// Version of the server (exact version) - TRY TO CONNECT TO ACTUAL VERSION OF SERVER
				sendPacket->SetVar("V", server.key[CLIENT_VERSION]);		// "clientVersion" of server (shows up in server log)
				sendPacket->SetVar("B-U-level", server.key[SERVER_LEVEL]);	// current map of server
				sendPacket->SetVar("B-U-gamemode", server.key[GAMEMODE]);	// Gameplay Mode (Conquest, Rush, SQDM,  etc)
				sendPacket->SetVar("B-U-sguid", server.key[SGUID]);			// Game PB Server GUID?
				sendPacket->SetVar("B-U-Time", server.key[TIME]);			// uptime of server?
				sendPacket->SetVar("B-U-hash", server.key[HASH]);			// Game hash?
				sendPacket->SetVar("B-U-region", server.key[REGION]);		// Game region
				sendPacket->SetVar("B-U-public", server.key[IS_PUBLIC]);	// Game is public
				sendPacket->SetVar("B-U-elo", server.key[ELO]);				// value that determines how good the players on the server are?

				sendPacket->SetVar("B-numObservers", server.key[NUM_OBSERVERS]);	// observers = spectators? or admins?
				sendPacket->SetVar("B-maxObservers", server.key[MAX_OBSERVERS]);	// Game max observers
				sendPacket->SetVar("B-U-Provider", server.key[PROVIDER]);			// provider id, figured out by server
				sendPacket->SetVar("B-U-gameMod", server.key[GAME_MOD]);			// maybe different value for vietnam here?
				sendPacket->SetVar("B-U-QueueLength", server.key[QUEUE_LENGTH]);	// players in queue or maximum queue length? (sometimes smaller than QP (-1?))
				queue->push_back(sendPacket);

				sendPacket = new Packet("GDET", 0x00000000);
				sendPacket->SetVar("TID", sock_tid);
				sendPacket->SetVar("LID", lobbyID);
				sendPacket->SetVar("GID", gameID);

				sendPacket->SetVar("D-AutoBalance", server.gdet[AUTOBALANCE]);
				sendPacket->SetVar("D-Crosshair", server.gdet[CROSSHAIR]);
				sendPacket->SetVar("D-FriendlyFire", server.gdet[FRIENDLY_FIRE]);
				sendPacket->SetVar("D-KillCam", server.gdet[KILLCAM]);
				sendPacket->SetVar("D-Minimap", server.gdet[MINIMAP]);
				sendPacket->SetVar("D-MinimapSpotting", server.gdet[MINIMAP_SPOTTING]);
				sendPacket->SetVar("UGID", server.key[UGID]);

				string serverDescription = server.gdet[SERVER_DESCRIPTION];
				int count = 0;
				if(!serverDescription.empty())
				{
					for(int i = 0, pos = 0; pos < (int)serverDescription.size(); i++, pos += 63)	// 63 is max limit of one description "cell"?
					{
						char buffer[24];
						sprintf(buffer, "D-ServerDescription%i", i);
						int length = 63;
						if(pos+length > (int)serverDescription.size())
							length = serverDescription.size()-pos;

						string splittedDescr = serverDescription.substr(pos, length);
						sendPacket->SetVar(buffer, splittedDescr);
						count++;
					}
				}
				sendPacket->SetVar("D-ServerDescriptionCount", count);
				if(!server.gdet[BANNERURL].empty())
					sendPacket->SetVar("D-BannerUrl", server.gdet[BANNERURL]);
				sendPacket->SetVar("D-ThirdPersonVehicleCameras", server.gdet[THIRD_PERSON_VEHICLE_CAMERAS]);
				sendPacket->SetVar("D-ThreeDSpotting", server.gdet[THREE_D_SPOTTING]);

				for(int i = 0; i < 32; i++)	//get all the player names that are on the server so we can get their id's
				{
					string entry = server.gdet[PDAT00+i];
					if(entry.compare("|0|0|0|0") != 0)	//when the column contains a different value than the default then we have a player
					{
						string name = entry;
						int end = name.find_first_of('|');
						name.erase(end, name.size()-end);
						debug->notification(4, connectionType, "player name on server: %s", name.c_str());
						playerList.push_back(name);
					}

					char buffer[12];
					if(i < 10)
						sprintf(buffer, "D-pdat0%i", i);
					else
						sprintf(buffer, "D-pdat%i", i);
					sendPacket->SetVar(buffer, entry);
				}
				queue->push_back(sendPacket);

				while(!playerList.empty())	//gather all the information of the players that are already on the server and put them into packets
				{
					list_entry tempPersona;
					string name = playerList.front();

					if(database->getPersona(name, &tempPersona))
					{
						sendPacket = new Packet("PDAT", 0x00000000);
						sendPacket->SetVar("NAME", name);
						sendPacket->SetVar("TID", sock_tid);
						if(tempPersona.data)
							sendPacket->SetVar("PID", tempPersona.data[PLAYER_ID]);	//id of the player on the server
						else
							debug->warning(2, connectionType, "tempPersona with id %i contains no data!!", tempPersona.id);
						sendPacket->SetVar("UID", tempPersona.id);		//this is definitely persona_id
						sendPacket->SetVar("LID", lobbyID);
						sendPacket->SetVar("GID", gameID);
						queue->push_back(sendPacket);
					}
					else
					{
						debug->warning(2, connectionType, "GDAT no persona with name \"%s\" found!", name.c_str());
						break;
					}
					playerList.pop_front();
				}
				sendPacket = NULL;
			}
			else	// invalid GID
			{
				sendPacket	= new Packet("GDAT", 0x00000000);
				sendPacket->SetVar("TID", ++sock_tid);
			}
		}
	}

	else if(strcmp(type, "EGAM") == 0)
	{
		debug->notification(4, connectionType, "Client requesting to join server");
		string lobbyID = packet->GetVar("LID");
		string gameID = packet->GetVar("GID");
		int gid = lexical_cast<int>(gameID);

		bool server_is_full = false;
		string* game = database->getGameData(gid, false);
		if(game)
		{
			bool isServerLocal = false;
			int temp_players = lexical_cast<int>(game[ACTIVE_PLAYERS]);
			int max_players = lexical_cast<int>(game[MAX_PLAYERS]);
			if(temp_players+1 > max_players)
			{
				temp_players = lexical_cast<int>(game[QUEUE_LENGTH])+1;
				game[QUEUE_LENGTH] = lexical_cast<string>(temp_players);
				server_is_full = true;
			}

			if(!server_is_full)
			{
				sendPacket = new Packet("EGAM", 0x00000000);
				sendPacket->SetVar("TID", ++sock_tid);
				sendPacket->SetVar("LID", lobbyID);
				sendPacket->SetVar("GID", gameID);
			}
			else
			{
				original_queue_length = queue_position = temp_players;	// if we get here, temp_players already contains the queue_length value of the database
				debug->notification(3, connectionType, "Client got queued, queue_length=%i, queue_position=%i", original_queue_length, queue_position);

				sendPacket = new Packet("EGAM", 0x71756575);	// send EGAMqueu if server is full, header: 0x71756575
				sendPacket->SetVar("TID", ++sock_tid);
				// a new player is automatically at the end of a queue, right? but why was QPOS=1 and QLEN=2 when I tried it? simply put +/-1 for testing purposes
				sendPacket->SetVar("QPOS", queue_position);		// position of client in queue?
				sendPacket->SetVar("QLEN", queue_position+1);	// the whole length of the queue (probably) or how much players are currently in the queue?
				sendPacket->SetVar("LID", lobbyID);
				sendPacket->SetVar("GID", gameID);
			}

			queue->push_back(sendPacket);
			// put a mutex here?
			TcpConnection* server_socket = fw->getServerSocket(gid);
			persona.data[PLAYER_ID] = lexical_cast<string>(server_socket->getGameServer()->addPlayerId());	// player_id is a unique id of the player on the server (just a count of all players that connected)
			string ticket = fw->randomString(10, SEED_NUMBERS);

			if(server_socket)
			{
				// this packet gets sent to the SERVER the client connects to, it contains information about the client
				if(!server_is_full)
				{
					sendPacket = new Packet("EGRQ", 0x00000000);
					sendPacket->SetVar("R-INT-PORT", packet->GetVar("R-INT-PORT"));
					sendPacket->SetVar("R-INT-IP", packet->GetVar("R-INT-IP"));	// internal ip where the CLIENT is hosted
					sendPacket->SetVar("PORT", packet->GetVar("PORT"));
					sendPacket->SetVar("NAME", persona.name);
					sendPacket->SetVar("PTYPE", packet->GetVar("PTYPE"));
					sendPacket->SetVar("TICKET", ticket);
					sendPacket->SetVar("PID", persona.data[PLAYER_ID]);
					sendPacket->SetVar("UID", persona.id);						// this has to be persona_id

					if(!fw->isEmuLocal())   // a public IP is in the config so we have to determine in what network the server is
					{
						string serverIp = game[IP];

						if(!isLocal && serverIp.compare(ip) != 0)			// client is outside and remote server ip is not equal to the client ip
							sendPacket->SetVar("IP", ip);
						else if(!isLocal && serverIp.compare(ip) == 0)		// client is outside but server has the same ip as the client so they must be in the same network
							sendPacket->SetVar("IP", packet->GetVar("R-INT-IP"));
						else if(isLocal && serverIp.compare(fw->portsCfg().emulator_ip) == 0)	// client is local and server has the same public ip as the emulator so they must be in the same network
						{
							isServerLocal = true;
							sendPacket->SetVar("IP", packet->GetVar("R-INT-IP"));
						}
						else	// client is local so check in what network the server is
						{
							isServerLocal = fw->isIpLocal(serverIp);
							if(!isServerLocal)		// server is in another network so send ip from the emulator config
								sendPacket->SetVar("IP", emuIp);
							else					// everyone is local
								sendPacket->SetVar("IP", ip);
						}
					}
					else
					{
						isServerLocal =  true;
						sendPacket->SetVar("IP", ip);
					}
					sendPacket->SetVar("LID", lobbyID);
					sendPacket->SetVar("GID", gameID);
				}
				else	//<-A QENT {R-INT-PORT=10000, R-INT-IP=192.168.178.29, NAME=Brueckel, PID=27, UID=812008934, LID=257, GID=77635}
				{
					sendPacket = new Packet("QENT", 0x00000000);
					sendPacket->SetVar("R-INT-PORT", packet->GetVar("R-INT-PORT"));
					sendPacket->SetVar("R-INT-IP", packet->GetVar("R-INT-IP"));	// internal ip where the CLIENT is hosted
					sendPacket->SetVar("NAME", persona.name);
					sendPacket->SetVar("PID", persona.data[PLAYER_ID]);
					sendPacket->SetVar("UID", persona.id);						// this has to be persona_id
					sendPacket->SetVar("LID", lobbyID);
					sendPacket->SetVar("GID", gameID);
				}
				server_socket->handle_invoke(sendPacket);
				server_socket = NULL;
			}

			string server_pid;
			if(game[PLATFORM].compare("PC") == 0)	// I know this check is anything but consistent but it's better than nothing
				server_pid = "1";
			else if(game[PLATFORM].compare("ps3") == 0)
				server_pid = "2";
			else
				server_pid = "3";

			sendPacket = new Packet("EGEG", 0x00000000);
			sendPacket->SetVar("PL", "pc");
			sendPacket->SetVar("TICKET", ticket);
			sendPacket->SetVar("PID", persona.data[PLAYER_ID]);
			if(!fw->isEmuLocal())	// a public IP is in the config so we have to determine in what network the server is
			{
				string serverIp = game[IP];

				if(!isLocal && serverIp.compare(ip) != 0)			// client is outside and remote server ip is not equal to the client ip
					sendPacket->SetVar("I", serverIp);
				else if(!isLocal && serverIp.compare(ip) == 0)		// client is outside but server has the same ip as the client so they must be in the same network
					sendPacket->SetVar("I", game[INTERNAL_IP]);
				else	// client is local so check in what network the server is
				{
					if(!isServerLocal)		// server is in another network so send public ip
						sendPacket->SetVar("I", serverIp);
					else					// everyone is local
						sendPacket->SetVar("I", game[INTERNAL_IP]);
				}
			}
			else
				sendPacket->SetVar("I", game[IP]);
			sendPacket->SetVar("P", game[PORT]);
			sendPacket->SetVar("HUID", server_pid);	// server persona id
			sendPacket->SetVar("INT-PORT", game[INTERNAL_PORT]);
			sendPacket->SetVar("EKEY", "AIBSgPFqRDg0TfdXW1zUGa4%3d");	// this must be the same key as the one we have on the server? keep it constant in both connections for now (we could integrate it in the database...)
			sendPacket->SetVar("INT-IP", game[INTERNAL_IP]);			// internal ip where the SERVER is hosted
			sendPacket->SetVar("UGID", game[UGID]);
			sendPacket->SetVar("LID", lobbyID);
			sendPacket->SetVar("GID", gameID);
			if(queue_position > 0)
				sendPacket->isDelayed(true, 40);
		}
		else
			debug->warning(2, connectionType, "GID %s was not found in the database", gameID.c_str());
	}

	else if(strcmp(type, "ECNL") == 0)
	{
		string gameID = packet->GetVar("GID");
		string lobbyID = packet->GetVar("LID");
		if(queue_position > 0)
		{
			debug->notification(3, connectionType, "Client is leaving queue, updating queue_length = %s", persona.data[PLAYER_ID].c_str());	// why did I use player id here?
			string* game = database->getGameData(lexical_cast<int>(gameID), false);
			if(!game)
				debug->warning(1, connectionType, "EGAM - The requested GID %i was not found in the database, ignoring packet.", gameID.c_str());
			else
			{
				int queue_length = lexical_cast<int>(game[QUEUE_LENGTH]);
				if(queue_length > 0)
					game[QUEUE_LENGTH] = lexical_cast<string>(queue_length-1);

				// why do we need to send another packet to the server here?
				TcpConnection* server_socket = fw->getServerSocket(lexical_cast<int>(gameID));
				if(server_socket)
				{
					sendPacket = new Packet("QLVT", 0x00000000);
					sendPacket->SetVar("PID", persona.data[PLAYER_ID]);
					sendPacket->SetVar("LID", lobbyID);
					sendPacket->SetVar("GID", gameID);
					server_socket->handle_invoke(sendPacket);
					server_socket = NULL;
				}
			}
			queue_position = 0;
			persona.data[PLAYER_ID] = "0";
		}
		sendPacket = new Packet("ECNL", 0x00000000);	// when is this ECNLmisc?? in case we want to send that, this is the type: 0x6D697363
		sendPacket->SetVar("TID", ++sock_tid);
		sendPacket->SetVar("LID", lobbyID);
		sendPacket->SetVar("GID", gameID);
	}
	else
		state = UNKNOWN;

	if(sendPacket)
		queue->push_back(sendPacket);

	return state;
}

void GameClient::filterGames(Packet* packet, list<int>* filtered)
{
	/*
	FILTER-ATTR-U-gameMod		- GAME_MOD
	FILTER-MIN-SIZE				- ACTIVE_PLAYERS	// just check if filter is present, then filter games with active_players > 0
	FILTER-NOT-FULL				- MAX_PLAYERS		// just check if filter is present, then filter games where active_players < max_players
	FILTER-ATTR-U-EA			- EA
	FILTER-ATTR-U-HasPassword	- HAS_PASSWORD
	FILTER-ATTR-U-Punkbuster	- PUNKBUSTER
	FILTER-ATTR-U-Softcore		- SOFTCORE
	FILTER-ATTR-U-gamemode		- GAMEMODE
	FILTER-ATTR-U-level			- SERVER_LEVEL
	FILTER-ATTR-U-region		- REGION
	FILTER-ATTR-U-public		- IS_PUBLIC
	FAV-GAME					- SERVER_NAME	// multiple choices, seperated with ";"
	FAV-GAME-UID				- UGID			// multiple choices, seperated with ";"
	*/
	list<linked_key> filters;
	linked_key tempFilter;

	string temp = packet->GetVar("FILTER-ATTR-U-gameMod");
	if(!temp.empty())
	{
		tempFilter.key = temp;
		tempFilter.id = GAME_MOD;
		filters.push_back(tempFilter);
	}

	temp = packet->GetVar("FILTER-MIN-SIZE");
	if(!temp.empty())
	{
		tempFilter.key = temp;
		tempFilter.id = ACTIVE_PLAYERS;
		filters.push_back(tempFilter);
	}

	temp = packet->GetVar("FILTER-NOT-FULL");
	if(!temp.empty())
	{
		tempFilter.key = temp;
		tempFilter.id = MAX_PLAYERS;
		filters.push_back(tempFilter);
	}

	temp = packet->GetVar("FILTER-ATTR-U-EA");
	if(!temp.empty())
	{
		tempFilter.key = temp;
		tempFilter.id = EA;
		filters.push_back(tempFilter);
	}

	temp = packet->GetVar("FILTER-ATTR-U-HasPassword");
	if(!temp.empty())
	{
		tempFilter.key = temp;
		tempFilter.id = HAS_PASSWORD;
		filters.push_back(tempFilter);
	}

	//ignore punkbuster filters since it should be always deactivated anyway
	/*temp = packet->GetVar("FILTER-ATTR-U-Punkbuster");
	if(!temp.empty())
	{
		tempFilter.key = temp;
		tempFilter.id = PUNKBUSTER;
		filters.push_back(tempFilter);
	}*/

	temp = packet->GetVar("FILTER-ATTR-U-Softcore");
	if(!temp.empty())
	{
		tempFilter.key = temp;
		tempFilter.id = SOFTCORE;
		filters.push_back(tempFilter);
	}

	temp = packet->GetVar("FILTER-ATTR-U-gamemode");
	if(!temp.empty())
	{
		tempFilter.key = temp;
		tempFilter.id = GAMEMODE;
		filters.push_back(tempFilter);
	}

	temp = packet->GetVar("FILTER-ATTR-U-level");
	if(!temp.empty())
	{
		tempFilter.key = temp;
		tempFilter.id = SERVER_LEVEL;
		filters.push_back(tempFilter);
	}

	temp = packet->GetVar("FILTER-ATTR-U-region");
	if(!temp.empty())
	{
		tempFilter.key = temp;
		tempFilter.id = REGION;
		filters.push_back(tempFilter);
	}

	temp = packet->GetVar("FILTER-ATTR-U-public");
	if(!temp.empty())
	{
		tempFilter.key = temp;
		tempFilter.id = IS_PUBLIC;
		filters.push_back(tempFilter);
	}

	temp = packet->GetVar("FAV-GAME");
	if(!temp.empty())
	{
		size_t start = 0, end = 0;
		if((end = temp.find(';', start)) != string::npos)
		{
			tempFilter.key = temp.substr(start, end-start);
			tempFilter.id = SERVER_NAME;
			filters.push_back(tempFilter);

			start = end+1;
			while((end = temp.find(';', start)) != string::npos && start < temp.size())
			{
				tempFilter.key = temp.substr(start, end-start);
				tempFilter.id = SERVER_NAME;
				filters.push_back(tempFilter);
				start = end+1;
			}
		}
		else
		{
			tempFilter.key = temp;
			tempFilter.id = SERVER_NAME;
			filters.push_back(tempFilter);
		}
	}

	temp = packet->GetVar("FAV-GAME-UID");
	if(!temp.empty())
	{
		size_t start = 0, end = 0;
		if((end = temp.find(';', start)) != string::npos)
		{
			tempFilter.key = temp.substr(start, end-start);
			tempFilter.id = UGID;
			filters.push_back(tempFilter);

			start = end+1;
			while((end = temp.find(';', start)) != string::npos && start < temp.size())
			{
				tempFilter.key = temp.substr(start, end-start);
				tempFilter.id = UGID;
				filters.push_back(tempFilter);
				start = end+1;
			}
		}
		else
		{
			tempFilter.key = temp;
			tempFilter.id = UGID;
			filters.push_back(tempFilter);
		}
	}
	database->listMatchingGames(&filters, filtered);
}
