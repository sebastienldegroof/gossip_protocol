#pragma once

#include "Address.h"


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
 * STRUCT NAME: Message
 *
 * DESCRIPTION: Header and content of a message. Header contains
 * the message addressees, the type of the message, the timestamp of when
 * it was sent, and the number of attached updates. The content of the
 * message in contained in the array of UpdateMsg objects.
 */
struct Message
{
	Address srcAddr;
	Address dstAddr;
	MsgType msgType;
	long timestamp;
	int updateCount;
	UpdateMsg *piggyBack;

	Message(Address &, Address &, MsgType, long, int);
	Message(char *, int);
	Message(int);
	Message();
	~Message();
	Message(const Message &);				  // copy constructor
	Message &operator=(const Message &);	  // copy assignment operator
	Message(Message &&) noexcept;			  // move constructor
	Message &operator=(Message &) noexcept; // move assignment operator
	int msgSize();
	void serialize(char *, int);
};