#include "GameServer.h"
#include "Framework.h"
#include "../Base64.h"

extern Logger* debug;
extern Framework* fw;

GameServer::GameServer(int connectionType, Database* db)
{
	this->connectionType = connectionType;
	this->logDatabase = fw->DatabaseLogging();
	database = db;

	game.key = NULL;
	game.gdet = NULL;
	sock_tid	= 0;
	gid			= -1;
	player_count = 0;
}

GameServer::~GameServer()
{
	if(gid != -1 && connectionType == GAME_SERVER)		// we only use gid in theater
		database->removeGame(gid);
}

bool GameServer::acct(list<Packet*>* queue, Packet* packet, string txn)
{
	Packet* sendPacket = NULL;

	if(txn.compare("NuLogin") == 0)
	{
		database->getUser(packet->GetVar("nuid"), &user);
		sendPacket = new Packet("acct", 0x80000000);
		sendPacket->SetVar("TXN", txn);

		if(user.id > 0 && user.data[PASSWORD].compare(packet->GetVar("password")) == 0)
		{
			user_lkey = fw->randomString(27, SEED_DEFAULT);
			user_lkey.append(".");

			sendPacket->SetVar("nuid", user.name);
			sendPacket->SetVar("lkey", user_lkey);
			sendPacket->SetVar("profileId", user.id);
			sendPacket->SetVar("userId", user.id);
			string encryptedInfo = packet->GetVar("returnEncryptedInfo");
			if(!encryptedInfo.empty() && (encryptedInfo.compare("1") == 0))
			{
				string encrypted("Ciyvab0tregdVsBtboIpeChe4G6uzC1v5_-SIxmvSL");
				encrypted.append(fw->randomString(86, SEED_DEFAULT));
				sendPacket->SetVar("encryptedLoginInfo", encrypted);	//lets just assume one half of that string is constant, the other is random
				//if this is missing, server shows at OnConnectionReady only zeroes - though this doesnt seem to influence the connection?
				//sendPacket->SetVar("encryptedLoginInfo", "Ciyvab0tregdVsBtboIpeChe4G6uzC1v5_-SIxmvSLJubxCaMWOxD1iabTkdIjA6nFyrA9nvKvtbovnovBD99T8hIHEGPkl-I9xeEXAXN9ICCmuaWcMHP6q33WjvASgI");
				//Ciyvab0tregdVsBtboIpeChe4G6uzC1v5_-SIxmvSLJubxCaMWOxD1iabTkdIjA6nFyrA9nvKvtbovnovBD99T8hIHEGPkl-I9xeEXAXN9ICCmuaWcMHP6q33WjvASgI
				//Ciyvab0tregdVsBtboIpeChe4G6uzC1v5_-SIxmvSLLA2SDISJCuIEL6m-ixPfA4rGinOqN3E8Cie2MlUFu4Q_weSUURodQ9Mg8DS5xtFIUaMKYq-iYiCaVLsGiZ8SGD
				//Ciyvab0tregdVsBtboIpeChe4G6uzC1v5_-SIxmvSLKoLAEQKPuixjvalEgthqSGBY-Q0KMaLiq3nF71Vb3f0i60Im6crOnhh3EZWCZs-iarYM-C9yhxjw0V66Uqm0CK
			}
			if(logDatabase)
				debug->simpleNotification(3, DATABASE, database->listEntryToString(&user, "logged in", true).c_str());
		}
	}

	else if(txn.compare("NuGetPersonas") == 0)
	{
		// we can actually assume this is always the same (only multiple personas for clients)
		sendPacket = new Packet("acct", 0x80000000);
		sendPacket->SetVar("TXN", txn);
		sendPacket->SetVar("personas.[]", "1");
		switch(user.id)
		{
			default:
			case 1:
				sendPacket->SetVar("personas.0", "bfbc2.server.p");
				break;
			case 2:
				sendPacket->SetVar("personas.0", "bfbc.server.ps");
				break;
			case 3:
				sendPacket->SetVar("personas.0", "bfbc.server.xe");
				break;
		}
	}

	else if(txn.compare("NuLoginPersona") == 0)
	{
		sendPacket = new Packet("acct", 0x80000000);
		sendPacket->SetVar("TXN", txn);
		if(database->getPersona(packet->GetVar("name"), &persona))
		{
			persona_lkey = fw->randomString(27, SEED_DEFAULT);
			persona_lkey.append(".");
			sendPacket->SetVar("lkey", persona_lkey);
			sendPacket->SetVar("profileId", persona.id);
			sendPacket->SetVar("userId", persona.id);

			if(logDatabase)
				debug->simpleNotification(3, DATABASE, database->listEntryToString(&persona, "logged in", false).c_str());
		}
	}

	else if(txn.compare("NuAddPersona") == 0)	//if the server accounts are unaccessible for users then we never get here anyway
	{
		string name = packet->GetVar("name");
		//add persona here but is not required for servers
		sendPacket = new Packet("acct", 0x80000000);
		sendPacket->SetVar("TXN", txn);
	}

	else if(txn.compare("NuGetEntitlements") == 0)
	{
		string groupName = packet->GetVar("groupName");
		int playerUserId = lexical_cast<int>(packet->GetVar("masterUserId"));

		if(!groupName.empty())
		{
			if(groupName.compare("BFBC2PC") == 0)
			{
				//TODO: make the BFBC2 entitlements database
				sendPacket = new Packet("acct", 0x80000000);
				sendPacket->SetVar("TXN", txn);
				sendPacket->SetVar("entitlements.[]", "0");
			}
			else if(groupName.compare("AddsVetRank") == 0)
			{
				sendPacket = new Packet("acct", 0x80000000);
				sendPacket->SetVar("TXN", txn);
				int count = 0;
				sendPacket->SetVar("entitlements.0.statusReasonCode", "");
				sendPacket->SetVar("entitlements.0.groupName", "AddsVetRank");
				sendPacket->SetVar("entitlements.0.grantDate", "2011-07-30T0%3a11Z");
				sendPacket->SetVar("entitlements.0.version", "0");
				sendPacket->SetVar("entitlements.0.entitlementId", "1114495162");
				sendPacket->SetVar("entitlements.0.terminationDate", "");
				sendPacket->SetVar("entitlements.0.productId", "");
				sendPacket->SetVar("entitlements.0.entitlementTag", "BFBC2%3aPC%3aADDSVETRANK");
				sendPacket->SetVar("entitlements.0.status", "ACTIVE");
				sendPacket->SetVar("entitlements.0.userId", playerUserId);
				count++;
				if(fw->emuCfg().all_players_are_veterans)
				{
					sendPacket->SetVar("entitlements.1.statusReasonCode", "NONE");
					sendPacket->SetVar("entitlements.1.groupName", "AddsVetRank");
					sendPacket->SetVar("entitlements.1.grantDate", "2011-10-27T6%3a49Z");
					sendPacket->SetVar("entitlements.1.version", "0");
					sendPacket->SetVar("entitlements.1.entitlementId", "1272849755");
					sendPacket->SetVar("entitlements.1.terminationDate", "");
					sendPacket->SetVar("entitlements.1.productId", "OFB-EAST%3a40873");
					sendPacket->SetVar("entitlements.1.entitlementTag", "BF3%3aPC%3aADDSVETRANK");
					sendPacket->SetVar("entitlements.1.status", "ACTIVE");
					sendPacket->SetVar("entitlements.1.userId", playerUserId);
					count++;
				}
				sendPacket->SetVar("entitlements.[]", count);
			}
			else if(groupName.compare("BattlefieldBadCompany2") == 0)
			{
				sendPacket = new Packet("acct", 0x80000000);
				sendPacket->SetVar("TXN", txn);
				sendPacket->SetVar("entitlements.[]", 0);
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
				sendPacket = new Packet("acct", 0x80000000);
				sendPacket->SetVar("TXN", txn);
				sendPacket->SetVar("entitlements.[]", "0");
			}
		}
		else
		{
			string entitlementTag = packet->GetVar("entitlementTag");
			string projectId = packet->GetVar("projectId");
			if(!entitlementTag.empty() && entitlementTag.compare("BFBC2:PC:VIETNAM_ACCESS") == 0)
			{
				sendPacket = new Packet("acct", 0x80000000);
				sendPacket->SetVar("TXN", txn);
				if(fw->emuCfg().vietnam_for_all)
				{
					sendPacket->SetVar("entitlements.[]", "1");
					sendPacket->SetVar("entitlements.0.statusReasonCode", "");
					sendPacket->SetVar("entitlements.0.groupName", "BFBC2PC");
					sendPacket->SetVar("entitlements.0.grantDate", "2010-12-23T9%3a6Z");
					sendPacket->SetVar("entitlements.0.version", "0");
					sendPacket->SetVar("entitlements.0.entitlementId", "1");
					sendPacket->SetVar("entitlements.0.terminationDate", "");
					sendPacket->SetVar("entitlements.0.productId", "DR%3a219316800");
					sendPacket->SetVar("entitlements.0.entitlementTag", "BFBC2%3aPC%3aVIETNAM_ACCESS");
					sendPacket->SetVar("entitlements.0.status", "ACTIVE");
					sendPacket->SetVar("entitlements.0.userId", playerUserId);
				}
				else
					sendPacket->SetVar("entitlements.[]", "0");
			}
			else
			{
				if(!projectId.empty() && projectId.compare("136844") == 0)
				{
					sendPacket = new Packet("acct", 0x80000000);
					sendPacket->SetVar("TXN", txn);
					//sendPacket->SetVar("entitlements.[]", 5);
					//all clients always have these 3 packets (for sure?)
					sendPacket->SetVar("entitlements.0.userId", playerUserId);
					sendPacket->SetVar("entitlements.0.groupName", "NoVetRank");
					sendPacket->SetVar("entitlements.0.entitlementId", "1114495797");
					sendPacket->SetVar("entitlements.0.entitlementTag", "BFBC2NAM%3aPC%3aNOVETRANK");
					sendPacket->SetVar("entitlements.0.productId", "");
					sendPacket->SetVar("entitlements.0.version", "0");
					sendPacket->SetVar("entitlements.0.grantDate", "2011-07-30T0%3a11Z");
					sendPacket->SetVar("entitlements.0.terminationDate", "");
					sendPacket->SetVar("entitlements.0.statusReasonCode", "");
					sendPacket->SetVar("entitlements.0.status", "ACTIVE");

					sendPacket->SetVar("entitlements.1.userId", playerUserId);
					sendPacket->SetVar("entitlements.1.groupName", "");
					sendPacket->SetVar("entitlements.1.entitlementId", "1114490796");
					sendPacket->SetVar("entitlements.1.entitlementTag", "ONLINE_ACCESS");
					sendPacket->SetVar("entitlements.1.productId", "DR%3a156691300");
					sendPacket->SetVar("entitlements.1.version", "0");
					sendPacket->SetVar("entitlements.1.grantDate", "2011-07-30T0%3a6Z");
					sendPacket->SetVar("entitlements.1.terminationDate", "");
					sendPacket->SetVar("entitlements.1.statusReasonCode", "");
					sendPacket->SetVar("entitlements.1.status", "ACTIVE");

					//sometimes clients don't have this in the packet...whatever
					sendPacket->SetVar("entitlements.2.userId", playerUserId);
					sendPacket->SetVar("entitlements.2.groupName", "AddsVetRank");
					sendPacket->SetVar("entitlements.2.entitlementId", "1114495162");
					sendPacket->SetVar("entitlements.2.entitlementTag", "BFBC2%3aPC%3aADDSVETRANK");
					sendPacket->SetVar("entitlements.2.productId", "");
					sendPacket->SetVar("entitlements.2.version", "0");
					sendPacket->SetVar("entitlements.2.grantDate", "2011-07-30T0%3a11Z");
					sendPacket->SetVar("entitlements.2.terminationDate", "");
					sendPacket->SetVar("entitlements.2.statusReasonCode", "");
					sendPacket->SetVar("entitlements.2.status", "ACTIVE");

					// beta access is nice too (though it doesnt seem to affect anything)
					sendPacket->SetVar("entitlements.3.userId", playerUserId);
					sendPacket->SetVar("entitlements.3.groupName", "BFBC2PC");
					sendPacket->SetVar("entitlements.3.entitlementId", "432044675");
					sendPacket->SetVar("entitlements.3.entitlementTag", "BETA_ONLINE_ACCESS");
					sendPacket->SetVar("entitlements.3.productId", "OFB-BFBC%3a19121");
					sendPacket->SetVar("entitlements.3.version", "0");
					sendPacket->SetVar("entitlements.3.grantDate", "2010-02-09T18%3a5Z");
					sendPacket->SetVar("entitlements.3.terminationDate", "");
					sendPacket->SetVar("entitlements.3.statusReasonCode", "");
					sendPacket->SetVar("entitlements.3.status", "ACTIVE");

					if(fw->emuCfg().premium_for_all)		//this is where the specact packet is (sometimes at least, don't know what defines that but it seems irrelevant)
					{
						sendPacket->SetVar("entitlements.4.userId", playerUserId);
						sendPacket->SetVar("entitlements.4.groupName", "BFBC2PC");
						sendPacket->SetVar("entitlements.4.entitlementId", "3");
						sendPacket->SetVar("entitlements.4.entitlementTag", "BFBC2%3aPC%3aLimitedEdition");
						sendPacket->SetVar("entitlements.4.productId", "OFB-BFBC%3a19120");
						sendPacket->SetVar("entitlements.4.version", "1");
						sendPacket->SetVar("entitlements.4.grantDate", "2011-07-30T0%3a11Z");
						sendPacket->SetVar("entitlements.4.terminationDate", "");
						sendPacket->SetVar("entitlements.4.statusReasonCode", "");
						sendPacket->SetVar("entitlements.4.status", "ACTIVE");
						sendPacket->SetVar("entitlements.[]", "5");
					}
					else
						sendPacket->SetVar("entitlements.[]", "4");
				}
				else if(!projectId.empty() && projectId.compare("302061") == 0)			//specact packet did only appear here during traffic
				{
					sendPacket = new Packet("acct", 0x80000000);
					sendPacket->SetVar("TXN", txn);
					if(fw->emuCfg().specact_for_all)
					{
						sendPacket->SetVar("entitlements.[]", "1");
						sendPacket->SetVar("entitlements.0.userId", playerUserId);
						sendPacket->SetVar("entitlements.0.groupName", "BFBC2PC");
						sendPacket->SetVar("entitlements.0.entitlementId", "4");
						sendPacket->SetVar("entitlements.0.entitlementTag", "BFBC2%3aPC%3aALLKIT");
						sendPacket->SetVar("entitlements.0.productId", "DR%3a192365600");
						sendPacket->SetVar("entitlements.0.version", "0");
						sendPacket->SetVar("entitlements.0.grantDate", "2010-12-21T22%3a34Z");
						sendPacket->SetVar("entitlements.0.terminationDate", "");
						sendPacket->SetVar("entitlements.0.statusReasonCode", "");
						sendPacket->SetVar("entitlements.0.status", "ACTIVE");
					}
					else
						sendPacket->SetVar("entitlements.[]", "0");
				}
				else
				{
					debug->warning(2, connectionType, "Could not handle TXN=%s projectId=%s", txn.c_str(), projectId.c_str());
					sendPacket = new Packet("acct", 0x80000000);
					sendPacket->SetVar("TXN", txn);
					sendPacket->SetVar("entitlements.[]", "0");
				}
			}
		}
	}

	else if(txn.compare("NuGrantEntitlement") == 0)		//what does this do?
	{
		//->N req acct 0xc0000011 {TXN=NuGrantEntitlement, entitlementTag=BFBC2:PC:ADDSVETRANK, groupName=AddsVetRank, masterId=2814702718}
		sendPacket = new Packet("acct", 0x80000000);
		sendPacket->SetVar("TXN", txn);
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

bool GameServer::asso(list<Packet*>* queue, Packet* packet, string txn)
{
	Packet* sendPacket = NULL;

	if(txn.compare("GetAssociations") == 0)
	{
		sendPacket = new Packet("asso", 0x80000000);
		sendPacket->SetVar("domainPartition.domain", "eagames");
		sendPacket->SetVar("domainPartition.subDomain", "BFBC2");
		sendPacket->SetVar("TXN", txn);
		sendPacket->SetVar("members.[]", "0");
		sendPacket->SetVar("owner.id", packet->GetVar("owner.id"));
		sendPacket->SetVar("owner.type", "1");

		string type = packet->GetVar("type");
		if(type.compare("PlasmaFriends") == 0)
		{
			//TODO: make the friends list database
			sendPacket->SetVar("maxListSize", "20");
			sendPacket->SetVar("owner.name", persona.name);
			sendPacket->SetVar("type", "PlasmaFriends");
		}
		else if(type.compare("PlasmaMute") == 0)
		{
			//TODO: make the mute list database
			sendPacket->SetVar("maxListSize", "20");
			sendPacket->SetVar("owner.name", persona.name);
			sendPacket->SetVar("type", "PlasmaMute");
		}
		else if(type.compare("PlasmaBlock") == 0)
		{
			//TODO: make the block list database
			sendPacket->SetVar("maxListSize", "20");
			sendPacket->SetVar("owner.name", persona.name);
			sendPacket->SetVar("type", "PlasmaBlock");
		}
		else if(type.compare("PlasmaRecentPlayers") == 0)
		{
			//TODO: make the recent list database
			sendPacket->SetVar("maxListSize", "100");
			sendPacket->SetVar("type", "PlasmaRecentPlayers");
		}
		else if(type.compare("dogtags") == 0)
		{
			sendPacket->SetVar("maxListSize", "20");
			sendPacket->SetVar("owner.name", persona.name);
			sendPacket->SetVar("type", "dogtags");
		}
		else
			debug->warning(1, connectionType, "Could not handle TXN=%s type=%s", txn.c_str(), type.c_str());
	}

	else if(txn.compare("AddAssociations") == 0)
	{
		/*this is not necessary since we are getting the user_id when GetStats is called anyway
		it may get relevent when we want to implement friends though

		tempPid = lexical_cast<int>(packet->GetVar("addRequests.0.member.id"));
		list_entry tempPlayer;

		if(!database->getPersona(tempPid, &tempPlayer))
			debug->warning(1, connectionType, "Couldn't find persona id %i in database!!", tempPid);
		*/
		sendPacket = new Packet("asso", 0x80000000);
		sendPacket->SetVar("TXN", txn);
		sendPacket->SetVar("domainPartition.domain", "eagames");
		sendPacket->SetVar("domainPartition.subDomain", "BFBC2");
		sendPacket->SetVar("maxListSize", "100");
		sendPacket->SetVar("type", "PlasmaRecentPlayers");
		sendPacket->SetVar("result.[]", "0");
		//->N req asso 0xc0000009 {TXN=AddAssociations, domainPartition.domain=eagames, domainPartition.subDomain=BFBC2, domainPartition.key=, type=PlasmaRecentPlayers, listFullBehavior=RollLeastRecentlyModified, addRequests.[]=1, addRequests.0.owner.id=224444376, addRequests.0.owner.type=1, addRequests.0.member.id=335897299, addRequests.0.member.type=1, addRequests.0.mutual=0}
		//<-O res asso 0x80000009 {result.[]=0, domainPartition.domain=eagames, domainPartition.subDomain=BFBC2, maxListSize=100, TXN=AddAssociations, type=PlasmaRecentPlayers}
	}

	else
		return true;

	if(sendPacket)
		queue->push_back(sendPacket);

	return false;
}

int GameServer::rank(list<Packet*>* queue, Packet* packet, string txn)
{
	int state = NORMAL;
	Packet* sendPacket = NULL;

	if(txn.compare("GetStats") == 0)
	{
		int playerPersonaId = lexical_cast<int>(packet->GetVar("owner"));
		list_entry result;

		if(database->getPersona(playerPersonaId, &result))
		{
			Stats* stats_data = new Stats(result.data[USER_EMAIL], result.name);
			sendPacket = new Packet("rank", 0x80000000);
			sendPacket->SetVar("TXN", txn);

			string buffer = packet->GetVar("keys.[]");
			int count = lexical_cast<int>(buffer);
			sendPacket->SetVar("stats.[]", buffer);

			char iterBuf[24];
			for(int i=0; i < count; i++)
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
		else
		{
			debug->warning(1, connectionType, "Couldn't find persona id %i in database!!", playerPersonaId);
			sendPacket = new Packet("rank", 0x80000000);
			sendPacket->SetVar("TXN", txn);
			sendPacket->SetVar("stats.[]", "0");
		}
	}

	else if(txn.compare("UpdateStats") == 0)
	{
		//this is coming sometimes from the server (not on EA though?) so we have to accept only u.0.ot=1 to ignore this case
		//rank 0xc000001c {TXN=UpdateStats, u.0.o=100, u.0.ot=11, u.0.s.[]=1, u.0.s.0.ut=3, u.0.s.0.k=veteran, u.0.s.0.v=1.0000, u.0.s.0.pt=0, u.[]=1}

		//TXN=UpdateStats, u.0.o=6, u.0.ot=1, u.0.s.[]=25, u.0.s.0.ut=3, u.0.s.0.k=c_9a91__sw_g, u.0.s.0.v=566.1666, u.0.s.0.pt=0, u.0.s.1.ut=3, u.0.s.1.k=c__de_ko_g, u.0.s.1.v=1.0000, u.0.s.1.pt=0, u.0.s.2.ut=3, u.0.s.2.k=c_ah64__ddw_g, u.0.s.2.v=63.5230, u.0.s.2.pt=0, u.0.s.3.ut=3, u.0.s.3.k=c_ah64__si_g, u.0.s.3.v=15.0667, u.0.s.3.pt=0, u.0.s.4.ut=1, u.0.s.4.k=c_brmel_mel__kw_g, u.0.s.4.v=1.0000, u.0.s.4.pt=0, u.0.s.5.ut=1, u.0.s.5.k=c_gomel_mel__kw_g, u.0.s.5.v=1.0000, u.0.s.5.pt=0, u.0.s.6.ut=1, u.0.s.6.k=c_i04_mel__kw_g, u.0.s.6.v=1.0000, u.0.s.6.pt=0, u.0.s.7.ut=1, u.0.s.7.k=c_i05_mel__kw_g, u.0.s.7.v=1.0000, u.0.s.7.pt=0, u.0.s.8.ut=1, u.0.s.8.k=c_i20___ko_g, u.0.s.8.v=1.0000, u.0.s.8.pt=0, u.0.s.9.ut=1, u.0.s.9.k=c_i27___sa_g, u.0.s.9.v=567.3000, u.0.s.9.pt=0, u.0.s.10.ut=1, u.0.s.10.k=c_i28___sa_g, u.0.s.10.v=567.3000, u.0.s.10.pt=0, u.0.s.11.ut=1, u.0.s.11.k=c_i29___sa_g, u.0.s.11.v=567.3000, u.0.s.11.pt=0, u.0.s.12.ut=1, u.0.s.12.k=c_i30___fc_g, u.0.s.12.v=1.0000, u.0.s.12.pt=0, u.0.s.13.ut=3, u.0.s.13.k=c_knv__kw_g, u.0.s.13.v=1.0000, u.0.s.13.pt=0, u.0.s.14.ut=3, u.0.s.14.k=c_knv__sw_g, u.0.s.14.v=1.1334, u.0.s.14.pt=0, u.0.s.15.ut=1, u.0.s.15.k=c_plmel_mel__kw_g, u.0.s.15.v=1.0000, u.0.s.15.pt=0, u.0.s.16.ut=1, u.0.s.16.k=c_simel_mel__kw_g, u.0.s.16.v=1.0000, u.0.s.16.pt=0, u.0.s.17.ut=1, u.0.s.17.k=c_ta38___sa_g, u.0.s.17.v=567.3000, u.0.s.17.pt=0, u.0.s.18.ut=3, u.0.s.18.k=dogt, u.0.s.18.v=1.0000, u.0.s.18.pt=0, u.0.s.19.ut=0, u.0.s.19.k=elo, u.0.s.19.v=4.9848, u.0.s.19.t=20130802, u.0.s.19.pt=0, u.0.s.20.ut=3, u.0.s.20.k=kills, u.0.s.20.v=1.0000, u.0.s.20.pt=0, u.0.s.21.ut=3, u.0.s.21.k=sc_demo, u.0.s.21.v=200.0000, u.0.s.21.pt=0, u.0.s.22.ut=3, u.0.s.22.k=sc_general, u.0.s.22.v=50.0000, u.0.s.22.pt=0, u.0.s.23.ut=3, u.0.s.23.k=sc_objective, u.0.s.23.v=150.0000, u.0.s.23.pt=0, u.0.s.24.ut=3, u.0.s.24.k=time, u.0.s.24.v=567.3000, u.0.s.24.pt=0, u.[]=1}
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
					//debug->notification(3, connectionType, "Updated key %s with value \"%s\", old value=\"%s\", updateMethod=%i", key.c_str(), value.c_str(), buffer, updateMethod);
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
	else
		state = UNKNOWN;

	if(sendPacket)
		queue->push_back(sendPacket);

	return state;
}

bool GameServer::pres(list<Packet*>* queue, Packet* packet, string txn)
{
	Packet* sendPacket = NULL;

	if(txn.compare("SetPresenceStatus") == 0)
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

bool GameServer::fltr(list<Packet*>* queue, Packet* packet, string txn)
{
	Packet* sendPacket = NULL;
	if(txn.compare("FilterProfanity") == 0)
	{
		sendPacket = new Packet("fltr", 0x80000000);
		sendPacket->SetVar("TXN", txn);
		sendPacket->SetVar("realtime", packet->GetVar("realtime"));
		sendPacket->SetVar("subChar", packet->GetVar("subChar"));
		int count = lexical_cast<int>(packet->GetVar("strings.[]"));
		sendPacket->SetVar("strings.[]", count);
		for(int i = 0; i < count; i++)
		{
			string buffer = "strings.";
			buffer.append(lexical_cast<string>(i));
			string buffer2 = buffer;
			buffer.append(".filtered");
			sendPacket->SetVar(buffer, packet->GetVar(buffer2));

			buffer2.append(".status");
			sendPacket->SetVar(buffer2, "0");	// 1 = it was a bad word; 0 = it was not a bad word (set it constant for simplicities sake)
		}
	}
	else
		return true;

	if(sendPacket)
		queue->push_back(sendPacket);

	return false;
}

//////////////////////////////
// Theater processing stuff //
//////////////////////////////
int GameServer::ProcessTheater(list<Packet*>* queue, Packet* packet, char* type, const char* ip)
{
	Packet* sendPacket = NULL;
	int state = NORMAL;

	if(strcmp(type, "CONN") == 0)
	{
		clientVersion = packet->GetVar("VERS");
		//locale = packet->GetVar("LOCALE");
		//platform = packet->GetVar("PLAT");

		sendPacket = new Packet("CONN", 0x00000000);
		sendPacket->SetVar("TIME", (int)time(NULL));
		sendPacket->SetVar("TID", ++sock_tid);
		sendPacket->SetVar("activityTimeoutSecs", "240");
		sendPacket->SetVar("PROT", packet->GetVar("PROT"));
		queue->push_back(sendPacket);
	}

	else if(strcmp(type, "PING") == 0)
	{
		state = SKIP;
	}

	else if(strcmp(type, "USER") == 0)
	{
		persona_lkey = packet->GetVar("LKEY").c_str();
		platform = packet->GetVar("SKU");

		if(platform.compare("PC") == 0)
			database->getPersona(1, &persona);
		else if(platform.compare("ps3") == 0)
			database->getPersona(2, &persona);
		else	//xbox
		{
			debug->warning(3, connectionType, "USER - unknown SKU (if this is xbox its okay): %s", platform.c_str());
			database->getPersona(3, &persona);
		}

		sendPacket = new Packet("USER", 0x00000000);
		sendPacket->SetVar("NAME", persona.name);
		sendPacket->SetVar("TID", ++sock_tid);
		queue->push_back(sendPacket);
	}

	else if(strcmp(type, "CGAM") == 0)
	{
		sendPacket = new Packet("CGAM", 0x00000000);
		sendPacket->SetVar("TID", ++sock_tid);

		game.key = new string[GAMES_SIZE];
		game.gdet = new string[GDET_SIZE];	//we need to allocate the space for gdet here as well before adding it to database!!
		//(otherwise the pointer stored in the database will not point to the same location)

		if(!fw->isEmuLocal())   // was a public IP defined?
		{
			if(packet->GetVar("INT_IP").compare(ip) != 0)	// if these IP's are not the same then check if the remote IP is public (is this even necessary?)
			{
				if(!fw->isIpLocal(ip))	// server is in another network so send public ip to client
					game.key[IP] = ip;				// IP (probably outside server ip)
				else					// everyone is local
					game.key[IP] = fw->portsCfg().emulator_ip;	// we want this if the server is in the same network as the client
			}
			else// if these IP's are the same then the server is most likely in the same network as the emulator so put the emulator's public ip as the server public ip
				game.key[IP] = fw->portsCfg().emulator_ip;
		}
		else
			game.key[IP] = ip;	// so in that case we put the IP from the config here (hopefully the internal ip we get afterwards is correct)

		game.key[PORT] = packet->GetVar("PORT");
		game.key[INTERNAL_IP] = packet->GetVar("INT-IP");
		game.key[INTERNAL_PORT] = packet->GetVar("INT-PORT");

		game.key[SERVER_NAME] = packet->GetVar("NAME");
		game.key[SERVER_LEVEL] = "";
		game.key[ACTIVE_PLAYERS] = "0";
		game.key[MAX_PLAYERS] = packet->GetVar("MAX-PLAYERS");
		game.key[QUEUE_LENGTH] = "0";		//what about packet->GetVar("QLEN")?
		game.key[JOINING_PLAYERS] = "0";

		game.key[GAMEMODE] = "";
		game.key[SOFTCORE] = "0";
		game.key[HARDCORE] = packet->GetVar("B-U-Hardcore");
		game.key[HAS_PASSWORD] = packet->GetVar("B-U-HasPassword");
		game.key[EA] = "0";
		game.key[GAME_MOD] = "";
		game.key[TIME] = "";

		game.key[PUNKBUSTER] = "0";/*packet->GetVar("B-U-Punkbuster");	// permanently disable punkbuster for all servers
		if(game.key[PUNKBUSTER].compare("1") == 0)
			game.key[PUNKBUSTER_VERSION] = "\"v1.826 | A1382 C2.277\"";
		else*/
			game.key[PUNKBUSTER_VERSION] = "";
		game.key[PLATFORM] = platform;
		game.key[REGION] = "";
		string serverVersion = packet->GetVar("B-version");
		if(fw->emuCfg().global_server_version.compare("false") != 0)
			serverVersion = fw->emuCfg().global_server_version;
		game.key[VERSION] = serverVersion;
		game.key[CLIENT_VERSION] = clientVersion;

		game.key[IS_PUBLIC] = "";
		game.key[ELO] = "1000";
		game.key[NUM_OBSERVERS] = packet->GetVar("B-numObservers");
		game.key[MAX_OBSERVERS] = packet->GetVar("B-maxObservers");

		game.key[SGUID] = "";
		game.key[HASH] = "";
		game.key[PROVIDER] = "";
		string ugid = packet->GetVar("UGID");
		game.key[UGID] = ugid;
		game.key[TYPE] = packet->GetVar("TYPE");
		game.key[JOIN] = packet->GetVar("JOIN");

		for(int i = 0; i < GDET_SIZE; i++)
			game.gdet[i] = "";

		gid = database->addGame(game);
		sendPacket->SetVar("MAX-PLAYERS", game.key[MAX_PLAYERS]);
		sendPacket->SetVar("EKEY", "AIBSgPFqRDg0TfdXW1zUGa4%3d");	//ALIbKIHiufVFQz2KEzUuF14%3d
		sendPacket->SetVar("UGID", ugid);
		sendPacket->SetVar("JOIN", game.key[JOIN]);
		sendPacket->SetVar("LID", database->getLobbyInfo(LOBBY_ID));
		secret = packet->GetVar("SECRET");	//is this useful for us? lets store this to be sure
		if(!secret.empty())
			sendPacket->SetVar("SECRET", secret);
		else	//server requests a server ID, lets make a static one for now
			sendPacket->SetVar("SECRET", "4l94N6Y0A3Il3+kb55pVfK6xRjc+Z6sGNuztPeNGwN5CMwC7ZlE/lwel07yciyZ5y3bav7whbzHugPm11NfuBg%3d%3d");		// is this required?
		sendPacket->SetVar("J", game.key[JOIN]);
		sendPacket->SetVar("GID", gid);
		queue->push_back(sendPacket);

		if(logDatabase)
			debug->simpleNotification(3, DATABASE, database->gameToString(gid, "added", &game, false).c_str());
	}

	else if(strcmp(type, "UGAM") == 0)
	{
		for(int i = 4; i < GAMES_SIZE; i++)		//the first 4 values are never updated (ip and port stuff) so we never have to check for them
		{
			string value = packet->GetVar(ServerCoreInfo[i]);
			if(!value.empty())
					game.key[i] = value;
		}
		if(logDatabase)
			debug->simpleNotification(3, DATABASE, database->gameToString(gid, "updated", &game, false).c_str());

		state = SKIP;
	}

	else if(strcmp(type, "UBRA") == 0)
	{
		sendPacket = new Packet("UBRA", 0x00000000);
		sendPacket->SetVar("TID", ++sock_tid);
		queue->push_back(sendPacket);
		state = NORMAL;
	}

	else if(strcmp(type, "UGDE") == 0)
	{
		bool updateDescription = false;

		for(int i = 0; i < GDET_SIZE; i++)		//the first 4 values are never updated (ip and port stuff) so we never have to check for them
		{
			string value = packet->GetVar(ServerGdetInfo[i]);
			if(!value.empty())
			{
				game.gdet[i] = value;
				if(i == SERVER_DESCRIPTION)
					updateDescription = true;
			}
		}

		if(updateDescription)		//there are descriptions to get, check how many and write them into database
		{
			int descriptionCount = lexical_cast<int>(game.gdet[SERVER_DESCRIPTION]);
			string description = "";
			for(int i = 0; i < descriptionCount; i++)
			{
				char buffer[24];
				sprintf(buffer, "D-ServerDescription%i", i);
				description.append(packet->GetVar(buffer));
			}
			game.gdet[SERVER_DESCRIPTION] = description;
		}
		if(logDatabase)
			debug->simpleNotification(3, DATABASE, database->gameToString(gid, "updated", &game, true).c_str());

		state = SKIP;
	}

	else if(strcmp(type, "UQUE") == 0)	//->O UQUE 0x40000000 {LID=257, GID=77635, QUEUE=27, TID=508}
	{
		debug->notification(3, connectionType, "Handling UQUE, PID=%s", packet->GetVar("QUEUE").c_str());
		//the queue_length was already updated when player sent QENT (because the queue_info was needed) so what should we do here?
		sendPacket = new Packet("UQUE", 0x00000000);
		sendPacket->SetVar("TID", ++sock_tid);
		queue->push_back(sendPacket);
	}

	else if(strcmp(type, "EGRS") == 0)
	{
		debug->notification(3, connectionType, "Handling EGRS, PID=%s", packet->GetVar("PID").c_str());

		sendPacket = new Packet("EGRS", 0x00000000);
		if(packet->GetVar("ALLOWED").compare("1") == 0)		// update player count on server
		{
			// joining_players++
			int joining_players = lexical_cast<int>(game.key[JOINING_PLAYERS])+1;
			game.key[JOINING_PLAYERS] = lexical_cast<string>(joining_players);
		}
		sendPacket->SetVar("TID", ++sock_tid);
		queue->push_back(sendPacket);
	}

	else if(strcmp(type, "PENT") == 0) // entitle player?
	{
		// joining_players--
		int player_count = lexical_cast<int>(game.key[JOINING_PLAYERS])-1;
		if(player_count >= 0)
			game.key[JOINING_PLAYERS] = lexical_cast<string>(player_count);

		// active_players++
		player_count = lexical_cast<int>(game.key[ACTIVE_PLAYERS])+1;
		game.key[ACTIVE_PLAYERS] = lexical_cast<string>(player_count);

		sendPacket = new Packet("PENT", 0x00000000);
		sendPacket->SetVar("TID", ++sock_tid);
		sendPacket->SetVar("PID", packet->GetVar("PID"));
		queue->push_back(sendPacket);
	}

	else if(strcmp(type, "PLVT") == 0) //remove player?
	{
		sendPacket = new Packet("KICK", 0x00000000);
		sendPacket->SetVar("PID", packet->GetVar("PID"));
		sendPacket->SetVar("LID", packet->GetVar("LID"));
		sendPacket->SetVar("GID", packet->GetVar("GID"));
		queue->push_back(sendPacket);

		// active_players--
		int active_players = lexical_cast<int>(game.key[ACTIVE_PLAYERS])-1;
		if(active_players >= 0)
			game.key[ACTIVE_PLAYERS] = lexical_cast<string>(active_players);

		sendPacket = new Packet("PLVT", 0x00000000);
		sendPacket->SetVar("TID", ++sock_tid);
		queue->push_back(sendPacket);
	}
	else
		state = UNKNOWN;

	return state;
}

int GameServer::addPlayerId()
{
	if(connectionType == GAME_SERVER)
	{
		player_count++;
		return player_count;
	}
	else
		return 0;
}
