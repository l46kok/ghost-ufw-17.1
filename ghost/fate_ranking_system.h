#ifndef FATE_RANKING_SYSTEM_H
#define FATE_RANKING_SYSTEM_H

class FRS
{
public:
	FRS(CGHost *nGHost, uint16_t hostPort, string bindAddress);
	~FRS();
	bool Update( void *fd, void *send_fd );
	unsigned int SetFD( void *fd, void *send_fd, int *nfds );

private:
	CGHost *m_GHost;
	CTCPServer *m_FRSSocket;				// listening socket for Fate Ranking System (ufwfate.net)
	CTCPSocket *m_FRSClientSocket;			// Fate Ranking System socket for connected client
	bool m_FRSConnect;						// config value: flag for FRS connection
	uint16_t m_hostPort;
	string m_BindAddress;
	void GetGames(void *send_fd);
	void RefreshBanList();
};


#endif
