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
#include "stdincludes.h"
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
 * Message data structures
 */

/**
 * Message Types
 */
enum MsgType
{
	JOINREQ,
	JOINACK,
	PINGREQ,
	PINGACK,
};

/**
 *  Member states
 */
enum MemberState
{
	ALIVE,
	SUSPECT,
	FAIL,
};

/**
 * STRUCT NAME: UpdateMsg
 *
 * DESCRIPTION: Piggybacked update messages. Contains
 * the node's address, state, and timestamp of the recorded
 * state.
 */

struct UpdateMsg
{
	Address nodeAddress;
	MemberState nodeState;
	long timestamp;

	UpdateMsg(Address &, MemberState, long);
	UpdateMsg(Address &);
	UpdateMsg();
	~UpdateMsg();
	UpdateMsg(const UpdateMsg &);				// copy constructor
	UpdateMsg &operator=(const UpdateMsg &);	// copy assignment operator
	UpdateMsg(UpdateMsg &&) noexcept;			// move constructor
	UpdateMsg &operator=(UpdateMsg &) noexcept; // move assignment operator
	bool operator==(const UpdateMsg &) const;   // equality operator
	bool operator!=(const UpdateMsg &) const;   // inequality operator
	int msgSize();
	void serialize(char *, int);
};

/**
 * STRUCT NAME: MessageHdr
 *
 * DESCRIPTION: Header and content of a message. Header contains
 * the message addressees, the type of the message, the timestamp of when
 * it was sent, and the number of attached updates. The content of the
 * message in contained in the array of UpdateMsg objects.
 */
struct MessageHdr
{
	Address srcAddr;
	Address dstAddr;
	MsgType msgType;
	long timestamp;
	int updateCount;
	UpdateMsg *piggyBack;

	MessageHdr(Address &, Address &, MsgType, long, int);
	MessageHdr(char *, int);
	MessageHdr(int);
	MessageHdr();
	~MessageHdr();
	MessageHdr(const MessageHdr &);				  // copy constructor
	MessageHdr &operator=(const MessageHdr &);	  // copy assignment operator
	MessageHdr(MessageHdr &&) noexcept;			  // move constructor
	MessageHdr &operator=(MessageHdr &) noexcept; // move assignment operator
	int msgSize();
	void serialize(char *, int);
};

/**
 * CLASS NAME: UpdateBuffer
 *
 * DESCRIPTION: Maintains an ordered list of updates that
 * need to be piggybacked. This class holds the logic that
 * determines how many and what updates should be piggybacked
 * on a message.
 *
 * Oldest updates are sent first and purged when they have been
 * sent more than the maxSent threshold.
 */
class UpdateBuffer
{
private:
	int maxUpdates;
	int maxSent;
	std::vector<std::pair<UpdateMsg,short>> updateBuffer;

public:
	UpdateBuffer();
	UpdateBuffer(int);
	UpdateBuffer(int, int);
	UpdateBuffer(const UpdateBuffer &);				  // copy constructor
	UpdateBuffer &operator=(const UpdateBuffer &);	  // copy assignment operator
	UpdateBuffer(UpdateBuffer &&) noexcept;			  // move constructor
	UpdateBuffer &operator=(UpdateBuffer &) noexcept; // move assignment operator
	int getUpdateCount();
	void insertUpdate(UpdateMsg);
	void insertUpdatesFromMsg(MessageHdr &);
	bool contains(UpdateMsg &msg);
	UpdateMsg at(int);
	std::vector<UpdateMsg> getAllUpdates();
	void cleanupBuffer();
};

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
	bool recvJoinReq(MessageHdr &);
	bool sendJoinReq(Address &);
	bool recvJoinAck(MessageHdr &);
	bool sendJoinAck(Address &);
	bool recvPingReq(MessageHdr &);
	bool sendPingReq(Address &, Address &);
	bool recvPingAck(MessageHdr &);
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
	void attachUpdates(MessageHdr &, int);
	void loadMemberList(MessageHdr &, std::vector<MemberListEntry>&);
	int isNullAddress(Address &);
	Address getJoinAddress();
	void printAddress(Address &);
	bool isFailed() const;
	void setFailed(bool);
	Address getAddress() const;
};

