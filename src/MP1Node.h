/**********************************
 * FILE NAME: MP1Node.h
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Header file of MP1Node class. (Revised 2020)
 *
 *  Starter code template
 **********************************/

#pragma once

#include "IMember.h"
#include "Message.h"
#include "UpdateBuffer.h"
#include "Constants.h"
#include "Log.h"
#include "Params.h"
#include "Member.h"
#include "EmulNet.h"
#include "Queue.h"

#include <unordered_set>

/**
 * Macros
 */
#define TREMOVE 20
#define TFAIL 5

/**
 * CLASS NAME: MP1Node
 *
 * DESCRIPTION: Class implementing Membership protocol functionalities for failure detection
 */
class MP1Node: public IMember
{
private:
	EmulNet *emulNet;
	Log *log;
	Params *par;
	Member *memberNode;
	bool shouldDeleteMember;
	char NULLADDR[6];
	long timestamp;
	UpdateBuffer updateBuffer;
	Address lastPing;
	std::vector<std::pair<Address,short>> indirectPings;
	std::vector<std::pair<Address,short>> suspectList;
	std::vector<std::pair<Address,long>> deadList;
	std::vector<std::pair<Address,Address>> forwardedPings;

public:
	MP1Node(Params *params, EmulNet *emul, Log *log, Address *address);
	MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address);
	Member *getMemberNode() { return memberNode; }
	~MP1Node();

	// node operations
	void nodeStart(char *servaddrstr, short serverport);
	int initThisNode(Address &);
	void initMemberListTable(Member *);
	int introduceSelfToGroup(Address &);
	int finishUpThisNode();
	int recvLoop();
	static int enqueueWrapper(void *env, char *buff, int size);
	int nodeLoop();
	void checkMessages();
	void nodeLoopOps();

	// node messaging
	bool recvCallBack(char *, int );
	bool recvJoinReq(Message &);
	bool sendJoinReq(Address &);
	bool recvJoinAck(Message &);
	bool sendJoinAck(Address &);
	bool recvPingReq(Message &);
	bool sendPingReq(Address &, Address &);
	bool recvPingAck(Message &);
	bool sendPingAck(Address &, Address &);

	// node helper functions
	void processUpdate(UpdateMsg&);
	void stayinAlive(Address&);
	bool addMember(Address&);
	bool addMember(Address&, long);
	bool removeMember(Address&);
	bool addIndirect(Address&);
	bool removeIndirect(Address&);
	bool addSuspect(Address&);
	bool removeSuspect(Address&);
	bool addDead(Address&, long);
	bool removeDead(Address&, long);
	bool addForward(Address&, Address&);
	Address removeForward(Address&);
	int getPiggybackCount();
	void attachUpdates(Message &, int);
	void loadMemberList(Message &, std::vector<MemberListEntry>&);
	int isNullAddress(Address &);
	Address getJoinAddress();
	void printAddress(Address &);
	bool isFailed() const;
	void setFailed(bool);
	Address getAddress() const;
};

