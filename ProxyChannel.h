//////////////////////////////////////////////////////////////////
//
// ProxyChannel.h
//
// Copyright (c) Citron Network Inc. 2001-2002
//
// This work is published under the GNU Public License (GPL)
// see file COPYING for details.
// We also explicitely grant the right to link this code
// with the OpenH323 library.
//
// initial author: Chin-Wei Huang <cwhuang@linux.org.tw>
// initial version: 12/7/2001
//
//////////////////////////////////////////////////////////////////

#ifndef __proxychannel_h__
#define __proxychannel_h__

#ifdef P_SOLARIS
#define map stl_map
#endif

#include <map>
#include "RasTbl.h"
#include "ProxyThread.h"

class H245Handler;
class CallSignalSocket;
class H245Socket;
class UDPProxySocket;
class T120ProxySocket;
class LogicalChannel;
class RTPLogicalChannel;
class T120LogicalChannel;
class H245ProxyHandler;

class H245_RequestMessage;
class H245_ResponseMessage;
class H245_CommandMessage;
class H245_IndicationMessage;
class H245_H2250LogicalChannelParameters;
class H245_OpenLogicalChannel;
class H245_OpenLogicalChannelAck;
class H245_OpenLogicalChannelReject;
class H245_CloseLogicalChannel;

class H245Handler {
// This class handles H.245 messages which can either be transmitted on their
// own TCP connection or can be tunneled in the Q.931 connection
public:
	H245Handler(PIPSocket::Address l) : localAddr(l) {}
	virtual ~H245Handler() {}

	virtual void OnH245Address(H225_TransportAddress &) {}
	virtual bool HandleMesg(PPER_Stream &);
	virtual bool HandleFastStartSetup(H245_OpenLogicalChannel &);
	virtual bool HandleFastStartResponse(H245_OpenLogicalChannel &);
	typedef bool (H245Handler::*pMem)(H245_OpenLogicalChannel &);

	PIPSocket::Address GetLocalAddr() const { return localAddr; }
	void SetLocalAddr(PIPSocket::Address local) { localAddr = local; }

protected:
	virtual bool HandleRequest(H245_RequestMessage &);
	virtual bool HandleResponse(H245_ResponseMessage &);
	virtual bool HandleCommand(H245_CommandMessage &);
	virtual bool HandleIndication(H245_IndicationMessage &);

private:
	PIPSocket::Address localAddr;
};

class H245ProxyHandler : public H245Handler {
public:
	typedef std::map<WORD, LogicalChannel *>::iterator iterator;
	typedef std::map<WORD, LogicalChannel *>::const_iterator const_iterator;
	typedef std::map<WORD, RTPLogicalChannel *>::iterator siterator;
	typedef std::map<WORD, RTPLogicalChannel *>::const_iterator const_siterator;

	H245ProxyHandler(CallSignalSocket *, PIPSocket::Address, H245ProxyHandler * = 0);
	virtual ~H245ProxyHandler();

	// override from class H245Handler
	virtual bool HandleFastStartSetup(H245_OpenLogicalChannel &);
	virtual bool HandleFastStartResponse(H245_OpenLogicalChannel &);

	LogicalChannel *FindLogicalChannel(WORD);
	RTPLogicalChannel *FindRTPLogicalChannelBySessionID(WORD);
	
private:
	// override from class H245Handler
	virtual bool HandleRequest(H245_RequestMessage &);
	virtual bool HandleResponse(H245_ResponseMessage &);

	bool OnLogicalChannelParameters(H245_H2250LogicalChannelParameters *, WORD);
	bool HandleOpenLogicalChannel(H245_OpenLogicalChannel &);
	bool HandleOpenLogicalChannelAck(H245_OpenLogicalChannelAck &);
	bool HandleOpenLogicalChannelReject(H245_OpenLogicalChannelReject &);
	bool HandleCloseLogicalChannel(H245_CloseLogicalChannel &);

	RTPLogicalChannel *CreateRTPLogicalChannel(WORD, WORD);
	RTPLogicalChannel *CreateFastStartLogicalChannel(WORD);
	T120LogicalChannel *CreateT120LogicalChannel(WORD);
	void RemoveLogicalChannel(WORD flcn);

	ProxyHandleThread *handler;
	std::map<WORD, LogicalChannel *> logicalChannels;
	std::map<WORD, RTPLogicalChannel *> sessionIDs;
	std::map<WORD, RTPLogicalChannel *> fastStartLCs;
	H245ProxyHandler *peer;
};

class NATHandler : public H245Handler {
public:
	NATHandler(PIPSocket::Address, PIPSocket::Address);

	// override from class H245Handler
	virtual void OnH245Address(H225_TransportAddress &);
	virtual bool HandleFastStartSetup(H245_OpenLogicalChannel &);
	virtual bool HandleFastStartResponse(H245_OpenLogicalChannel &);

private:
	// override from class H245Handler
	virtual bool HandleRequest(H245_RequestMessage &);
	virtual bool HandleResponse(H245_ResponseMessage &);

	bool HandleOpenLogicalChannel(H245_OpenLogicalChannel &);
	bool HandleOpenLogicalChannelAck(H245_OpenLogicalChannelAck &);

	PIPSocket::Address remoteAddr;
};

class CallSignalSocket : public TCPProxySocket {
public:
	PCLASSINFO ( CallSignalSocket, TCPProxySocket )

	CallSignalSocket();
	CallSignalSocket(CallSignalSocket *, WORD);
	~CallSignalSocket();

	// override from class ProxySocket
        virtual Result ReceiveData();
	virtual bool EndSession();

	// override from class TCPProxySocket
	virtual TCPProxySocket *ConnectTo();

	bool HandleH245Mesg(PPER_Stream &);
	void OnH245ChannelClosed() { m_h245socket = 0; }

protected:
	void OnSetup(H225_Setup_UUIE &);
	void OnCallProceeding(H225_CallProceeding_UUIE &);
	void OnConnect(H225_Connect_UUIE &);
	void OnAlerting(H225_Alerting_UUIE &);
	void OnInformation(H225_Information_UUIE &);
	void OnReleaseComplete(H225_ReleaseComplete_UUIE &);
	void OnFacility(H225_Facility_UUIE &);
	void OnProgress(H225_Progress_UUIE &);
	void OnEmpty(H225_H323_UU_PDU_h323_message_body &);
	void OnStatus(H225_Status_UUIE &);
	void OnStatusInquiry(H225_StatusInquiry_UUIE &);
	void OnSetupAcknowledge(H225_SetupAcknowledge_UUIE &);
	void OnNotify(H225_Notify_UUIE &);
	void OnNonStandardData(PASN_OctetString &);
	void OnTunneledH245(H225_ArrayOf_PASN_OctetString &);
	void OnFastStart(H225_ArrayOf_PASN_OctetString &, bool);

	template<class UUIE> void HandleH245Address(UUIE & uu)
	{
		if (m_h245handler && uu.HasOptionalField(UUIE::e_h245Address))
			if (!SetH245Address(uu.m_h245Address))
				uu.RemoveOptionalField(UUIE::e_h245Address);
	}
	template<class UUIE> void HandleFastStart(UUIE & uu, bool fromCaller)
	{
		if (m_h245handler && uu.HasOptionalField(UUIE::e_fastStart))
			OnFastStart(uu.m_fastStart, fromCaller);
	}

	// localAddr is NOT the local address the socket bind to,
	// but the local address that remote socket bind to
	// they may be different in multi-homed environment
	Address localAddr, peerAddr;
	WORD peerPort;

private:
	void BuildReleasePDU(Q931 &) const;
	bool SetH245Address(H225_TransportAddress &);
	// the method is only valid within ReceiveData()
	Q931 *GetReceivedQ931() const;

	callptr m_call;
	WORD m_crv;
	H245Handler *m_h245handler;
	H245Socket *m_h245socket;
	bool m_h245Tunneling;
	Q931 *m_receivedQ931;
};

inline bool CallSignalSocket::HandleH245Mesg(PPER_Stream & strm)
{
	return m_h245handler->HandleMesg(strm);
}

inline Q931 *CallSignalSocket::GetReceivedQ931() const
{
	return m_receivedQ931;
}

#endif // __proxychannel_h__

