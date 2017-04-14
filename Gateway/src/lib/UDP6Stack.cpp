/*
 * UDPStack.cpp
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
 *     Version: 0.0.0
 */

#include "Defines.h"

#ifdef NETWORK_UDP6

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <string.h>
#include "ProcessFramework.h"
#include "UDP6Stack.h"

using namespace std;
using namespace tomyGateway;

extern uint16_t getUint16(uint8_t* pos);
extern uint32_t getUint32(uint8_t* pos);
extern void setUint16(uint8_t* pos, uint16_t val);
extern void setUint32(uint8_t* pos, uint32_t val);

/*=========================================
       Class Network
 =========================================*/
Network::Network(){
}

Network::~Network(){

}

void Network::unicast(NWAddress128* addr128,
		#ifdef SCOPE_ID
			uint32_t scopeId,
		#endif
		uint16_t addr16, uint8_t* payload, uint16_t payloadLength){
	uint8_t ipAddress[16];
	UDPPort::unicast(payload, payloadLength, addr128->getAddress(ipAddress),
		#ifdef SCOPE_ID
			scopeId,
		#endif
		addr16);
}

void Network::broadcast(uint8_t* payload, uint16_t payloadLength){
	UDPPort::multicast(payload, payloadLength);
}

bool Network::getResponse(NWResponse* response){
	uint8_t ipAddress[16];
	uint32_t scopeId = 0;
	uint16_t portNo = 0;
	uint16_t msgLen;
	uint8_t  msgType;

	uint8_t* buf = response->getPayloadPtr();
	memset(ipAddress, 0, 16*sizeof(uint8_t));
	uint16_t recvLen = UDPPort::recv(buf, MQTTSN_MAX_FRAME_SIZE, ipAddress, &scopeId, &portNo);
	if(recvLen < 0){
		return false;
	}else{
		if(buf[0] == 0x01){
			msgLen = getUint16(buf + 1);
			msgType = *(buf + 3);
		}else{
			msgLen = (uint16_t)*(buf);
			msgType = *(buf + 1);
		}
		if(msgLen != recvLen){
			return false;
		}
		response->setLength(msgLen);
		response->setMsgType(msgType);
		response->setClientAddress16(portNo);
		response->setClientAddress128(ipAddress);
		#ifdef SCOPE_ID
			response->setClientScopeId(scopeId);
		#endif
		return true;
	}
}

int Network::initialize(Udp6Config  config){
	return UDPPort::open(config);
}


/*=========================================
       Class udpStack
 =========================================*/

UDPPort::UDPPort(){
    _disconReq = false;
    _sockfdUnicast = -1;
    _sockfdMulticast = -1;
	_gPortNo = 0;
	memset(_gIpAddr, 0, 16*sizeof(uint8_t));

}

UDPPort::~UDPPort(){
    close();
}

void UDPPort::close(){
	if(_sockfdUnicast > 0){
		::close( _sockfdUnicast);
		_sockfdUnicast = -1;
	}
	if(_sockfdMulticast > 0){
		::close( _sockfdMulticast);
		_sockfdMulticast = -1;
	}
}

int UDPPort::open(Udp6Config config){

	const int loopch = 0;
	const int reuse = 1;
	const int only6 = 1;

	if(config.uPortNo == 0 || config.gPortNo == 0){
		return -1;
	}
	_gPortNo = htons(config.gPortNo);

	if(inet_pton(AF_INET6, config.ipAddress, _gIpAddr) != 1) {
	      D_NWSTACK("error bad IP MULTICAST\n");
	      return -1;
	}

	D_NWSTACK("IPv6 address :%s\n",config.ipAddress);

	/*------ Create unicast socket --------*/
	_sockfdUnicast = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if (_sockfdUnicast < 0){
		return -1;
	}

	if(setsockopt(_sockfdUnicast, IPPROTO_IPV6, IPV6_V6ONLY, &only6, sizeof(only6))) {
		return -1;
	}

	setsockopt(_sockfdUnicast, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

	sockaddr_in6 addru;
	memset(&addru, 0, sizeof(addru));
	addru.sin6_family = AF_INET6;
	addru.sin6_port = htons(config.uPortNo);
	addru.sin6_addr   = in6addr_any;

	if( ::bind ( _sockfdUnicast, (sockaddr*)&addru,  sizeof(addru)) <0){
		return -1;
	}

	if(setsockopt(_sockfdUnicast, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loopch, sizeof(loopch)) <0 ){
		D_NWSTACK("error IPV6_MULTICAST_LOOP in UDPPort::open\n");
		close();
		return -1;
	}

	/*------ Create Multicast socket --------*/
	_sockfdMulticast = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if (_sockfdMulticast < 0){
		close();
		return -1;
	}

	if(setsockopt(_sockfdMulticast, IPPROTO_IPV6, IPV6_V6ONLY, &only6, sizeof(only6))) {
		return -1;
	}

	setsockopt(_sockfdMulticast, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

	sockaddr_in6 addrm;
	memset(&addrm, 0, sizeof(addru));
	addrm.sin6_family = AF_INET6;
	addrm.sin6_port = _gPortNo;
	addrm.sin6_addr   = in6addr_any;

	if( ::bind ( _sockfdMulticast, (sockaddr*)&addrm,  sizeof(addrm)) <0){
		return -1;
	}

	if(setsockopt(_sockfdMulticast, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loopch, sizeof(loopch)) <0 ){
		D_NWSTACK("error IPV6_MULTICAST_LOOP in UDPPort::open\n");
		close();
		return -1;
	}


	ipv6_mreq mreq;

	/* Accept multicast from any interface */
	mreq.ipv6mr_interface = 0;

	/* Specify the multicast group */
	memcpy(&mreq.ipv6mr_multiaddr,
		   _gIpAddr,
		   sizeof(mreq.ipv6mr_multiaddr));

	if( setsockopt(_sockfdMulticast, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mreq, sizeof(mreq))< 0){
		D_NWSTACK("error Multicast IPV6_ADD_MEMBERSHIP in UDPPort::open\n");
		perror("multicast");
		close();
		return -1;
	}

	if( setsockopt(_sockfdUnicast, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mreq, sizeof(mreq))< 0){
		D_NWSTACK("error Unicast IPV6_ADD_MEMBERSHIP in UDPPort::open\n");
		close();
		return -1;
	}
	return 0;
}


int UDPPort::unicast(const uint8_t* buf, uint32_t length, uint8_t ipaddress[16],
		#ifdef SCOPE_ID
			uint32_t scopeId,
		#endif
		uint16_t port  ){
	sockaddr_in6 dest;
	dest.sin6_family = AF_INET6;
	dest.sin6_port = port;
	#ifdef SCOPE_ID
		dest.sin6_scope_id = scopeId;
	#endif
	memcpy(&dest.sin6_addr,
			   ipaddress,
               sizeof(dest.sin6_addr));

	int status = ::sendto( _sockfdUnicast, buf, length, 0, (const sockaddr*)&dest, sizeof(dest) );
	if( status < 0){
		D_NWSTACK("errno == %d in UDP6Port::sendto\n", errno);
	}
	char straddr[INET6_ADDRSTRLEN];
	D_NWSTACK("sendto %s/%d:%u length = %d\n",
			inet_ntop(AF_INET6, ipaddress, straddr, sizeof(straddr)),
			#ifdef SCOPE_ID
				scopeId,
			#else
				0,
			#endif
			htons(port), status);
	return status;
}

int UDPPort::multicast( const uint8_t* buf, uint32_t length ){
	return unicast(buf, length,_gIpAddr, 0, _gPortNo);
}

int UDPPort::recv(uint8_t* buf, uint16_t len, uint8_t ipaddress[16], uint32_t* scopeIdPtr, uint16_t* portPtr){
	fd_set recvfds;
	int maxSock = 0;

	FD_ZERO(&recvfds);
	FD_SET(_sockfdUnicast, &recvfds);
	FD_SET(_sockfdMulticast, &recvfds);

	if(_sockfdMulticast > _sockfdUnicast){
		maxSock = _sockfdMulticast;
	}else{
		maxSock = _sockfdUnicast;
	}

	select(maxSock + 1, &recvfds, 0, 0, 0);

	if(FD_ISSET(_sockfdUnicast, &recvfds)){
		return recvfrom (_sockfdUnicast,buf, len, 0,ipaddress, scopeIdPtr, portPtr );
	}else if(FD_ISSET(_sockfdMulticast, &recvfds)){
		return recvfrom (_sockfdMulticast,buf, len, 0,ipaddress, scopeIdPtr, portPtr );
	}
	return 0;
}

int UDPPort::recvfrom (int sockfd, uint8_t* buf, uint16_t len, uint8_t flags, uint8_t ipaddress[16], uint32_t* scopeIdPtr, uint16_t* portPtr ){
	sockaddr_in6 sender;
	socklen_t addrlen = sizeof(sender);
	memset(&sender, 0, addrlen);

	int status = ::recvfrom( sockfd, buf, len, flags, (sockaddr*)&sender, &addrlen );

	if ( status < 0 && errno != EAGAIN )	{
		D_NWSTACK("errno == %d in UDP6Port::recvfrom\n", errno);
		return -1;
	}

	memcpy(ipaddress,
               &sender.sin6_addr,
               sizeof(sender.sin6_addr));
	#ifdef SCOPE_ID
		*scopeIdPtr = sender.sin6_scope_id;
	#endif
	*portPtr = (uint16_t)sender.sin6_port;

	char straddr[INET6_ADDRSTRLEN];

	D_NWSTACK("recved from %s/%d:%d length = %d\n",
		  inet_ntop(AF_INET6, ipaddress, straddr, sizeof(straddr)),
		  #ifdef SCOPE_ID
		  	  *scopeIdPtr,
		  #else
			  0,
		  #endif
		  htons(*portPtr),status);

	return status;
}


/*=========================================
             Class NLLongAddress
 =========================================*/
NWAddress128::NWAddress128(){
	memset(_address, 0, 16*sizeof(uint8_t));
}

NWAddress128::NWAddress128(uint8_t address[16]){
	memcpy(_address, address, 16*sizeof(uint8_t));
}

uint8_t* NWAddress128::getAddress(uint8_t address[16]){
    return (uint8_t*)memcpy(address, _address, 16*sizeof(uint8_t));
}

void NWAddress128::setAddress(uint8_t address[16]){
	memcpy(_address, address, 16*sizeof(uint8_t));
}

bool NWAddress128::operator==(NWAddress128& addr){
	return memcmp(_address, addr._address, 16*sizeof(uint8_t))==0;
}

/*=========================================
             Class NWResponse
 =========================================*/
NWResponse::NWResponse(){
    _addr16 = 0;
    memset( _frameDataPtr, 0, MQTTSN_MAX_FRAME_SIZE);
}

uint8_t  NWResponse::getFrameLength(){
	return _len;
}

void NWResponse::setLength(uint16_t len){
	_len = len;
}

NWAddress128*  NWResponse::getClientAddress128(){
    return &_addr128;
}

uint32_t  NWResponse::getClientScopeId(){
    return _scopeId;
}

uint16_t NWResponse::getClientAddress16(){
  return _addr16;
}

void  NWResponse::setClientAddress128(uint8_t address[16]){
    _addr128.setAddress(address);
}

#ifdef SCOPE_ID
	void  NWResponse::setClientScopeId(uint32_t scopeId){
		_scopeId=scopeId;
	}
#endif

void  NWResponse::setClientAddress16(uint16_t addr16){
	_addr16 = addr16;
}

void NWResponse::setMsgType(uint8_t type){
	_type = type;
}


uint8_t NWResponse::getMsgType(){
	if(_len > 255){
		return _frameDataPtr[3];
	}else{
		return _frameDataPtr[1];
	}
}

uint8_t* NWResponse::getBody(){
	if(_len > 255){
		return _frameDataPtr + 4;
	}else{
		return _frameDataPtr + 2;
	}
}

uint16_t NWResponse::getBodyLength(){
	if(_len > 255){
		return getPayloadLength() - 4;
	}else{
		return getPayloadLength() - 2;
	}
}

uint8_t NWResponse::getPayload(uint8_t index){
		return _frameDataPtr[index + 2];

}

uint8_t* NWResponse::getPayloadPtr(){

		return _frameDataPtr;

}

uint8_t NWResponse::getPayloadLength(){

	return _len;
}

#endif /* NETWORK_UDP6 */
