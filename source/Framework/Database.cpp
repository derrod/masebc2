#include "Database.h"
#include <boost/filesystem/operations.hpp>

extern Logger* debug;

Database::Database(bool extended_info)
{
	lobby_num_games = 0;
	lobby = new string[LOBBY_SIZE];
	lobby[LOBBY_ID] = "1";
	lobby[LOBBY_NAME] = "bfbc2_01";
	lobby[LOBBY_LOCALE] = "en_US";
	lobby[LOBBY_MAX_GAMES] = "1000";

	this->extended_info = extended_info;
	initializeData();
}

Database::~Database()
{
	delete[] lobby;

	for(vector<GamesEntry>::iterator it = games.begin(); it != games.end(); it++)
	{
		if(it->key)
			delete[] it->key;
		if(it->gdet)
			delete[] it->gdet;
	}
	games.clear();
	personas.clear();
	users.clear();
}

void Database::initializeData()
{
	// these entries are always there (game servers login with them)
	list_entry server_persona = {1, "bfbc2.server.p", new string[PERSONA_SIZE]};
	server_persona.data[USER_ID] = "1";
	server_persona.data[USER_EMAIL] = "bfbc2.server.pc@ea.com";
	server_persona.data[PLAYER_ID] = "0";
	server_persona.data[PERSONA_ONLINE] = "1";
	personas.insert(server_persona);

	server_persona.id = 2;
	server_persona.name = "bfbc.server.ps";
	server_persona.data = new string[PERSONA_SIZE];
	server_persona.data[USER_ID] = "2";
	server_persona.data[USER_EMAIL] = "bfbc.server.ps3@ea.com";
	server_persona.data[PLAYER_ID] = "0";
	server_persona.data[PERSONA_ONLINE] = "1";
	personas.insert(server_persona);

	server_persona.id = 3;
	server_persona.name = "bfbc.server.xe";
	server_persona.data = new string[PERSONA_SIZE];
	server_persona.data[USER_ID] = "3";
	server_persona.data[USER_EMAIL] = "bfbc.server.xenon@ea.com";
	server_persona.data[PLAYER_ID] = "0";
	server_persona.data[PERSONA_ONLINE] = "1";
	personas.insert(server_persona);


	list_entry server_user = {1, "bfbc2.server.pc@ea.com", new string[USER_SIZE]};
	server_user.data[PASSWORD] = "Che6rEPA";
	server_user.data[COUNTRY] = "";
	server_user.data[BIRTHDAY] = "";
	server_user.data[USER_ONLINE] = "1";
	users.insert(server_user);

	server_user.id = 2;
	server_user.name = "bfbc.server.ps3@ea.com";
	server_user.data = new string[USER_SIZE];
	server_user.data[PASSWORD] = "zAmeH7bR";
	server_user.data[COUNTRY] = "";
	server_user.data[BIRTHDAY] = "";
	server_user.data[USER_ONLINE] = "1";
	users.insert(server_user);

	server_user.id = 3;
	server_user.name = "bfbc.server.xenon@ea.com";
	server_user.data = new string[USER_SIZE];
	server_user.data[PASSWORD] = "B8ApRavE";
	server_user.data[COUNTRY] = "";
	server_user.data[BIRTHDAY] = "";
	server_user.data[USER_ONLINE] = "1";
	users.insert(server_user);

	string directory = "./database/users.lst";
	if(!loadFileData(directory.c_str(), USERS))
		debug->notification(3, DATABASE, "Could not load users table from \"%s\", using an empty table for both users and personas instead.", directory.c_str());
	else
	{
		directory = "./database/personas.lst";
		if(!loadFileData(directory.c_str(), PERSONAS))
			debug->notification(3, DATABASE, "Could not load personas table from \"%s\", using an empty table instead.", directory.c_str());
	}
	debug->notification(1, DATABASE, "Finished initializing database");
}

void Database::saveDatabase()
{
	stringstream buffer;
	bool idsAvailable = false;
	while(!availableUserSlots.empty())
	{
		buffer << availableUserSlots.front() << "|";
		availableUserSlots.pop_front();
		if(!idsAvailable)
			idsAvailable = true;
	}
	if(idsAvailable)
		buffer << "\n\n";

	list_id& uli = users.get<int>();
	list_id::iterator it;
	while((it = uli.begin()) != uli.end())
	{
		if(it->id != 1 && it->id != 2 && it->id != 3)	// don't save the server stuff in the external file
		{
			buffer << it->id << "\f"
				<< it->name << "\f"
				<< it->data[PASSWORD] << "\f"
				<< it->data[COUNTRY] << "\f"
				<< it->data[BIRTHDAY] << "\f"
				<< "0" << "\n";		// we save this when program exits and at that point everybody is set offline
		}
		delete[] it->data;
		uli.erase(it);
	}

	saveFileData("./database/users.lst", buffer.str());
	buffer.str("");

	idsAvailable = false;
	while(!availablePersonaSlots.empty())
	{
		buffer << availablePersonaSlots.front() << "|";
		availablePersonaSlots.pop_front();
		if(!idsAvailable)
			idsAvailable = true;
	}
	if(idsAvailable)
		buffer << "\n\n";

	list_id& pli = personas.get<int>();
	while((it = pli.begin()) != pli.end())
	{
		if(it->id != 1 && it->id != 2 && it->id != 3)	// don't save the server stuff in the external file
		{
			buffer << it->id << "\f"
				<< it->name << "\f"
				<< it->data[USER_ID] << "\f"
				<< it->data[USER_EMAIL] << "\f"
				<< it->data[PLAYER_ID] << "\f"
				<< "0" << "\n";		// we save this when program exits and at that point everybody is set offline
		}
		delete[] it->data;
		pli.erase(it);
	}

	saveFileData("./database/personas.lst", buffer.str());
}

bool Database::loadFileData(const char* file, int struct_type)
{
	bool loadSuccess = true;
	string tempPath = file;
	tempPath.append(".tmp");

	// make a temporary backup of the .lst file (in case something goes wrong, or is this overcautious?)
	filesystem::path source_file(file), dest_file(tempPath);
	if(!filesystem::exists(source_file))
	{
		debug->warning(1, DATABASE, "There is a problem opening the file %s", file);
		return false;
	}
	filesystem::copy_file(source_file, dest_file);

	// open the actual .lst file
	ifstream in(file, ifstream::in);
	if(!in.is_open() || !in.good())
		return false;

	stringstream buffer;
	buffer << in.rdbuf();
	string list = buffer.str();
	in.close();

	if(struct_type == USERS)
		usersData = buffer.str();
	else
		personasData = buffer.str();

	// first line contains available id's in the list, if there is no first line the next available id is list.size()+1
	size_t start = 0, current = 0, end = list.find("\n\n", start);
	if(end != string::npos)
	{
		string header = list.substr(start, end-start);
		current = header.find('|', start);

		while(current < header.size() && current != string::npos)
		{
			int id = lexical_cast<int>(header.substr(start, current-start));

			if(struct_type == USERS)
				availableUserSlots.push_back(id);
			else
				availablePersonaSlots.push_back(id);

			start = current+1;
			current = header.find('|', start);
		}
		start = current = end+2;
	}

	end = list.find('\n', start);

	int count;
	if(struct_type)
		count = PERSONA_SIZE;
	else
		count = USER_SIZE;

	while(end != string::npos)
	{
		string line = list.substr(current, end-current);
		start = 0;
		list_entry row;

		end = line.find('\f', start);
		if(end == string::npos)
			end = line.size();

		row.id = lexical_cast<int>(line.substr(start, end-start));
		if(struct_type == USERS && row.id > highestUserId)
			highestUserId = row.id;
		else if(struct_type == PERSONAS && row.id > highestPersonaId)
			highestPersonaId = row.id;

		start = end+1;
		if(start < line.size())
		{
			end = line.find('\f', start);
			if(end == string::npos)
				end = line.size();
			row.name = line.substr(start, end-start);
			start = end+1;

			if(struct_type == USERS)
				row.data = new string[USER_SIZE];
			else
				row.data = new string[PERSONA_SIZE];

			for(int i = 0; i < count; i++)
			{
				if(start < line.size())
				{
					end = line.find('\f', start);
					if(end == string::npos)
						end = line.size();

					string value = line.substr(start, end-start);
					row.data[i] = value;

					start = end+1;
				}
				else
				{
					debug->warning(2, DATABASE, "Line in %s is missing at least one column, %i (tmp file will not be deleted as a backup)", file, i);
					loadSuccess = false;
					break;
				}
			}

			if(struct_type == USERS)
				users.insert(row);
			else
				personas.insert(row);
		}
		else
		{
			debug->warning(2, DATABASE, "%s seems to be corrupted, check the file (tmp will not be deleted as a backup)", file);
			loadSuccess = false;
			break;
		}
		current += start;
		end = list.find('\n', current);
	}
	if(loadSuccess)
		filesystem::remove(dest_file);	// everything went fine, remove the temporary backup again

	// recheck if there is no id higher than highestId in the availableId list, if so fix the list
	buffer.str("");
	bool rearrange = false;
	std::list<int>::iterator it;
	std::list<int> tempList;
	if(struct_type == PERSONAS && !availablePersonaSlots.empty())
	{
		it = availablePersonaSlots.begin();
		while(it != availablePersonaSlots.end())
		{
			// search for highest id in available id list
			if((*it) >= highestPersonaId)
			{
				rearrange = true;
				it = availablePersonaSlots.erase(it);
			}
			else
			{
				tempList.push_back((*it));
				it++;
			}
		}

		if(rearrange)
		{
			while(!tempList.empty())
			{
				buffer << tempList.front() << "|";
				tempList.pop_front();
			}
			size_t pos = personasData.find("\n\n");	// this should always be there if the id list is not empty
			if(pos != string::npos)
				personasData.replace(0, pos, buffer.str());
			else
				debug->warning(1, DATABASE, "%s seems to contain bad data!", file);
		}
	}
	else if(struct_type == USERS && !availableUserSlots.empty())
	{
		it = availableUserSlots.begin();
		while(it != availableUserSlots.end())
		{
			// search for highest id in available id list
			if((*it) >= highestUserId)
			{
				rearrange = true;
				it = availableUserSlots.erase(it);
			}
			else
			{
				tempList.push_back((*it));
				it++;
			}
		}

		if(rearrange)
		{
			while(!tempList.empty())
			{
				buffer << tempList.front() << "|";
				tempList.pop_front();
			}
			size_t pos = usersData.find("\n\n");	// this should always be there if the id list is not empty
			if(pos != string::npos)
				usersData.replace(0, pos, buffer.str());
			else
				debug->warning(1, DATABASE, "%s seems to contain bad data!", file);
		}
	}

	return true;
}

bool Database::saveFileData(string file, string data)
{
	bool done = false;
	string tempPath = file;
	tempPath.append(".tmp");

	// make a temporary backup of the .lst file (in case something goes wrong, or is this overcautious?)
	filesystem::path source_file(file), dest_file(tempPath);
	if(!filesystem::exists(source_file))
	{
		debug->warning(1, DATABASE, "There is a problem opening the file %s", file.c_str());
		return false;
	}
	filesystem::copy_file(source_file, dest_file);

	ofstream destFile(file, ofstream::out | ofstream::trunc);	// we delete all content, is it safe to do this?
	if(destFile.is_open() && destFile.good())
	{
		destFile.write(data.c_str(), data.size());
		if(!destFile.bad())
			done = true;
		destFile.close();
	}
	if(!done)
		debug->warning(1, DATABASE, "A problem occured when saving %s with the data:\n==begin==\n%s\n==end==\n", file.c_str(), data.c_str());
	else
		filesystem::remove(dest_file);	// everything went fine, remove the temporary backup again

	return done;
}

int Database::addUser(list_entry user)
{
	bool idFlag = false;
	if(availableUserSlots.empty())
		user.id = users.size()+1;	// only the size would be the last id in the list
	else
	{
		idFlag = true;
		user.id = availableUserSlots.front();
		availableUserSlots.pop_front();
	}

	pair<table::iterator, bool> success = users.insert(user);
	if(!success.second)
	{
		debug->notification(3, DATABASE, "User ID %i with name %s could not be added to database (name already exists in database?)", user.id, user.name.c_str());
		availableUserSlots.push_back(user.id);
		return -1;
	}
	else
	{
		if(idFlag)	// an id has been removed from the available id's list so remove it in file too
		{
			// fileUsers, usersData
			size_t pos = 0;
			string searchString = lexical_cast<string>(user.id);
			searchString.append("|");
			if((pos = usersData.find(searchString)) > 0 && usersData[pos-1] != '|')	// is the result the correct one or should we search again?
			{
				searchString.insert(searchString.begin(), '|');
				pos = usersData.find(searchString, pos-1);
				idFlag = false;
			}

			if(pos != string::npos)		// no need to delete anything if there is nothing (though that shouldn't be)
			{
				size_t end = pos+searchString.length();
				if(!idFlag)
					pos++;	// +1 because in this case pos points to a | and not the first number of the id

				if(pos < end && end < usersData.size())
					usersData.erase(pos, end);
			}
		}

		// id updating done, now add (append?) the actual entry
		usersData.append(lexical_cast<string>(user.id));
		usersData.append("\f");
		usersData.append(user.name);
		usersData.append("\f");
		usersData.append(user.data[PASSWORD]);
		usersData.append("\f");
		usersData.append(user.data[COUNTRY]);
		usersData.append("\f");
		usersData.append(user.data[BIRTHDAY]);
		usersData.append("\f0\n");		// this value is resetted when reconnecting anyway, no need to save the real value
		ofstream fileUsers("./database/users.lst", ofstream::out | ofstream::trunc);
		fileUsers.write(usersData.c_str(), usersData.size());
		fileUsers.close();

		// id updating done, now add (append?) the actual entry
		return user.id;
	}
}

bool Database::getUser(int id, list_entry* user)
{
	const list_id &li = users.get<int>();
	list_id::iterator it = li.find(id);
	if(it != li.end())
	{
		user->id = it->id;
		user->name = it->name;
		user->data = it->data;
		return true;
	}
	else
	{
		user->id = -1;
		user->name = "";
		user->data = NULL;
		debug->notification(3, DATABASE, "User ID %i was not found in the database", id);
		return false;
	}
}

bool Database::getUser(string name, list_entry* user)
{
	const list_string &ls = users.get<string>();
	list_string::iterator it = ls.find(name);
	if(it != ls.end())
	{
		user->id = it->id;
		user->name = it->name;
		user->data = it->data;
		return true;
	}
	else
	{
		user->id = -1;
		user->name = "";
		user->data = NULL;
		debug->notification(3, DATABASE, "User name %s was not found in the database", name.c_str());
		return false;
	}
}

int Database::addPersona(list_entry persona)
{
	bool idFlag = false;
	if(availablePersonaSlots.empty())
		persona.id = personas.size()+1;	//only the size would be the last id in the list
	else
	{
		idFlag = true;
		persona.id = availablePersonaSlots.front();
		availablePersonaSlots.pop_front();
	}

	pair<table::iterator, bool> success = personas.insert(persona);
	if(!success.second)
	{
		debug->notification(3, DATABASE, "Persona ID %i with name %s could not be added to database (name already exists in database?)", persona.id, persona.name.c_str());
		availablePersonaSlots.push_back(persona.id);	// insert failed so put id into list again
		return -1;
	}
	else
	{
		if(idFlag)	// an id has been removed from the available id's list so remove it in file too
		{
			// filePersonas, personasData
			size_t pos = 0;
			string searchString = lexical_cast<string>(persona.id);
			searchString.append("|");
			if((pos = personasData.find(searchString)) > 0 && personasData[pos-1] != '|')	// is the result the correct one or should we search again?
			{
				searchString.insert(searchString.begin(), '|');
				pos = personasData.find(searchString, pos-1);
				idFlag = false;
			}

			if(pos != string::npos)		// no need to delete anything if there is nothing (though that shouldn't be)
			{
				size_t end = pos+searchString.length();
				if(!idFlag)
					pos++;	// +1 because in this case pos points to a | and not the first number of the id

				if(pos < end && end < personasData.size())
					personasData.erase(pos, end-pos);
			}
		}

		if(persona.id > highestPersonaId)
			highestPersonaId = persona.id;

		// id updating done, now add (append?) the actual entry
		personasData.append(lexical_cast<string>(persona.id));
		personasData.append("\f");
		personasData.append(persona.name);
		personasData.append("\f");
		personasData.append(persona.data[USER_ID]);
		personasData.append("\f");
		personasData.append(persona.data[USER_EMAIL]);
		personasData.append("\f0\f0\n");	// these 2 values are resetted when reconnecting anyway, no need to save the real values
		ofstream filePersonas("./database/personas.lst", ofstream::out | ofstream::trunc);
		filePersonas.write(personasData.c_str(), personasData.size());
		filePersonas.close();

		return persona.id;
	}
}

void Database::removePersona(string name)
{
	list_string& ls = personas.get<string>();
	list_string::iterator it = ls.find(name);
	if(it != ls.end())
	{
		int id = it->id;
		delete[] it->data;
		ls.erase(it);

		size_t pos = 0;
		string searchString = lexical_cast<string>(id);
		if(id < highestPersonaId)					// <= because we already erased the entry here
		{
			availablePersonaSlots.push_back(id);	// save id (only if it's not the last one) so we know which ones are usable
			// save id also in file
			if((pos = personasData.find("\n\n")) != string::npos)	// there must be only one occurence of this
			{
				searchString.append("|");
				personasData.insert(pos, searchString);
				searchString.pop_back();
			}
			else
				debug->notification(4, DATABASE, "could not find \\n\\n in the personas.lst");
		}
		else if(id == highestPersonaId)
		{
			bool countDown = true;
			for(list<int>::iterator iter = availablePersonaSlots.begin(); iter != availablePersonaSlots.end(); iter++)
			{
				if((*iter) == highestPersonaId-1)
				{
					countDown = false;
					break;
				}
			}
			if(countDown)		// it's safe to set highest id -1 (however I'm not sure if it's save to do nothing when countDown == false...)
				highestPersonaId--;
		}
		else
			debug->warning(3, DATABASE, "id %i is last one in the list, highestId = %i", id, highestPersonaId);

		// now remove entry from file
		searchString.append("\f");
		searchString.append(name);

		if((pos = personasData.find(searchString)) > 0 && personasData[pos-1] != '\n')
		{
			searchString.insert(searchString.begin(), '\n');
			pos = personasData.find(searchString, pos-1);
			if(pos != string::npos)
				pos++;	// better to do this here
		}

		if(pos != string::npos)
		{
			size_t end = personasData.find('\n', pos)+1;	// we want the \n also removed
			if(end != string::npos)
				personasData.erase(pos, end-pos);
			else
				debug->warning(3, DATABASE, "removePersona() something is wrong with Persona %s in file data!", name.c_str());
		}
		ofstream filePersonas("./database/personas.lst", ofstream::out | ofstream::trunc);
		filePersonas.write(personasData.c_str(), personasData.size());
		filePersonas.close();
		debug->notification(3, DATABASE, "Persona %s with ID %i was removed from database", name.c_str(), id);
	}
	else
		debug->warning(3, DATABASE, "removePersona() Persona %s was not found in database!", name.c_str());
}

bool Database::getPersona(int id, list_entry* persona)
{
	const list_id &li = personas.get<int>();
	list_id::iterator it = li.find(id);
	if(it != li.end())
	{
		persona->id = it->id;
		persona->name = it->name;
		persona->data = it->data;
		return true;
	}
	else
	{
		persona->id = -1;
		persona->name = "";
		persona->data = NULL;
		debug->notification(3, DATABASE, "Persona ID %i was not found in the database", id);
		return false;
	}
}

bool Database::getPersona(string name, list_entry* persona)
{
	const list_string &ls = personas.get<string>();
	list_string::iterator it = ls.find(name);
	if(it != ls.end())
	{
		persona->id = it->id;
		persona->name = it->name;
		persona->data = it->data;
		return true;
	}
	else
	{
		persona->id = -1;
		persona->name = "";
		persona->data = NULL;
		debug->notification(3, DATABASE, "Persona name %s was not found in the database", name.c_str());
		return false;
	}
}

/*
bool Database::setUserData(int id, int index, string data)
{
	const list_id &li = users.get<int>();
	list_id::iterator it = li.find(id);
	if(it != li.end())
	{
		it->data[index] = data;
		return true;
	}
	else
	{
		debug->warning(3, DATABASE, "User ID %i was not found in the database", id);
		return false;
	}
}

bool Database::setUserData(string name, int index, string data)
{
	const list_string &ls = users.get<string>();
	list_string::iterator it = ls.find(name);
	if(it != ls.end())
	{
		it->data[index] = data;
		return true;
	}
	else
	{
		debug->warning(3, DATABASE, "User name %s was not found in the database", name.c_str());
		return false;
	}
}

bool Database::setPersonaData(int id, int index, string data)
{
	const list_id &li = personas.get<int>();
	list_id::iterator it = li.find(id);
	if(it != li.end())
	{
		it->data[index] = data;
		return true;
	}
	else
	{
		debug->warning(3, DATABASE, "Persona ID %i was not found in the database", id);
		return false;
	}
}

bool Database::setPersonaData(string name, int index, string data)
{
	const list_string &ls = personas.get<string>();
	list_string::iterator it = ls.find(name);
	if(it != ls.end())
	{
		it->data[index] = data;
		return true;
	}
	else
	{
		debug->warning(3, DATABASE, "Persona name %s was not found in the database", name.c_str());
		return false;
	}
}

bool Database::setGameData(int id, int index, string data, bool gdet)
{
	if(id <= 0 || id > (int)games.size())
	{
		debug->warning(3, DATABASE, "setGameData() - Invalid server ID was passed as parameter - %i", id);
		return false;
	}

	id--;	//vector list is null based but our ids are not
	if(!gdet)
		games[id].key[index] = data;
	else
		games[id].gdet[index] = data;

	return true;
}
*/

void Database::listMatchingPersonas(int index, string searchTerm, list<list_entry>* filtered)
{
	const list_string &ls = personas.get<string>();
	list_string::iterator it;
	for(it = ls.begin(); it != ls.end(); it++)
	{
		//no need to go through gdet struct because we never need to?
		if(it->data[index].compare(searchTerm) == 0)
			filtered->push_back((*it));
	}
	debug->notification(3, DATABASE, "Filtering out personas for %s = %s, %i results", personas_text[index].c_str(), searchTerm.c_str(), filtered->size());
	//since we used a pointer we dont need to return anything
}

int Database::addGame(GamesEntry game)
{
	lobby_num_games++;		//we add a game so we have a new active entry

	if(availableGameSlots.empty())
	{
		games.push_back(game);
		debug->notification(3, DATABASE, "Server was added to database, assigned ID: %i", games.size());
		return games.size();
	}
	else
	{
		int available_id = availableGameSlots.front();
		games[available_id].key = game.key;
		games[available_id].gdet = game.gdet;

		availableGameSlots.pop_front();
		available_id++;			//return plus one because game doesn't like it zero based
		debug->notification(3, DATABASE, "Server was added to database, assigned ID: %i", available_id);
		return available_id;
	}
}

void Database::removeGame(int id)
{
	if(isValidGid(id))
	{
		id--;
		if(games[id].key)
		{
			delete[] games[id].key;
			games[id].key = NULL;
		}
		if(games[id].gdet)
		{
			delete[] games[id].gdet;
			games[id].gdet = NULL;
		}
		availableGameSlots.push_back(id);
		debug->notification(3, DATABASE, "Server with ID %i was removed from database", id+1);
		lobby_num_games--;
	}
	else
		debug->warning(3, DATABASE, "removeGame() - Invalid server ID was passed as parameter - %i", id);
}

bool Database::isValidGid(int id)
{
	if(id > 0 && id <= (int)games.size())
		return true;
	else
		return false;
}

string* Database::getGameData(int id, bool gdet)
{
	if(isValidGid(id))
	{
		id--;	//vector list is null based but our ids are not
		if(!gdet)
			return games[id].key;
		else
			return games[id].gdet;
	}
	else
	{
		debug->warning(3, DATABASE, "getGameData() - Invalid server ID was passed as parameter - %i", id);
		return NULL;
	}
}

void Database::listMatchingGames(list<linked_key>* filters, list<int>* filtered, bool returnFirst)
{
	for(int i = 0; i < (int)games.size(); i++)
	{
		if(games[i].key)
		{
			list<linked_key>::iterator it;
			bool match = true;
			bool name_match = false;
			bool ugid_match = false;

			bool check_name = false;
			bool check_ugid = false;
			for(it = filters->begin(); it != filters->end(); it++)
			{
				switch(it->id)
				{
					case ACTIVE_PLAYERS:
					{
						if(it->key.compare("1") == 0)
						{
							int active_players = lexical_cast<int>(games[i].key[ACTIVE_PLAYERS]);
							if(active_players < 1)
								match = false;
						}
						break;
					}
					case MAX_PLAYERS:
					{
						if(it->key.compare("1") == 0)
						{
							int active_players = lexical_cast<int>(games[i].key[ACTIVE_PLAYERS]);
							int max_players = lexical_cast<int>(games[i].key[MAX_PLAYERS]);
							if(active_players >= max_players)
								match = false;
						}
						break;
					}
					case SERVER_NAME:
					{
						if(!check_name)
							check_name = true;

						if(games[i].key[SERVER_NAME].find(it->key) != string::npos)
							name_match = true;
						break;
					}
					case UGID:
					{
						if(!check_ugid)
							check_ugid = true;

						if(games[i].key[UGID].compare(it->key) == 0)// || it->key.compare("frostbite") == 0)
							ugid_match = true;
						break;
					}
					default:
					{
						if(it->key.compare(games[i].key[it->id]) != 0)
							match = false;
						break;
					}
				}

				if(!match)// && it->id != SERVER_NAME && it->id != UGID)
					break;
			}

			if(match)
			{
				// hopefully all possible cases are covered here
				if(!check_name && !check_ugid)
				{
					filtered->push_back(i+1);
					if(returnFirst)
						break;
				}
				else if(!check_ugid && check_name && name_match)
				{
					filtered->push_back(i+1);
					if(returnFirst)
						break;
				}
				else if(!check_name && check_ugid && ugid_match)
				{
					filtered->push_back(i+1);
					if(returnFirst)
						break;
				}
				else if(check_name && check_ugid && name_match && ugid_match)
				{
					filtered->push_back(i+1);
					if(returnFirst)
						break;
				}
			}
		}
	}
	debug->notification(3, DATABASE, "Found %i games of %i for the set filters", filtered->size(), games.size());
}

void Database::listAllGames(list<int>* allGames)
{
	for(int i = 0; i < (int)games.size(); i++)
	{
		if(games[i].key)
			allGames->push_back(i+1);	// we are one-based
	}
	debug->notification(3, DATABASE, "Found %i games (no filters)", allGames->size());
}

void Database::personaLogin(linked_key persona)
{
	lkeys_assigned.push_back(persona);
}

int Database::theaterLogin(string lkey)
{
	list<linked_key>::iterator it;
	for(it = lkeys_assigned.begin(); it != lkeys_assigned.end(); it++)
	{
		if(lkey.compare(it->key) == 0)
		{
			int id = it->id;
			lkeys_assigned.erase(it);
			return id;
		}
	}
	debug->warning(2, DATABASE, "theaterLogin() - no ID found for lkey %s", lkey.c_str());
	return -1;
}

string Database::getLobbyInfo(int index)
{
	return lobby[index];
}

int Database::getLobbyGames()
{
	return lobby_num_games;
}

string Database::listEntryToString(list_entry* entry, const char* state, bool user)
{
	string line = (user) ? "users" : "personas";
	line.append(" table ");
	line.append(state);
	line.append(" id: ");
	line.append(lexical_cast<string>(entry->id));
	line.append(", name: ");
	line.append(entry->name);
	line.append("\n");

	if(extended_info)
	{
		string border = "";
		string text = "";

		if(user)
		{
			for(int i = 0; i < USER_SIZE; i++)
			{
				int cell_length = entry->data[i].size();
				if(cell_length == 0)
					cell_length = 4;	//key is empty so we put NULL into cell

				if(cell_length < (int)users_text[i].size())
				{
					text.append(users_text[i].size() - cell_length, ' ');
					cell_length = users_text[i].size();
				}
				else
					line.append(cell_length - users_text[i].size(), ' ');

				line.append(users_text[i]);
				border.append(cell_length, '-');

				if(!entry->data[i].empty())
					text.append(entry->data[i]);
				else
					text.append("NULL");

				if(i+1 < USER_SIZE)
				{
					line.append(" | ");
					text.append(" | ");
					border.append("-|-");
				}
				else
				{
					line.append("\n");
					border.append("\n");
					text.append("\n");
				}
			}
		}
		else
		{
			for(int i = 0; i < PERSONA_SIZE; i++)
			{
				int cell_length = entry->data[i].size();
				if(cell_length == 0)
					cell_length = 4;	//key is empty so we put NULL into cell

				if(cell_length < (int)personas_text[i].size())
				{
					text.append(personas_text[i].size() - cell_length, ' ');
					cell_length = personas_text[i].size();
				}
				else
					line.append(cell_length - personas_text[i].size(), ' ');

				line.append(personas_text[i]);
				border.append(cell_length, '-');

				if(!entry->data[i].empty())
					text.append(entry->data[i]);
				else
					text.append("NULL");

				if(i+1 < PERSONA_SIZE)
				{
					line.append(" | ");
					text.append(" | ");
					border.append("-|-");
				}
				else
				{
					line.append("\n");
					border.append("\n");
					text.append("\n");
				}
			}
		}
		line.append(border);
		line.append(text);
	}
	else
	{
		int count;
		if(user)
			count = USER_SIZE;
		else
			count = PERSONA_SIZE;

		for(int i = 0; i < count; i++)
		{
			if(!entry->data[i].empty())
				line.append(entry->data[i]);
			else
				line.append("NULL");

			if(i+1 < count)
				line.append(", ");
			else
				line.append("\n");
		}
	}
	return line;
}

string Database::gameToString(int id, const char* state, GamesEntry* game, bool gdet)
{
	string line = (gdet) ? "gdet" : "games";
	line.append(" table ");
	line.append(state);
	line.append(" gid: ");
	line.append(lexical_cast<string>(id));
	line.append("\n");
	//char buffer[36];
	//sprintf(buffer, "%s table %s gid: %i\n", (gdet) ? "gdet" : "games", state, id);
	//string line = buffer;

	if(extended_info)
	{
		string border = "";
		string text = "";

		if(!gdet)
		{
			for(int i = 0; i < GAMES_SIZE; i++)
			{
				int cell_length = game->key[i].size();
				if(cell_length == 0)
					cell_length = 4;	//key is empty so we put NULL into cell

				if(cell_length < (int)games_text[i].size())
				{
					text.append(games_text[i].size() - cell_length, ' ');
					cell_length = games_text[i].size();
				}
				else
					line.append(cell_length - games_text[i].size(), ' ');

				line.append(games_text[i]);
				border.append(cell_length, '-');

				if(!game->key[i].empty())
					text.append(game->key[i]);
				else
					text.append("NULL");

				if(i+1 < GAMES_SIZE)
				{
					line.append(" | ");
					text.append(" | ");
					border.append("-|-");
				}
				else
				{
					line.append("\n");
					border.append("\n");
					text.append("\n");
				}
			}
		}
		else
		{
			for(int i = 0; i < GDET_SIZE; i++)
			{
				int cell_length = game->gdet[i].size();
				if(cell_length == 0)
					cell_length = 4;	//key is empty so we put NULL into cell

				if(cell_length < (int)gdet_text[i].size())
				{
					text.append(gdet_text[i].size() - cell_length, ' ');
					cell_length = gdet_text[i].size();
				}
				else
					line.append(cell_length - gdet_text[i].size(), ' ');

				line.append(gdet_text[i]);
				border.append(cell_length, '-');

				if(!game->gdet[i].empty())
					text.append(game->gdet[i]);
				else
					text.append("NULL");

				if(i+1 < GDET_SIZE)
				{
					line.append(" | ");
					text.append(" | ");
					border.append("-|-");
				}
				else
				{
					line.append("\n");
					border.append("\n");
					text.append("\n");
				}
			}
		}
		line.append(border);
		line.append(text);
	}
	else
	{
		if(!gdet)
		{
			for(int i = 0; i < GAMES_SIZE; i++)
			{
				if(!game->key[i].empty())
					line.append(game->key[i]);
				else
					line.append("NULL");

				if(i+1 < GAMES_SIZE)
					line.append(", ");
				else
					line.append("\n");
			}
		}
		else
		{
			for(int i = 0; i < GDET_SIZE; i++)
			{
				if(!game->gdet[i].empty())
					line.append(game->gdet[i]);
				else
					line.append("NULL");

				if(i+1 < GDET_SIZE)
					line.append(", ");
				else
					line.append("\n");
			}
		}
	}
	return line;
}
