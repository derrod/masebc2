#include "Stats.h"
#include "Logger.h"
#include "Framework.h"
#include <fstream>
#include <boost/filesystem/operations.hpp>

extern Logger* debug;
extern Framework* fw;
extern bool useTemplate;

Stats::Stats()
{
}

Stats::Stats(string email, string persona, bool createNew, bool initOnly)
{
	this->email = email;
	this->persona = persona;

	string statsName = "./templates/stats";
	if(fw->emuCfg().all_stats_unlocked)
		statsName.append("_unlocked");

	path = "./database/";
	path.append(email);
	path.push_back('/');
	path.append(persona);
	dogtagPath = path;
	path.append(".cfg");
	dogtagPath.append(".dog");

	ifstream statsFile(path, ifstream::in);
	if(statsFile.is_open() && statsFile.good())
	{
		if(!initOnly)
		{
			stringstream buffer;
			buffer << statsFile.rdbuf();
			data = buffer.str();
			statsFile.close();
			if(data.empty())	//if the stats file is empty or cannot be opened we have to get default stats somewhere
			{
				debug->warning(2, DEBUG, "Persona Stats file \"%s\" data corrupted, loading default stats from template...", path.c_str());
				buffer.clear();
				ifstream defaultFile(statsName, ifstream::in);
				if(defaultFile.is_open() && defaultFile.good())
				{
					buffer << defaultFile.rdbuf();
					data = buffer.str();
					defaultFile.close();
				}
				else
					debug->warning(1, DEBUG, "Could not get default stats, logout asap with the client and check all the files locations!");
			}
		}
		else
			data = "";
	}
	else
	{
		if(!createNew)
		{
			debug->warning(1, DEBUG, "Could not find persona file '%s', creating new one...", path.c_str());
			filesystem::path source_path(statsName), dest_path(path);
			if(!filesystem::exists(source_path))//copyFile("./templates/stats", path.c_str()))
				debug->warning(1, DEBUG, "Could not get default stats, logout asap with the client and check all the files locations!");
			else
				filesystem::copy_file(source_path, dest_path);

			if(!filesystem::exists(dest_path))
				debug->warning(1, DEBUG, "Failed to copy the default stats to %s, check you access permissions!", path.c_str());
			else
			{
				ifstream statsFile(path, ifstream::in);
				if(statsFile.is_open() && statsFile.good())
				{
					if(!initOnly)
					{
						stringstream buffer;
						buffer << statsFile.rdbuf();
						data = buffer.str();
						statsFile.close();
						if(data.empty())	//if the stats file is empty or cannot be opened we have to get default stats somewhere
							debug->warning(2, DEBUG, "Persona Stats file \"%s\" data corrupted, check your template file!!", path.c_str());
					}
					else
						data = "";
				}
			}
		}
		else
		{
			bool error = false;
			//if we create a new file then we don't want to load stats
			filesystem::path source_path(statsName), dest_path(path);
			if(!filesystem::exists(source_path))
				debug->warning(1, DEBUG, "Could not get default stats, logout asap with the client and check all the files locations!");
			else
				filesystem::copy_file(source_path, dest_path);

			if(!filesystem::exists(dest_path))
				debug->warning(1, DEBUG, "Failed to copy the default stats to %s, check you access permissions!", path.c_str());
			else
			{
				if(!error)
					debug->notification(2, DEBUG, "Created new persona file '%s'", path.c_str());

				ofstream dogFile(dogtagPath, ios::out | ios::trunc);
				if(!dogFile.is_open())
				{
					debug->warning(2, DEBUG, "Couldn't create dogtag file for '%s'", persona.c_str());
					error = true;
				}
				else
					dogFile.close();

				if(!error)
					debug->notification(2, DEBUG, "Created new dogtag file '%s'", dogtagPath.c_str());
			}
		}
	}
}

void Stats::saveStats()
{
	string tempFile = path;
	tempFile.append(".bak");
	filesystem::path source_path(path), dest_path(tempFile);
	filesystem::copy_file(source_path, dest_path);
	if(!filesystem::exists(dest_path))
		return;

	bool done = false;
	ofstream statsFile(path, ofstream::out | ofstream::trunc);	//we delete all content, is it safe to do this?
	if(statsFile.is_open() && statsFile.good())
	{
		statsFile.write(data.c_str(), data.size());
		if(!statsFile.bad())
			done = true;
		statsFile.close();
	}
	if(!done)
		filesystem::copy_file(dest_path, source_path, filesystem::copy_option::overwrite_if_exists);
	filesystem::remove(dest_path);
}

string Stats::getKey(string key)
{
	size_t start = 0, end = 0;
	string result = "";

	while((start = data.find(key, start)) != string::npos)	// only check for newline at start if we have a pos > 1
	{
		if((start == 0 || data[start-1] == '\n') && (start+key.size() < data.size() && data[start+key.size()] == '='))
			break;
		//debug->notification(2, DEBUG, "GetKey(), key=%s, start=%i, end=%i", key.c_str(), start, end);
		start += key.size()+1;
	}

	size_t actualStart = start+key.size()+1;
	if(start != string::npos && (end = data.find_first_of('\n', actualStart)) != string::npos)
	{
		if(end != actualStart)		//the found key is empty
			result = data.substr(actualStart, (end-actualStart));
	}
	return result;
}

bool Stats::setKey(string key, string value, bool ignore_warning)
{
	size_t start = 0, end = 0;
	string result = "";

	while((start = data.find(key, start)) != string::npos)	// only check for newline at start if we have a pos > 1
	{
		if((start == 0 || data[start-1] == '\n') && (start+key.size() < data.size() && data[start+key.size()] == '='))
			break;
		//debug->notification(2, DEBUG, "SetKey(), key=%s, start=%i, end=%i", key.c_str(), start, end);
		start += key.size()+1;
	}

	size_t actualStart = start+key.size()+1;	// +1 for '='
	if(start != string::npos && (end = data.find_first_of('\n', actualStart)) != string::npos)
	{
		//we found the key so we modify it
		data.replace(actualStart, (end-actualStart), value);
		return true;
	}
	else
	{
		//no occurrence is found, something is wrong
		if(!ignore_warning)
			debug->warning(1, DEBUG, "Could not find key \"%s\" in persona file \"%s\"", key.c_str(), path.c_str());
		return false;
	}
}

bool Stats::isEmpty()
{
	return data.empty();
}

int Stats::loadDogtags()
{
	ifstream file(dogtagPath, ifstream::in);
	if(file.is_open() && file.good())
	{
		stringstream buffer;
		buffer << file.rdbuf();
		dogtags = buffer.str();
		file.close();
	}
	else
	{
		dogtags = "";
		debug->warning(3, DEBUG, "The dogtag file \"%s\" was not found or could not be opened, creating a new one...", dogtagPath.c_str());
		ofstream dogFile(dogtagPath, ios::out | ios::trunc);
		if(!dogFile.is_open())
			debug->warning(2, DEBUG, "Couldn't create dogtag file for '%s'", persona.c_str());
		else
			dogFile.close();
	}

	if(dogtags.empty())
		return 0;
	else
	{
		size_t count = 0, i = 0;
		while(i != string::npos)
		{
			i = dogtags.find("\n", i);
			if(i != string::npos)
			{
				count++;
				i++;
			}
		}
		debug->notification(3, DEBUG, "Found %i dogtags in \"%s\"", count, dogtagPath.c_str());
		return count;
	}
}

Stats::dogtag Stats::getDogtag(int lineNumber)
{
	int count = 0;
	size_t start = 0, end = 0;
	dogtag entry;
	entry.data = "";
	entry.id = -1;

	while(count != lineNumber && start != string::npos && start < dogtags.size())
	{
		if(start < 0)
		{
			debug->warning(2, DEBUG, "getDogtag() - The while condition failed, exiting loop! count=%i start=%i dogtags.size()=%i lineNumber=%i, persona=\"%s/%s\", data=\"%s\"", count, start, (int)dogtags.size(), lineNumber, email.c_str(), persona.c_str(), dogtags.c_str());
			break;
		}

		if((start = dogtags.find("\n", start)) != string::npos)
		{
			end = start;
			count++;
			start++;
		}
	}

	if(end > 0)
	{
		start = dogtags.rfind("=", end)+1;
		//if(start == string::npos)
			//start = 0;		//some problem with the data here, just put all data in it
		entry.data = dogtags.substr(start, end-start);


		if(start > 0)
			end = start-1;
		else
			end = 0;

		if((start = dogtags.rfind("\n", end)) == string::npos)
			start = 0;		//we are at the first line here (probably)
		else
			start++;		//we want to have the start index after \n

		entry.id = dogtags.substr(start, end-start);
		debug->notification(4, DEBUG, "Got dogtag \"%s=%s\" in \"%s\", line number = %i", entry.id.c_str(), entry.data.c_str(), dogtagPath.c_str(), lineNumber);
	}
	else
		debug->warning(2, DEBUG, "There is something wrong with the dogtags in \"%s\", line number = %i", dogtagPath.c_str(), lineNumber);

	return entry;
}

void Stats::addDogtag(dogtag newDogtag)
{
	string buffer = "\n";
	buffer.append(newDogtag.id);
	buffer.append("=");

	size_t start = 0;
	if((start = dogtags.find(buffer, 0)) == string::npos)
	{
		buffer.erase(buffer.begin());
		start = dogtags.find(buffer, 0);
	}
	else
		buffer.erase(buffer.begin());	// if we remove the \n again we don't have to change the position right?

	buffer.append(newDogtag.data);
	if(start != string::npos)			// if we already have that id in the dogtags we only want to update it
	{
		//if we update a dogtag we only replace the line, newline at the end is not included
		size_t end = dogtags.find("\n", start+1);
		if(end == string::npos)
		{
			debug->warning(3, DEBUG, "dogtags data in \"%s\", seems to be corrupted, check the file, start=%i, length=%i", dogtagPath.c_str(), start, end-start);
			end = start;
		}
		dogtags.replace(start, end-start, buffer);
		debug->notification(4, DEBUG, "Replaced dogtag %s in \"%s\", start=%i, length=%i", buffer.c_str(), dogtagPath.c_str(), start, end-start);
	}
	else
	{
		buffer.append("\n");		//if we put a new dogtag we want a newline at the end
		dogtags.append(buffer);
		debug->notification(4, DEBUG, "Added dogtag %s to \"%s\"", buffer.c_str(), dogtagPath.c_str());
	}
}

void Stats::saveDogtags()
{
	string tempFile = dogtagPath;
	tempFile.append(".dbk");

	filesystem::path source_path(dogtagPath), dest_path(tempFile);
	filesystem::copy_file(source_path, dest_path);
	if(!filesystem::exists(dest_path))
		return;

	bool done = false;
	ofstream file(dogtagPath, ofstream::out | ofstream::trunc);	//we delete all content, is it safe to do this?
	if(file.is_open() && file.good())
	{
		file.write(dogtags.c_str(), dogtags.size());
		if(!file.bad())
			done = true;
		file.close();
		debug->notification(3, DEBUG, "The dogtag file \"%s\" was saved", dogtagPath.c_str());
	}
	else
		debug->warning(3, DEBUG, "The dogtag file \"%s\" was not found or could not be opened for saving", dogtagPath.c_str());
	if(!done)
		filesystem::copy_file(dest_path, source_path, filesystem::copy_option::overwrite_if_exists);

	filesystem::remove(dest_path);
}

void Stats::deleteStats()
{
	filesystem::path stats_path(path), dog_path(dogtagPath);
	if(filesystem::remove(stats_path))
		debug->notification(3, DEBUG, "Deleted file '%s'", path.c_str());

	if(filesystem::remove(dog_path))
		debug->notification(3, DEBUG, "Deleted file '%s'", dogtagPath.c_str());
}
