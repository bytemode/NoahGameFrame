#include "NFCMasterNet_WebServerModule.h"
#include "NFComm/NFMessageDefine/NFProtocolDefine.hpp"
NFIPluginManager* NFCMasterNet_WebServerModule::s_pPluginManager = nullptr;
bool NFCMasterNet_WebServerModule::Init()
{
	return true;
}
bool NFCMasterNet_WebServerModule::BeforeShut()
{
	return true;
}
bool NFCMasterNet_WebServerModule::Shut()
{
	// cleanup
	_mwCloseAllConnections(hp);
	SYSLOG(LOG_INFO, "Cleaning up...\n");
	for (i = 0; i < hp->maxClients; i++) {
		if (hp->hsSocketQueue[i].buffer) free(hp->hsSocketQueue[i].buffer);
	}
	for (i = 0; hp->pxUrlHandler[i].pchUrlPrefix; i++) {
		if (hp->pxUrlHandler[i].pfnUrlHandler && hp->pxUrlHandler[i].pfnEventHandler)
			hp->pxUrlHandler[i].pfnEventHandler(MW_UNINIT, &hp->pxUrlHandler[i], hp);
	}
	free(hp->hsSocketQueue);
	hp->hsSocketQueue = 0;

	// clear state vars
	hp->bKillWebserver = FALSE;
	hp->bWebserverRunning = FALSE;
	mwServerShutdown(&httpParam);
	UninitSocket();
	return true;
}

int NFCMasterNet_WebServerModule::GetMasterJson(UrlHandlerParam * param)
{
	NFIMasterNet_ServerModule* mMasterServerModule = s_pPluginManager->FindModule<NFIMasterNet_ServerModule>();
	std::string str = mMasterServerModule->GetServersStatus();

	char *p;
	p = param->pucBuffer;
	HttpStats *stats = &((HttpParam*)param->hp)->stats;
	HttpRequest *req = &param->hs->request;
	std::string strUrl(req->pucPath);
	IPADDR ip = param->hs->ipAddr;
	int bufsize = param->dataBytes;

	auto findCmd = SplitString(strUrl, "/");
	if (findCmd.size() != 2)
	{
		int len = snprintf(p, bufsize, "{\"code\":1,\"errMsg\":\"Url Cannot be recongnized!\"}", 1, 0, "utf-8");
		p += len;
		bufsize -= len;
		//return data to server
		param->dataBytes = (int)p - (int)(param->pucBuffer);
		param->fileType = HTTPFILETYPE_JS;
		return FLAG_DATA_RAW;
	}

	std::string strCmd = findCmd[1];
	std::map<string, int> CmdMap;
	CmdMap.insert(std::pair<std::string, int>("dashboard", 1));

	std::map<string, int>::iterator it = CmdMap.find(strCmd);
	if (it != CmdMap.end())
	{
		switch (CmdMap[strCmd])
		{
		case 1:
		{
			int ret = FLAG_DATA_RAW;

			if (stats->clientCount > 4) {
				param->pucBuffer = (char*)malloc(stats->clientCount * 256 + 1024);
				ret = FLAG_DATA_RAW | FLAG_TO_FREE;
			}

			p = param->pucBuffer;

			int len = snprintf(p, bufsize, str.c_str(), 1, 0, "utf-8");
			p += len;
			bufsize -= len;
			//return data to server
			param->dataBytes = (int)p - (int)(param->pucBuffer);
			param->fileType = HTTPFILETYPE_JS;
			return ret;
		}
		break;
		default:
			break;
		}

	}
	else {
		std::string errMsg = "{\"code\":1,\"errMsg\":\"Cannot find the command:" + strCmd + "!\"}";
		int len = snprintf(p, bufsize, errMsg.c_str(), 1, 0, "utf-8");
		p += len;
		bufsize -= len;
		//return data to server
		param->dataBytes = (int)p - (int)(param->pucBuffer);
		param->fileType = HTTPFILETYPE_JS;
		return FLAG_DATA_RAW;
	}
}

char * NFCMasterNet_WebServerModule::GetLocalAddrString()
{
	return "127.0.0.1";
}

void NFCMasterNet_WebServerModule::GetFullPath(char * buffer, const char * path)
{
	strcpy(buffer, path);
}

std::vector<std::string> NFCMasterNet_WebServerModule::SplitString(const std::string & str, std::string delim)
{
	std::vector<std::string> result;
	if (str.empty() || delim.empty())
	{
		return result;
	}

	std::string tmp;
	size_t pos_begin = str.find_first_not_of(delim);
	size_t pos = 0;
	while (pos_begin != std::string::npos)
	{
		pos = str.find(delim, pos_begin);
		if (pos != std::string::npos)
		{
			tmp = str.substr(pos_begin, pos - pos_begin);
			pos_begin = pos + delim.length();
		}
		else
		{
			tmp = str.substr(pos_begin);
			pos_begin = pos;
		}

		if (!tmp.empty())
		{
			result.push_back(tmp);
			tmp.clear();
		}
	}
	return result;
}

bool NFCMasterNet_WebServerModule::AfterInit()
{
	mKernelModule = pPluginManager->FindModule<NFIKernelModule>();
	m_pLogicClassModule = pPluginManager->FindModule<NFIClassModule>();
	m_pElementModule = pPluginManager->FindModule<NFIElementModule>();

	int nWebServerAppID = 0;
	NF_SHARE_PTR<NFIClass> xLogicClass = m_pLogicClassModule->GetElement(NFrame::HttpServer::ThisName());
	if (xLogicClass)
	{
		NFList<std::string>& strIdList = xLogicClass->GetIdList();
		std::string strId;
		for (bool bRet = strIdList.First(strId); bRet; bRet = strIdList.Next(strId))
		{
			nWebServerAppID = m_pElementModule->GetPropertyInt(strId, NFrame::HttpServer::ServerID());
			nWebPort = m_pElementModule->GetPropertyInt(strId, NFrame::HttpServer::WebPort());
			strWebRootPath = m_pElementModule->GetPropertyString(strId, NFrame::HttpServer::WebRootPath());
		}
	}

	if (pPluginManager->GetAppID() != nWebServerAppID)
	{
		return true;
	}

	//fill in default settings
	mwInitParam(&httpParam);
	httpParam.maxClients = 50;
	GetFullPath(httpParam.pchWebPath, strWebRootPath.c_str());
	httpParam.httpPort = nWebPort;

	httpParam.pxAuthHandler = authHandlerList;
	httpParam.pxUrlHandler = urlHandlerList;
	httpParam.flags = FLAG_DIR_LISTING;
	httpParam.tmSocketExpireTime = 15;
	{
		int i;
		int error = 0;
		for (i = 0; urlHandlerList[i].pchUrlPrefix; i++) {
			if (urlHandlerList[i].pfnEventHandler) {
				if (urlHandlerList[i].pfnEventHandler(MW_PARSE_ARGS, (void*)urlHandlerList[i].pfnEventHandler, &httpParam))
					error++;
			}
		}
		if (error > 0) {
			printf("Error parsing command line options\n");
			return -1;
		}
	}

	InitSocket();

	{
		int n;
		printf("Host: %s:%d\n", GetLocalAddrString(), httpParam.httpPort);
		printf("Web root: %s\n", httpParam.pchWebPath);
		printf("Max clients (per IP): %d (%d)\n", httpParam.maxClients, httpParam.maxClientsPerIP);
		for (n = 0; urlHandlerList[n].pchUrlPrefix; n++);
		printf("URL handlers: %d\n", n);
		if (httpParam.flags & FLAG_DIR_LISTING) printf("Dir listing enabled\n");
		if (httpParam.flags & FLAG_DISABLE_RANGE) printf("Byte-range disabled\n");

		//register page variable substitution callback
		//httpParam[i].pfnSubst=DefaultWebSubstCallback;
		//start server
		if (mwServerStart(&httpParam)) {
			printf("Error starting HTTP server\n");
		}
	}
	return true;
}

bool NFCMasterNet_WebServerModule::Execute()
{
	// main processing loop
	if (!hp->bKillWebserver) {
		time_t tmCurrentTime;
		SOCKET iSelectMaxFds;
		fd_set fdsSelectRead;
		fd_set fdsSelectWrite;

		// clear descriptor sets
		FD_ZERO(&fdsSelectRead);
		FD_ZERO(&fdsSelectWrite);
		FD_SET(hp->listenSocket, &fdsSelectRead);
		iSelectMaxFds = hp->listenSocket;

		// get current time
#ifndef WINCE
		tmCurrentTime = time(NULL);
#else
		tmCurrentTime = GetTickCount() >> 10;
#endif
		// build descriptor sets and close timed out sockets
		for (int i = 0; i < hp->maxClients; i++) {
			phsSocketCur = hp->hsSocketQueue + i;

			// get socket fd
			socket = phsSocketCur->socket;
			if (!socket) continue;

			{
				int iError = 0;
				int iOptSize = sizeof(int);
				if (getsockopt(socket, SOL_SOCKET, SO_ERROR, (char*)&iError, (socklen_t*)&iOptSize)) {
					// if a socket contains a error, close it
					SYSLOG(LOG_INFO, "[%d] Socket no longer vaild.\n", socket);
					phsSocketCur->flags = FLAG_CONN_CLOSE;
					_mwCloseSocket(hp, phsSocketCur);
					continue;
				}
			}
			// check expiration timer (for non-listening, in-use sockets)
			if (tmCurrentTime > phsSocketCur->tmExpirationTime) {
				SYSLOG(LOG_INFO, "[%d] Http socket expired\n", phsSocketCur->socket);
				hp->stats.timeOutCount++;
				// close connection
				phsSocketCur->flags = FLAG_CONN_CLOSE;
				_mwCloseSocket(hp, phsSocketCur);
			}
			else {
				if (phsSocketCur->dwResumeTick) {
					// suspended
					if (phsSocketCur->dwResumeTick > GetTickCount())
						continue;
					else
						phsSocketCur->dwResumeTick = 0;
				}
				if (ISFLAGSET(phsSocketCur, FLAG_RECEIVING)) {
					// add to read descriptor set
					FD_SET(socket, &fdsSelectRead);
				}
				if (ISFLAGSET(phsSocketCur, FLAG_SENDING)) {
					// add to write descriptor set
					FD_SET(socket, &fdsSelectWrite);
				}
				// check if new max socket
				if (socket > iSelectMaxFds) {
					iSelectMaxFds = socket;
				}
			}
		}

		{
			struct timeval tvSelectWait;
			// initialize select delay
			tvSelectWait.tv_sec = 1;
			tvSelectWait.tv_usec = 0; // note: using timeval here -> usec not nsec

									  // and check sockets (may take a while!)
			iRc = select(iSelectMaxFds + 1, &fdsSelectRead, &fdsSelectWrite,
				NULL, &tvSelectWait);
		}
		if (iRc < 0) {
			msleep(1000);
			return true;
		}
		if (iRc > 0) {
			// check which sockets are read/write able
			for (i = 0; i < hp->maxClients; i++) {
				BOOL bRead;
				BOOL bWrite;

				phsSocketCur = hp->hsSocketQueue + i;

				// get socket fd
				socket = phsSocketCur->socket;
				if (!socket) continue;

				// get read/write status for socket
				bRead = FD_ISSET(socket, &fdsSelectRead);
				bWrite = FD_ISSET(socket, &fdsSelectWrite);

				if ((bRead | bWrite) != 0) {
					//DBG("socket %d bWrite=%d, bRead=%d\n",phsSocketCur->socket,bWrite,bRead);
					// if readable or writeable then process
					if (bWrite && ISFLAGSET(phsSocketCur, FLAG_SENDING)) {
						iRc = _mwProcessWriteSocket(hp, phsSocketCur);
					}
					else if (bRead && ISFLAGSET(phsSocketCur, FLAG_RECEIVING)) {
						iRc = _mwProcessReadSocket(hp, phsSocketCur);
					}
					else {
						iRc = -1;
						DBG("Invalid socket state (flag: %08x)\n", phsSocketCur->flags);
					}
					if (!iRc) {
						// and reset expiration timer
#ifndef WINCE
						phsSocketCur->tmExpirationTime = time(NULL) + hp->tmSocketExpireTime;
#else
						phsSocketCur->tmExpirationTime = (GetTickCount() >> 10) + hp->tmSocketExpireTime;
#endif
					}
					else {
						SETFLAG(phsSocketCur, FLAG_CONN_CLOSE);
						_mwCloseSocket(hp, phsSocketCur);
					}
				}
			}

			// check if any socket to accept and accept the socket
			if (FD_ISSET(hp->listenSocket, &fdsSelectRead)) {
				// find empty slot
				phsSocketCur = 0;
				for (i = 0; i < hp->maxClients; i++) {
					if (hp->hsSocketQueue[i].socket == 0) {
						phsSocketCur = hp->hsSocketQueue + i;
						break;
					}
				}

				if (!phsSocketCur) {
					DBG("WARNING: clientCount:%d > maxClients:%d\n", hp->stats.clientCount, hp->maxClients);
					_mwDenySocket(hp, &sinaddr);
					return true;
				}

				phsSocketCur->socket = _mwAcceptSocket(hp, &sinaddr);
				if (phsSocketCur->socket == 0) return true;
				phsSocketCur->ipAddr.laddr = ntohl(sinaddr.sin_addr.s_addr);
				//SYSLOG(LOG_INFO, "[%d] IP: %d.%d.%d.%d\n", phsSocketCur->socket, phsSocketCur->ipAddr.caddr[3], phsSocketCur->ipAddr.caddr[2], phsSocketCur->ipAddr.caddr[1],phsSocketCur->ipAddr.caddr[0]);

				hp->stats.clientCount++;

				//fill structure with data
				_mwInitSocketData(phsSocketCur);
				phsSocketCur->request.pucPayload = 0;
#ifndef WINCE
				phsSocketCur->tmAcceptTime = time(NULL);
#else
				phsSocketCur->tmAcceptTime = GetTickCount() >> 10;
#endif
				phsSocketCur->tmExpirationTime = phsSocketCur->tmAcceptTime + hp->tmSocketExpireTime;
				phsSocketCur->iRequestCount = 0;
				DBG("Connected clients: %d\n", hp->stats.clientCount);

				//update max client count
				if (hp->stats.clientCount > hp->stats.clientCountMax) hp->stats.clientCountMax = hp->stats.clientCount;
			}
		}
		else {
			//DBG("Select Timeout\n");
			// select timeout
			// call idle event
			if (hp->pfnIdleCallback) {
				(*hp->pfnIdleCallback)(hp);
			}
		}
	}


	return true;
}
