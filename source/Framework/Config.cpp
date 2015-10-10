#include "Config.h"
#include <fstream>

Config::Config()
{

}

bool Config::loadConfigFile()
{
	ifstream configFile("./config.ini", ifstream::in);
	if(configFile.is_open() && configFile.good())
	{
		stringstream buffer;
		buffer << configFile.rdbuf();
		data = buffer.str();
		configFile.close();

		if(!storeValues())
		{
			cout << "Failed to load certain values in the config.ini, be sure that EVERY option \nhas a valid value and try it again.\n" << endl;
			cout << "...Press ENTER to quit..." << endl;
			cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
			return false;
		}
		else
			return true;
	}
	else
	{
		cout << "Failed to load config.ini\n\n...Press ENTER to quit..." << endl;
		cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
		return false;
	}
}

bool Config::storeValues()
{
	bool success = false;
	if(getValuesFromSection("[debug]"))
	{
		if(getValuesFromSection("[connection]"))
		{
			if(getValuesFromSection("[emulator]"))
				success = getValuesFromSection("[console]");
		}
	}

	return success;
}

bool Config::getValuesFromSection(const char* section)
{
	int success = 0;
	if(strcmp(section, "[debug]") == 0)
	{
		success += setValue(getValue("log_create"), debugCfg.log_create);
		success += setValue(getValue("log_timestamp"), debugCfg.log_timestamp);
		success += setValue(getValue("file_log_level"), debugCfg.file_notification_level);
		success += setValue(getValue("console_log_level"), debugCfg.console_notification_level);
		debugCfg.file_warning_level = debugCfg.file_notification_level;
		debugCfg.console_warning_level = debugCfg.console_notification_level;
		//success += setValue(getValue("file_warning_level"), debugCfg.file_warning_level);
		//success += setValue(getValue("console_warning_level"), debugCfg.console_warning_level);
		success += setValue(getValue("display_database_table_info"), debugCfg.display_db_extended);
	}
	else if(strcmp(section, "[connection]") == 0)
	{
		success += setValue(getValue("emulator_ip"), portsCfg.emulator_ip);
		if(portsCfg.emulator_ip.compare("REPLACE_ME") == 0)
		{
			cout << "'emulator_ip' still has its default value, its required to replace \"REPLACE_ME\"\nwith the network IP of the PC where you want to host this emulator for incoming\nconnections to be able to connect properly!\n" << endl;
			success++;
		}

		success += setValue(getValue("misc_port"), portsCfg.emulator_port);
		success += setValue(getValue("plasma_client_port"), portsCfg.plasma_client_port);
		success += setValue(getValue("theater_client_port"), portsCfg.theater_client_port);
		success += setValue(getValue("plasma_server_port"), portsCfg.plasma_server_port);
		success += setValue(getValue("theater_server_port"), portsCfg.theater_server_port);
		success += setValue(getValue("http_enabled"), portsCfg.use_http);
		//success += setValue(getValue("http_updater_respond"), portsCfg.updater_respond);
		portsCfg.updater_respond = false;
	}
	else if(strcmp(section, "[emulator]") == 0)
	{
		success += setValue(getValue("override_server_version"), emuCfg.global_server_version);
		success += setValue(getValue("enable_server_filters"), emuCfg.enable_server_filters);
		success += setValue(getValue("all_stats_unlocked"), emuCfg.all_stats_unlocked);
		success += setValue(getValue("all_are_veteran"), emuCfg.all_players_are_veterans);
		success += setValue(getValue("vietnam_for_all"), emuCfg.vietnam_for_all);
		success += setValue(getValue("premium_for_all"), emuCfg.premium_for_all);
		success += setValue(getValue("specact_for_all"), emuCfg.specact_for_all);
	}
	else if(strcmp(section, "[console]") == 0)
	{
		success += setValue(getValue("use_color"), conCfg.use_color);
		//success += setValue(getValue("use_default_console"), conCfg.use_default_console);
		conCfg.use_default_console = false;
		success += setValue(getValue("window_buffer_size"), conCfg.buffer_size);
		success += setValue(getValue("message_cut_off_length"), conCfg.message_cut_off_length);
	}

	if(success == 0)
		return true;
	else
		return false;
}

bool Config::setValue(string value, bool& storage)
{
	if(value.compare("false") == 0 || value.compare("0") == 0)
		storage = false;
	else if(value.compare("true") == 0 || value.compare("1") == 0)
		storage = true;
	else
		return true;

	return false;
}

bool Config::setValue(string value, int& storage)
{
	if(value.empty())
		return true;
	else
	{
		storage = lexical_cast<int>(value);
		return false;
	}
}

bool Config::setValue(string value, string& storage, bool allow_empty)
{
	if(!value.empty() || allow_empty)
	{
		storage.insert(0, value);
		return false;
	}
	else
		return true;
}

string Config::getValue(const char* key)
{
	string searchString = key;
	searchString.insert(searchString.begin(), '\n');
	string value = "";
	size_t position = data.find(searchString);
	if(position != string::npos)
	{
		position += searchString.size();
		while(data.at(position) == ' ' || data.at(position) == '=')
			position++;

		while(data.at(position) != '\t' && data.at(position) != '\n' && data.at(position) != ';' && data.at(position) != ' ')
		{
			value.push_back(data.at(position));
			position++;

			if(position >= data.size())
				break;
		}
	}
	return value;
}

debugSettings Config::getDebugCfg()
{
	return debugCfg;
}

consoleSettings Config::getConCfg()
{
	return conCfg;
}

emulatorSettings Config::getEmuCfg()
{
	return emuCfg;
}

connectionSettings Config::getPortsCfg()
{
	return portsCfg;
}
