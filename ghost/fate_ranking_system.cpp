#include "ghost.h"
#include "socket.h"
#include "util.h"
#include "fate_ranking_system.h"
#include "game_base.h"
#include "gameplayer.h"
#include "bnet.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>


using namespace boost::property_tree;

FRS :: FRS(CGHost *nGHost, uint16_t hostPort, string bindAddress) 
{
	m_FRSClientSocket = NULL;
	m_FRSSocket = NULL;
	m_hostPort = hostPort;
	m_GHost = nGHost;
	m_BindAddress = bindAddress;

	if (!m_FRSSocket)
	{
		m_FRSSocket = new CTCPServer();
		if (m_FRSSocket->Listen(m_BindAddress, m_hostPort))
		{
			CONSOLE_Print( "[FRS] listening for Fate Ranking System on port " + UTIL_ToString( m_hostPort ) );
			m_FRSConnect = true;
		}
		else
		{
			CONSOLE_Print( "[FRS] error listening for Fate Ranking System on port " + UTIL_ToString( m_hostPort ) );
			delete m_FRSSocket;
			m_FRSSocket = NULL;
			m_FRSConnect = false;
		}
	}
	else if( m_FRSSocket->HasError( ) )
	{
		CONSOLE_Print( "[FRS] Fate Ranking System listener error (" + m_FRSSocket->GetErrorString( ) + ")" );
		delete m_FRSSocket;
		m_FRSSocket = NULL;
		m_FRSConnect = false;
	}
}

FRS :: ~FRS( )
{
	delete m_FRSClientSocket;
	delete m_FRSSocket;
}


unsigned int FRS :: SetFD( void *fd, void *send_fd, int *nfds )
{
	unsigned int NumFDs = 0;

	if( m_FRSSocket )
	{
		m_FRSSocket->SetFD( (fd_set *)fd, (fd_set *)send_fd, nfds );
		NumFDs++;
	}

	if (m_FRSClientSocket)
	{
		m_FRSClientSocket->SetFD( (fd_set *)fd, (fd_set *)send_fd, nfds );
		NumFDs++;
	}

	return NumFDs;
}


bool FRS :: Update( void *fd, void *send_fd )
{
	if (!m_FRSConnect)
		return true;
	
	if (m_FRSClientSocket)
	{
		if (m_FRSClientSocket->HasError() || !m_FRSClientSocket->GetConnected())
		{
			delete m_FRSClientSocket;
			m_FRSClientSocket = NULL;
		}
		else
		{
			m_FRSClientSocket->DoRecv((fd_set *)fd);
			string RecvBuffer = *(m_FRSClientSocket->GetBytes());
			if (!RecvBuffer.empty())
			{
				if (RecvBuffer == "GetGames")
				{
					GetGames(send_fd);
				}	
				else if (RecvBuffer == "RefreshBanList")
				{
					RefreshBanList();
				}
			}
			m_FRSClientSocket->ClearRecvBuffer();
		}
	}

	CTCPSocket *NewSocket = m_FRSSocket->Accept((fd_set *)fd);
	if ( NewSocket )
	{
		m_FRSClientSocket = NewSocket;
	}
	return true;
}

void FRS :: GetGames(void* send_fd) {
	ptree gamePt;
	ptree lobbyPt;
	ptree progressArrPt;
	std::stringstream ss;
	if (m_GHost->m_CurrentGame)
	{
		lobbyPt.put("IsAvailable",true);
		lobbyPt.put("PlayerCount",m_GHost->m_CurrentGame->GetNumHumanPlayers());
		lobbyPt.put("SlotSize",m_GHost->m_CurrentGame->GetSlots());
		lobbyPt.put("GameName",m_GHost->m_CurrentGame->GetGameName());
		lobbyPt.put("Owner",m_GHost->m_CurrentGame->GetOwnerName());

		ptree lobbyPlayersPt;
		vector<CGamePlayer *> lobbyPlayers = m_GHost->m_CurrentGame->GetGamePlayers();
		for( vector<CGamePlayer *> :: iterator i = lobbyPlayers.begin(); i != lobbyPlayers.end( ); i++ )
		{
			if( !(*i)->GetLeftMessageSent())
			{	
				ptree lobbyPlPt;
				lobbyPlPt.put("PlayerName",(*i)->GetNameTerminated());
				lobbyPlPt.put("Server",(*i)->GetJoinedRealm());
				lobbyPlPt.put("Ping",(*i)->GetPing(m_GHost->m_LCPings));
				lobbyPlayersPt.push_back(std::make_pair("",lobbyPlPt));
			}
		}

		if (!lobbyPlayersPt.empty()) {
			lobbyPt.add_child("LobbyPlayers",lobbyPlayersPt);
		}
	}
	else
	{
		lobbyPt.put("IsAvailable",false);
	}

	for (unsigned int i = 0; i < m_GHost->m_Games.size( ); i++)
	{
		ptree progressPt;
		progressPt.put("GameNumber",i+1);
		progressPt.put("PlayerCount",m_GHost->m_Games[i]->GetNumHumanPlayers());
		progressPt.put("SlotSize",m_GHost->m_Games[i]->GetStartPlayers());
		progressPt.put("GameName",m_GHost->m_Games[i]->GetGameName());
		progressPt.put("Owner",m_GHost->m_Games[i]->GetOwnerName());
		progressPt.put("GameDuration",m_GHost->m_Games[i]->GetGameDuration());

		ptree gamePlayersPt;
		vector<CGamePlayer *> gamePlayers = m_GHost->m_Games[i]->GetGamePlayers();
		for( vector<CGamePlayer *> :: iterator j = gamePlayers.begin(); j != gamePlayers.end( ); j++ )
		{
			ptree gamePlPt;
			gamePlPt.put("PlayerName",(*j)->GetNameTerminated());
			gamePlPt.put("Server",(*j)->GetJoinedRealm());
			gamePlPt.put("Ping",(*j)->GetPing(m_GHost->m_LCPings));
			gamePlPt.put("IsConnected",!(*j)->GetLeftMessageSent());
			gamePlayersPt.push_back(std::make_pair("",gamePlPt));
		}

		if (!gamePlayersPt.empty()) {
			map<string, string> frsEventList = m_GHost->m_Games[i]->GetFRSEventInfo();
			ptree frsEventListPt; 
			for (map<string, string> ::iterator k = frsEventList.begin(); k != frsEventList.end(); k++)
			{
				frsEventListPt.push_back(std::make_pair("",k->second));
			}

			ptree frsKillsPt;
			map<uint32_t, uint32_t> frsKills = m_GHost->m_Games[i]->GetFRSKills();
			for (map<uint32_t, uint32_t> :: iterator k = frsKills.begin(); k != frsKills.end(); k++)
			{
				ptree killPt;
				killPt.put("PlayerID",k->first);
				killPt.put("Kills",k->second);
				frsKillsPt.push_back(std::make_pair("",killPt));
			}

			ptree frsDeathsPt;
			map<uint32_t, uint32_t> frsDeaths = m_GHost->m_Games[i]->GetFRSDeaths();
			for (map<uint32_t, uint32_t> :: iterator k = frsDeaths.begin(); k != frsDeaths.end(); k++)
			{
				ptree deathPt;
				deathPt.put("PlayerID",k->first);
				deathPt.put("Deaths",k->second);
				frsDeathsPt.push_back(std::make_pair("",deathPt));
			}

			ptree frsAssistsPt;
			map<uint32_t, uint32_t> frsAssists = m_GHost->m_Games[i]->GetFRSAssists();
			for (map<uint32_t, uint32_t> :: iterator k = frsAssists.begin(); k != frsAssists.end(); k++)
			{
				ptree assistPt;
				assistPt.put("PlayerID",k->first);
				assistPt.put("Assists",k->second);
				frsAssistsPt.push_back(std::make_pair("",assistPt));
			}
			progressPt.add_child("FRSEvents", frsEventListPt);
			progressPt.add_child("FRSKills", frsKillsPt);
			progressPt.add_child("FRSDeaths", frsDeathsPt);
			progressPt.add_child("FRSAssists", frsAssistsPt);
			progressPt.add_child("ProgressPlayers", gamePlayersPt);
		}

		progressArrPt.push_back(std::make_pair("", progressPt));
	}
	gamePt.add_child("Lobby",lobbyPt);
	gamePt.add_child("Progress",progressArrPt);
	write_json(ss,gamePt);
	string jsonContent = ss.str();
	uint32_t byteLength = jsonContent.length();
	BYTEARRAY byteLengthArr = UTIL_CreateByteArray(byteLength, 4);
	jsonContent = string(byteLengthArr.begin(), byteLengthArr.end()) + jsonContent;
	m_FRSClientSocket->PutBytes(jsonContent);
	m_FRSClientSocket->DoSend((fd_set *)send_fd);
}

void FRS :: RefreshBanList() {
	for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
	{
		(*i)->RefreshBanList();
	}
}