#include "Packet.h"

Packet::Packet(const char* createType, unsigned int createType2, size_t createLength, const char* createData)
{
	type	= new char[HEADER_LENGTH+1];	// want a final NULL
	type2	= createType2;
	length	= createLength;
	count = true;
	first = true;
	encoded = false;
	delayed = false;
	delay_time = 0;

	strncpy(type, createType, HEADER_LENGTH);
	type[HEADER_LENGTH] = 0;

	// When no length and data
	if(createLength < 0)
		return;

	var_buffer = createData;
}

Packet::Packet(const char* createType, unsigned int createType2, bool count, bool first)
{
	type	= new char[HEADER_LENGTH+1];	// want a final NULL
	type2	= createType2;
	length	= 12;
	this->count = count;
	this->first = first;
	encoded = false;
	delayed = false;
	delay_time = 0;

	strncpy(type, createType, HEADER_LENGTH);
	type[HEADER_LENGTH] = 0;
}

Packet::Packet(Packet* packet)
{
	type	= new char[HEADER_LENGTH+1];	// want a final NULL
	type2	= packet->GetType2();
	length	= 12;
	count = packet->GetValCount();
	first = packet->GetValFirst();
	encoded = packet->isEncoded();
	delayed = packet->isDelayed();
	delay_time = packet->getDelayTime();

	strncpy(type, packet->GetType(), HEADER_LENGTH);
	type[HEADER_LENGTH] = 0;

	// When no length and data
	if(packet->GetLength() < 0)
		return;

	var_buffer = packet->GetData();
}

Packet::~Packet( )
{
	delete[] type;
	var_buffer.clear();
}

// old method (slower or just more code?)
/*string Packet::GetVar(string varname)
{
	//old version where I actually searched twice every time

	int start, end;
	string result = "";
	string searchString = varname;// + '=';
	searchString.append("=");

	start = var_buffer.find(searchString);
	//if the name we search for begins at 0 then we know for sure that there cannot be a second one we are searching for
	//because such a string would have a '\n' before its name which cannot be the case at the beginning
	if(start > 0)
	{
		//we know the wanted string isn't the first one so we have to do a second search
		//with a more unique searchString that makes sure we get the right one
		searchString.insert(0, "\n");
		start = var_buffer.find(searchString, start-1);	//step back one char to integrate a possible missing '\n' in the search
		//if we get no more results then the wanted name is not in our packet (thats a fact!)
	}
	end = var_buffer.find_first_of('\n', start+1);		//we are definitely searching after the start index

	if(start != string::npos && end != string::npos)
	{
		int actualStart = start+searchString.size();
		if(end != actualStart)		//the found key is empty
			result = var_buffer.substr(actualStart, (end-actualStart));
	}
	return result;
}

void Packet::SetVar(string varname, string varvalue, bool skipCheck)
{
	if(!skipCheck)		//ususally we don't set the same key twice so its save to skip a check for that
	{
		int start, end;
		string result = "";
		string searchString = varname;
		searchString.append("=");
		//if(GetNumberOfKeys() > 0)
			//searchString.insert(0, "\n");		//this helps searching for the exact name

		start = var_buffer.find(searchString);
		//if the name we search for begins at 0 then we know for sure that there cannot be a second one we are searching for
		//because such a string would have a '\n' before its name which cannot be the case at the beginning
		if(start > 0)
		{
			//we know the wanted string isn't the first one so we have to do a second search
			//with a more unique searchString that makes sure we get the right one
			searchString.insert(0, "\n");
			start = var_buffer.find(searchString, start-1);	//step back one char to integrate a possible missing '\n' in the search
			//if we get no more results then the wanted name is not in our packet (thats a fact!)
		}
		end = var_buffer.find_first_of('\n', start+1);		//we are definitely searching after the start index

		if(start == string::npos || end == string::npos)
		{
			//we have a new entry
			string newEntry = varname;
			newEntry.append("=");
			newEntry.append(varvalue);
			newEntry.append("\n");
			var_buffer.append(newEntry);
		}
		else
		{
			//the entry we want to set is already in the packet so we modify it
			int actualStart = start+searchString.size();
			var_buffer.replace(actualStart, (end-actualStart), varvalue);
		}
	}
	else
	{
		string newEntry = varname;
		newEntry.append("=");
		newEntry.append(varvalue);
		newEntry.append("\n");
		var_buffer.append(newEntry);
	}
}//*/

string Packet::GetVar(string varname)
{
	size_t start, end;
	string result = "";
	varname.insert(varname.begin(), '\n');
	varname.append("=");

	if((start = var_buffer.find(varname)) == string::npos)
	{
		varname.erase(varname.begin());
		start = var_buffer.find(varname);	// repeat search without including newline at beginning
	}
	//if the name we search for begins at 0 then we know for sure that there cannot be a second one we are searching for
	//because such a string would have a '\n' before its name which cannot be the case at the beginning

	size_t actualStart = start+varname.size();
	if(start != string::npos && (end = var_buffer.find('\n', actualStart)) != string::npos)
	{
		if(end != actualStart)					// the found key is empty
			result = var_buffer.substr(actualStart, (end-actualStart));
	}
	return result;
}

void Packet::SetVar(string varname, string varvalue, bool skipCheck)
{
	if(!skipCheck)		//ususally we don't set the same key twice so its save to skip a check for that
	{
		size_t start, end;
		string result = "";
		varname.insert(varname.begin(), '\n');
		varname.append("=");

		if((start = var_buffer.find(varname)) == string::npos)
		{
			varname.erase(varname.begin());
			start = var_buffer.find(varname);	// repeat search without including newline at beginning
		}
		//if the name we search for begins at 0 then we know for sure that there cannot be a second one we are searching for
		//because such a string would have a '\n' before its name which cannot be the case at the beginning

		if(start == string::npos || (end = var_buffer.find('\n', start+1)) == string::npos)
		{
			//we have a new entry
			varname.append("=");
			varname.append(varvalue);
			varname.append("\n");
			var_buffer.append(varname);
		}
		else
		{
			//the entry we want to set is already in the packet so we modify it
			int actualStart = start+varname.size();
			var_buffer.replace(actualStart, (end-actualStart), varvalue);
		}
	}
	else
	{
		varname.append("=");
		varname.append(varvalue);
		varname.append("\n");
		var_buffer.append(varname);
	}
}

string Packet::GetVar(const char* varname)
{
	string varnam = varname;
	return GetVar(varnam);
}

int Packet::GetNumberOfKeys()
{
	// just count the number of newlines for this
	size_t buffer = 0, size = 0;
	while((buffer = var_buffer.find('\n', buffer)) != string::npos)
	{
		size++;
		buffer++;	// update starting point as well or this ends up in an infite loop
	}
	return size;
}

string Packet::toString()
{
	string readable = var_buffer;
	if(!readable.empty())
	{
		size_t buffer = 0;
		while((buffer = readable.find('\n', buffer)) != string::npos && buffer < readable.size())
		{
			if(buffer < readable.size()-1)
				readable.replace(buffer, 1, ", ");
			else
				readable.pop_back();	// delete last newline (only gets executed if last char is really \n)
			buffer++;			// increased the string +1 so update the current index
		}
	}
	return readable;
}

void Packet::SetVar(string varname, int varvalue, bool skipCheck)
{
	SetVar(varname, lexical_cast<string>(varvalue), skipCheck);
}

void Packet::SetType2(unsigned int type2)
{
	this->type2 = type2;
}

string Packet::GetData( )
{
	return var_buffer;
}

char* Packet::GetType()
{
	return type;
}

unsigned int Packet::GetType2()
{
	return type2;
}

int Packet::GetLength()
{
	return length;
}

bool Packet::isEncoded()
{
	return encoded;
}

void Packet::isEncoded(bool flag)
{
	encoded = flag;
}

bool Packet::isDelayed()
{
	return delayed;
}

int Packet::getDelayTime()
{
	return delay_time;
}

void Packet::isDelayed(bool flag, int seconds)
{
	delayed = flag;
	delay_time = seconds;
}

bool Packet::GetValCount()
{
	return count;
}

bool Packet::GetValFirst()
{
	return first;
}
