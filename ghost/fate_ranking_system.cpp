#include "ghost.h"
#include "socket.h"
#include "util.h"
#include "fate_ranking_system.h"
#include "game_base.h"
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
				ptree gamePt;
				ptree lobbyPt;
				ptree progressArrPt;
				std::stringstream ss;
				
				if (RecvBuffer == "GetGames")
				{
					if (m_GHost->m_CurrentGame)
					{
						lobbyPt.put("IsAvailable",true);
						lobbyPt.put("PlayerCount",m_GHost->m_CurrentGame->GetNumHumanPlayers());
						lobbyPt.put("SlotSize",m_GHost->m_CurrentGame->GetSlots());
						lobbyPt.put("GameName",m_GHost->m_CurrentGame->GetGameName());
						lobbyPt.put("Owner",m_GHost->m_CurrentGame->GetOwnerName());
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
						progressArrPt.push_back(std::make_pair("", progressPt));
					}
					gamePt.add_child("Lobby",lobbyPt);
					gamePt.add_child("Progress",progressArrPt);
					write_json(ss,gamePt);
					m_FRSClientSocket->PutBytes(ss.str());
					m_FRSClientSocket->DoSend((fd_set *)send_fd);
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