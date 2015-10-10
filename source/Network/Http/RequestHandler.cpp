#include "RequestHandler.h"
#include "../../Framework/Database.h"
#include "../../Framework/Framework.h"
#include <fstream>

extern Database* db;
extern Framework* fw;

struct mapping
{
	const char* extension;
	const char* mime_type;
}mappings[] =
{
	{ "gif", "image/gif" },
	{ "htm", "text/html" },
	{ "html", "text/html" },
	{ "jpg", "image/jpeg" },
	{ "png", "image/png" },
	{ "xml", "text/xml" },
	{ 0, 0 } // Marks end of list.
};

string extension_to_type(const string& extension)
{
	for (mapping* m = mappings; m->extension; ++m)
	{
		if (m->extension == extension)
			return m->mime_type;
	}
	return "text/plain";
}

RequestHandler::RequestHandler(const string& doc_root) : doc_root_(doc_root)
{
}

void RequestHandler::handle_request(const request& req, reply& rep, bool updater_respond)
{
	// Decode url to path.
	string request_path;
	if (!url_decode(req.uri, request_path))
	{
		rep = reply::stock_reply(reply::bad_request);
		return;
	}

	// Request path must be absolute and not contain "..".
	if (request_path.empty() || request_path[0] != '/' || request_path.find("..") != string::npos)
	{
		rep = reply::stock_reply(reply::bad_request);
		return;
	}

	// If path ends in slash (i.e. is a directory) then add "index.html".
	if (request_path[request_path.size() - 1] == '/')
		request_path.append("index.html");

	// Determine the file extension.
	std::size_t last_slash_pos = request_path.find_last_of("/");
	std::size_t last_dot_pos = request_path.find_last_of(".");

	string extension;
	if (last_dot_pos != string::npos && last_dot_pos > last_slash_pos)
		extension = request_path.substr(last_dot_pos + 1);

	// Open the file to send back.
	if(updater_respond && request_path.compare("/easo/editorial/BF/2010/BFBC2/config/PC/InstallerConfig.xml") == 0)
		request_path = "installerConfig";
	else if(request_path.compare("/easo/editorial/BF/2010/BFBC2/config/PC/version") == 0)
		request_path = "version";
	else if(request_path.compare("/easo/editorial/BF/2010/BFBC2/config/PC/game.xml") == 0)
		request_path = "game";
	else if(request_path.find("/fileupload/locker2.jsp?site=easo&cmd=dir&lkey=") != string::npos)
	{
		size_t start = request_path.find("&pers="), end = request_path.find("&game=");
		if(start != string::npos && end != string::npos && start+6 < end)
		{
			start += 6;
			string name = request_path.substr(start, end-start);
			start = 0;
			//fw->convertHtml(&name);

			// these 2 chars are saved encoded in the database so we need to reverse these changes temporarily
			start = 0;
			string tempName = name;
			while((start = tempName.find('%', start)) != string::npos)	// do this first because its part of the encoding
			{
				tempName.replace(start, 1, "%25");
				start++;
			}
			start = 0;
			while((start = tempName.find('=', start)) != string::npos)
				tempName.replace(start, 1, "%3d");
			start = 0;
			while((start = tempName.find('\"', start)) != string::npos)
				tempName.replace(start, 1, "%22");

			list_entry persona;
			if(db->getPersona(tempName, &persona))
			{
				string locker = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<LOCKER error=\"0\" game=\"/eagames/bfbc2\" maxBytes=\"2867200\" maxFiles=\"10\" numBytes=\"0\" numFiles=\"0\" ownr=\"";
				locker.append(lexical_cast<string>(persona.id));
				locker.append("\" pers=\"");
				locker.append(name);
				locker.append("\"/>");

				// Fill out the reply to be sent to the client.
				rep.status = reply::ok;
				rep.content.append(locker.c_str(), locker.size());

				rep.headers.resize(2);
				rep.headers[0].name = "Content-Length";
				rep.headers[0].value = lexical_cast<string>(rep.content.size());
				rep.headers[1].name = "Content-Type";
				rep.headers[1].value = "text/xml";
				return;
			}
		}
	}
	else
		request_path = "";

	string full_path = doc_root_ + request_path;
	ifstream is(full_path.c_str(), ios::in | ios::binary);
	if (!is)
	{
		rep = reply::stock_reply(reply::not_found);
		return;
	}

	// Fill out the reply to be sent to the client.
	rep.status = reply::ok;
	char buf[512];
	while (is.read(buf, sizeof(buf)).gcount() > 0)
		rep.content.append(buf, (unsigned int)is.gcount());

	rep.headers.resize(2);
	rep.headers[0].name = "Content-Length";
	rep.headers[0].value = lexical_cast<string>(rep.content.size());
	rep.headers[1].name = "Content-Type";
	rep.headers[1].value = extension_to_type(extension);
}

bool RequestHandler::url_decode(const string& in, string& out)
{
	out.clear();
	out.reserve(in.size());
	for (std::size_t i = 0; i < in.size(); ++i)
	{
		if (in[i] == '%')
		{
			if (i + 3 <= in.size())
			{
				int value = 0;
				istringstream is(in.substr(i + 1, 2));
				if (is >> std::hex >> value)
				{
					out += static_cast<char>(value);
					i += 2;
				}
				else
					return false;
			}
			else
				return false;
		}
		else if (in[i] == '+')
			out += ' ';
		else
			out += in[i];
	}
	return true;
}
