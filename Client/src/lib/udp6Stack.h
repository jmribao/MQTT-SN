/*
 * udpStack.h
 *
 *                      The BSD License
 *
 *           Copyright (c) 2014, tomoaki@tomy-tech.com
 *                    All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  Created on: 2014/06/01
 *    Modified: Juan Manuel Fernández Ribao
 *      Author: Tomoaki YAMAGUCHI
 *     Version: 1.0.0
 */

#ifndef UDP6STACK_H_
#define UDP6STACK_H_

#include "MQTTSN_Application.h"
#include "mqUtil.h"
#include "Network.h"

#ifdef NETWORK_UDP6

#include <sys/time.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <string>
#include <arpa/inet.h>


#define STAT_UNICAST   1
#define STAT_MULTICAST 2

#define SOCKET_MAXHOSTNAME  200
#define SOCKET_MAXCONNECTIONS  5
#define SOCKET_MAXRECV  500
#define SOCKET_MAXBUFFER_LENGTH 500 // buffer size

#define PACKET_TIMEOUT_CHECK   200  // msec

using namespace std;

namespace tomyClient {

/*============================================
              NWAddress128
 =============================================*/
class NWAddress128 {
public:
	NWAddress128(uint8_t address[16]);
	NWAddress128(void);
	uint8_t* getAddress(uint8_t address[16]);
	void setAddress(uint8_t address[16]);
	void resetAddress();
	bool isAddress(uint8_t address[16]);
	bool operator==(NWAddress128&);
private:
	uint8_t _address[16];
} __attribute__((__packed__));

/*============================================
               NWResponse
 =============================================*/

class NWResponse {
public:
	NWResponse();
	bool    isAvailable();
	uint8_t  isError();
	uint8_t  getType();
	uint8_t  getFrameLength();
	uint8_t  getPayload(uint8_t index);
	uint8_t* getPayload();
	uint8_t* getBody();
	uint16_t getBodyLength();
	uint8_t  getPayloadLength();
	uint16_t getAddress16();
	NWAddress128& getAddress128();
	#ifdef SCOPE_ID
		uint32_t getScopeId();
	#endif
	void setLength(uint16_t len);
//	void setType(uint8_t type);
	void setFrame(uint8_t* framePtr);
	void setAddress128(uint8_t address[16]);
	#ifdef SCOPE_ID
		void setScopeId(uint32_t scopeId);
	#endif
	void setAddress16(uint16_t portNo);
	void setErrorCode(uint8_t);
	void setAvailable(bool);
	void resetResponse();
private:
	NWAddress128 _addr128;
	#ifdef SCOPE_ID
		uint32_t _scopeId;
	#endif
	uint16_t _addr16;
	uint16_t _len;
	uint8_t* _frameDataPtr;
	uint8_t  _type;
	uint8_t  _errorCode;
	bool    _complete;
};


/*========================================
       Class UpdPort
 =======================================*/
class UdpPort{
public:
	UdpPort();
	virtual ~UdpPort();

	bool open(NETWORK_CONFIG config);

	int unicast(const uint8_t* buf, uint32_t length, uint8_t ipaddress[16],
			#ifdef SCOPE_ID
				uint32_t scopeId,
			#endif
			uint16_t port  );
	int multicast( const uint8_t* buf, uint32_t length );
	int recv(uint8_t* buf, uint16_t len, bool nonblock, uint8_t ipaddress[16],
			#ifdef SCOPE_ID
				uint32_t* scopeId,
			#endif
			uint16_t* port );
	int recv(uint8_t* buf, uint16_t len, int flags);
	bool checkRecvBuf();
	bool isUnicast();

private:
	void close();
	int recvfrom ( uint8_t* buf, uint16_t len, int flags, uint8_t ipaddress[16],
			#ifdef SCOPE_ID
				uint32_t* scopeId,
			#endif
			uint16_t* port );

	int _sockfdUcast;
	int _sockfdMcast;
	uint16_t _gPortNo;
	uint16_t _uPortNo;
	uint8_t _gIpAddr[16];
	uint8_t  _castStat;
	bool   _disconReq;

};

#define NO_ERROR	0
#define PACKET_EXCEEDS_LENGTH  1
/*===========================================
               Class  Network
 ============================================*/
class Network : public UdpPort {
public:
    Network();
    ~Network();

    void send(uint8_t* xmitData, uint8_t dataLen, SendReqType type);
    int  readPacket(uint8_t type = 0);
    void setGwAddress();
    void resetGwAddress(void);
    void setRxHandler(void (*callbackPtr)(NWResponse* data, int* returnCode));
    void setSleep();
    int  initialize(Udp6Config  config);
private:
    int  readApiFrame();

	NWResponse _nlResp;
    uint8_t _gwIpAddress[16];
	#ifdef SCOPE_ID
		uint32_t _scopeId;
	#endif
	uint16_t _gwPortNo;
    int     _returnCode;
    bool _sleepflg;
	void (*_rxCallbackPtr)(NWResponse* data, int* returnCode);
    uint8_t _rxFrameDataBuf[MQTTSN_MAX_FRAME_SIZE];

};


}    /* end of namespace */

#endif
#endif   /*  UDPSTACK_H__  */
