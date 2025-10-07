/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions. (Revised 2020)
 *
 *  Starter code template
 **********************************/

#define HEADER_SIZE 10  // Number of updates piggybacked on a message
#define REPEAT_UPDATE 3 // Number of times an update will be piggybacked
#define DEAD_COUNTER 1  // Number of time periods before declaring a node dead
#define K_NEIGHBORS 3   // Number of other nodes to send indirect PINGREQ to

#include <sstream>
#include "MP1Node.h"

/**
 * CLASS NAME: MP1Node
 */

/**
 * MP1Node overloaded constructors
 */
MP1Node::MP1Node(Params *params, EmulNet *emul, Log *log, Address *address)
{
    for (int i = 0; i < 6; i++)
    {
        NULLADDR[i] = 0;
    }
    this->memberNode = new Member;
    this->shouldDeleteMember = true;
    memberNode->inited = false;
    this->emulNet = emul;
    this->log = log;
    this->par = params;
    this->memberNode->addr = *address;
    this->timestamp = 0;
    this->updateBuffer = UpdateBuffer(par->MAX_NNB);
}
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address)
{
    for (int i = 0; i < 6; i++)
    {
        NULLADDR[i] = 0;
    }
    this->memberNode = member;
    this->shouldDeleteMember = false;
    this->emulNet = emul;
    this->log = log;
    this->par = params;
    this->memberNode->addr = *address;
    this->timestamp = 0;
    this->updateBuffer = UpdateBuffer(par->MAX_NNB);
}

/**
 * MP1Node destructor
 */
MP1Node::~MP1Node()
{
    if (shouldDeleteMember)
    {
        delete this->memberNode;
    }
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * All initializations routines for a member.
 * Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport)
{
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if (initThisNode(joinaddr) == -1)
    {
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
        exit(1);
    }

    if (!introduceSelfToGroup(joinaddr))
    {
        finishUpThisNode();
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address &joinaddr)
{
    memberNode->bFailed = false;
    memberNode->inited = true;
    memberNode->inGroup = false;
    memberNode->timeOutCounter = DEAD_COUNTER;
    initMemberListTable(memberNode);
    // node is up!

    // not super sure what to do with these fields yet...
    // Maybe I dont need them???
    // the coupling between a Member object and MP1Node is weird...
    memberNode->nnb = 0;
    memberNode->heartbeat = 0;
    memberNode->pingCounter = TFAIL;

    return 0;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode)
{
    memberNode->memberList.clear();
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address &joinaddr)
{
    if (memberNode->addr == joinaddr)
    {
        // I am the group booter (first process to join the group). Boot up the group
        memberNode->inGroup = true;
    }
    else
    {
        // Send JOINREQ to introducer of group
        sendJoinReq(joinaddr);
    }
    return 1;
}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode()
{
    timestamp++; // sending message, so increment
    long updateTimestamp = timestamp + (long)memberNode->memberList.size();
    // Send pingack to all nodes containing a FAIL update about myself
    for (int i{0}; i < (int)memberNode->memberList.size(); i++)
    {
        MemberListEntry *mle{&memberNode->memberList.at(i)};
        Address destAddr{Address()};
        *(int *)&destAddr.addr = mle->id;
        *(short *)&destAddr.addr[4] = mle->port;
        if (destAddr == memberNode->addr)
        {
            continue;
        }

        MessageHdr msg{MessageHdr(memberNode->addr, destAddr, JOINACK, timestamp, 1)};
        msg.piggyBack[0] = UpdateMsg(memberNode->addr, FAIL, updateTimestamp);
        int msgSize{msg.msgSize()};
        char *msgData{new char[msgSize]{}};
        msg.serialize(msgData, msgSize);
        emulNet->ENsend(&memberNode->addr, &destAddr, msgData, msgSize);
        delete[] msgData;
        msgData = nullptr;
    }

    this->shouldDeleteMember = true;
    return 0;
}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop()
{
    if (memberNode->bFailed)
    {
        return false;
    }
    else
    {
        return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size)
{
    Queue q;
    return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * Check your messages in queue and perform membership protocol duties
 */
int MP1Node::nodeLoop()
{
    if (memberNode->bFailed)
    {
        return 0;
    }

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if (!memberNode->inGroup)
    {
        return 0;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();

    return 0;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages()
{
    void *ptr;
    int size;

    // Pop waiting messages from memberNode's mp1q
    while (!memberNode->mp1q.empty())
    {
        ptr = memberNode->mp1q.front().elt;
        size = memberNode->mp1q.front().size;
        memberNode->mp1q.pop();
        recvCallBack((char *)ptr, size);
    }
    return;
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: When this is called, the node has received and responded
 * to all messages. We will now run through the updateBuffer
 * for any updates we haven't applied and perform our own
 * membership duties by checking timeouts and sending a ping.
 */
void MP1Node::nodeLoopOps()
{
    // Apply all updates still in the buffer
    std::vector<UpdateMsg> currentUpdates{updateBuffer.getAllUpdates()};
    for (int i{0}; i < (int)currentUpdates.size(); i++)
    {
        processUpdate(currentUpdates.at(i));
    }

    // check suspect list and create FAIL updates if timeout
    if (suspectList.size() > 0)
    {
        // we need to check our suspectList for any timeouts
        std::vector<std::pair<Address, short>>::iterator iter;
        for (iter = suspectList.begin(); iter != suspectList.end();)
        {
            if (iter->second > memberNode->timeOutCounter)
            {
                removeMember(iter->first);
                addDead(iter->first, this->timestamp);
                updateBuffer.insertUpdate(UpdateMsg(iter->first, FAIL, this->timestamp));
                iter = suspectList.erase(iter);
            }
            else
            {
                iter->second++;
                ++iter;
            }
        }
    }
    // check inflight indirect pings and addSuspect if timeout
    if (indirectPings.size() > 0)
    {
        // we need to check our inflight indirect pings
        std::vector<std::pair<Address, short>>::iterator iter;
        for (iter = indirectPings.begin(); iter != indirectPings.end();)
        {
            if (iter->second > 4)
            // four protocol periods is how long it should take
            // src -1-> proxy -2-> tgt -3-> proxy -4-> src
            {
                addSuspect(iter->first);
                updateBuffer.insertUpdate(UpdateMsg(iter->first, SUSPECT, timestamp));
                iter = indirectPings.erase(iter);
            }
            else
            {
                iter->second++;
                ++iter;
            }
        }
    }

    // check inflight ping and create indirect pings if timeout
    if (!isNullAddress(lastPing))
    {
        // create list of K unique neighbors to pingreq
        int k{0};
        if ((int)memberNode->memberList.size() < K_NEIGHBORS + 2) // adding two so we dont count ourself or the target
        {
            k = (int)memberNode->memberList.size() - 2;
        }
        else
        {
            k = K_NEIGHBORS;
        }
        Address k_neighbors[k]{};

        int mle = rand() % (int)memberNode->memberList.size();
        int i{0};
        while (i < k)
        {
            if (mle >= (int)memberNode->memberList.size())
            {
                mle = 0;
            }
            MemberListEntry *k_mle{&memberNode->memberList.at(mle)};
            Address k_neighbor{Address()};
            *(int *)&k_neighbor.addr = k_mle->id;
            *(short *)&k_neighbor.addr[4] = k_mle->port;
            if (k_neighbor == lastPing || k_neighbor == memberNode->addr)
            {
                mle++;
                continue;
            }
            k_neighbors[i] = k_neighbor;
            mle++;
            i++;
        }

        for (int i{0}; i < k; i++)
        {
            sendPingReq(k_neighbors[i], lastPing);
        }

        addIndirect(lastPing);
        lastPing = Address();
    }

    /**
     * pick the next ping target based on heartbeat age
     * and also increment the heartbeats
     */
    long maxHeartbeat{0};
    long minTimestamp{0}; // used for tiebreaking
    Address nodeAddr = Address();
    Address nextPing = Address();
    for (int i{0}; i < (int)memberNode->memberList.size(); i++)
    {
        memberNode->memberList.at(i).heartbeat++;

        *(int *)(&nodeAddr.addr) = memberNode->memberList.at(i).id;
        *(short *)(&nodeAddr.addr[4]) = memberNode->memberList.at(i).port;
        if (nodeAddr == memberNode->addr)
        {
            continue;
        }
        if (memberNode->memberList.at(i).heartbeat > maxHeartbeat)
        {
            maxHeartbeat = memberNode->memberList.at(i).heartbeat;
            minTimestamp = memberNode->memberList.at(i).timestamp;
            nextPing = nodeAddr;
        }
        else if (memberNode->memberList.at(i).heartbeat == maxHeartbeat && memberNode->memberList.at(i).timestamp < minTimestamp)
        {
            maxHeartbeat = memberNode->memberList.at(i).heartbeat;
            minTimestamp = memberNode->memberList.at(i).timestamp;
            nextPing = nodeAddr;
        }
    }
    sendPingReq(nextPing, nextPing);
    lastPing = nextPing;
}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(char *msgData, int size)
{
    bool res = false;
    /**
     * Constructor reads the char array and creates
     * a MessgeHdr object
     */
    MessageHdr msg{MessageHdr(msgData, size)};

    /**
     * Every event (sending or receiving) causes the
     * clock to increment. So here, we are receiving.
     */
    if (msg.timestamp > timestamp)
    {
        timestamp = msg.timestamp + 1;
    }
    else
    {
        timestamp++;
    }

    /**
     * Read in the updates attached to the message
     */
    updateBuffer.insertUpdatesFromMsg(msg);

    /**
     * We can also mark the sending member as alive since we
     * received a message from them
     */
    stayinAlive(msg.srcAddr);

    switch (msg.msgType)
    {
    case JOINREQ:
        res = recvJoinReq(msg);
        break;
    case JOINACK:
        res = recvJoinAck(msg);
        break;
    case PINGREQ:
        res = recvPingReq(msg);
        break;
    case PINGACK:
        res = recvPingAck(msg);
        break;
    }

    return res;
}

/**
 * FUNCTION NAME: recvJoinReq
 *
 * DESCRIPTION: Message handler JOINREQ messages send by new nodes.
 * The new node needs to synchronize their Lamport clock and
 * membership list, so those will be sent back in the reply.
 */
bool MP1Node::recvJoinReq(MessageHdr &msg)
{
    addMember(msg.srcAddr);
    return sendJoinAck(msg.srcAddr);
}

/**
 * FUNCTION NAME: sendJoinReq
 *
 * DESCRIPTION: Called when a new node is initiated. Sent to introducer
 * node. All values of the message are default since the new node
 * has no information.
 */
bool MP1Node::sendJoinReq(Address &joinAddress)
{
    timestamp++; // Sending message, so increment

    MessageHdr msg{MessageHdr(memberNode->addr, joinAddress, JOINREQ, timestamp, 0)};

    int msgSize{msg.msgSize()};
    char *msgData{new char[msgSize]{}};
    msg.serialize(msgData, msgSize);
    emulNet->ENsend(&memberNode->addr, &joinAddress, msgData, msgSize);
    delete[] msgData;
    msgData = nullptr;
    return true;
}

/**
 * FUNCTION NAME: recvJoinAck
 *
 * DESCRIPTION: Message handler JOINACK messages sent by the introducer.
 * Checks if we see ourself in the attached MemberList before setting
 * inGroup to true.
 *
 * Also used by nodes leaving the system. Leaving nodes will attach a
 * FAIL update about themselves. That update is received and processed
 * in the recvCallBack.
 */
bool MP1Node::recvJoinAck(MessageHdr &msg)
{
    if (msg.updateCount > 0)
    {
        // Check if we see ourselves in the memberlist
        for (int i{0}; i < msg.updateCount; i++)
        {
            if (msg.piggyBack[i].nodeAddress == this->memberNode->addr && msg.piggyBack[i].nodeState == ALIVE)
            {
                this->memberNode->inGroup = true;
                return true;
            }
        }
    }
    // If we dont see ourself, we will ignore the message and try again
    return false;
}

/**
 * FUNCTION NAME: sendJoinAck
 *
 * DESCRIPTION: Called when the node has received has received a JOINREQ.
 * Replies with the full MemberList.
 */
bool MP1Node::sendJoinAck(Address &newNodeAddress)
{
    timestamp++; // Sending message, so increment

    MessageHdr msg{MessageHdr(memberNode->addr, newNodeAddress, JOINACK, timestamp, memberNode->memberList.size())};
    loadMemberList(msg, memberNode->memberList);

    int msgSize{msg.msgSize()};
    char *msgData{new char[msgSize]};
    msg.serialize(msgData, msgSize);
    emulNet->ENsend(&memberNode->addr, &newNodeAddress, msgData, msgSize);
    delete[] msgData;
    msgData = nullptr;
    return true;
}

/**
 * FUNCTION NAME: recvPingReq
 *
 * DESCRIPTION: Message handler PINGREQ sent by all nodes.
 * The targetAddress field in a PINGREQ will determine if
 * the ping is direct or indirect.
 */
bool MP1Node::recvPingReq(MessageHdr &msg)
{
    if (msg.dstAddr == msg.piggyBack[0].nodeAddress)
    {
        /**
         * The message's dstAddr and the targetAddr are the same,
         * therefore we received a direct ping and can reply back
         */
        return sendPingAck(msg.srcAddr, memberNode->addr);
    }
    else
    {
        /**
         * The message's dstAddr and the targetAddr are different,
         * therefore we need to forward the indirect ping.
         * We also need to track the ping session so that we can
         * let the original requestor know if we get something back;
         */
        addForward(msg.srcAddr, msg.piggyBack[0].nodeAddress);
        return sendPingReq(msg.piggyBack[0].nodeAddress, msg.piggyBack[0].nodeAddress);
    }
}

/**
 * FUNCTION NAME: sendPingReq
 *
 * DESCRIPTION: Initiates a direct PING or forwards and indirect PING.
 * The difference between the two is whether the targetAddr is the same
 * as the destAddr. If destAddr != targetAddr, that means that the sender
 * is sending an indirect ping, and the receiver should forward it to the
 * intended target. The targetAddr is stored in the first update
 * that is attached to the message.
 */
bool MP1Node::sendPingReq(Address &destAddr, Address &targetAddr)
{
    timestamp++; // sending message, so increment

    int updateCount = getPiggybackCount();
    MessageHdr msg{MessageHdr(memberNode->addr, destAddr, PINGREQ, timestamp, updateCount)};
    msg.piggyBack[0] = UpdateMsg(targetAddr);
    attachUpdates(msg, updateCount);

    int msgSize{msg.msgSize()};
    char *msgData{new char[msgSize]{}};
    msg.serialize(msgData, msgSize);
    emulNet->ENsend(&memberNode->addr, &destAddr, msgData, msgSize);
    delete[] msgData;
    msgData = nullptr;
    return true;
}

/**
 * FUNCTION NAME: recvPingAck
 *
 * DESCRIPTION: Message handler PingAck messages sent by all nodes.
 * We need to check the following and react accorindingly:
 * 1. The PingAck was a reply to our direct ping
 *    -> reset the member's heartbeat
 * 2. The PingAck was a reply from a node that was suspected
 *    -> propagate ALIVE update
 * 3. The PingAck was a reply to our indirect ping
 *    -> reset the target member's heartbeat
 * 4. The PingAck was a reply to another ping that we forwarded
 *    -> forward pingack to original requester
 */
bool MP1Node::recvPingAck(MessageHdr &msg)
{
    bool result = false;
    /**
     * stayinAlive() call in the recvCallBack() function performs
     * the first two checks, so we only need to do the third and forth.
     */

    // reply to our indirect ping
    if (msg.srcAddr != msg.piggyBack[0].nodeAddress)
    {
        stayinAlive(msg.piggyBack[0].nodeAddress);
        result = true;
    }
    // reply to a forwarded indirect ping
    else
    {
        Address forward = removeForward(msg.srcAddr);
        if (!isNullAddress(forward))
        {
            result = sendPingAck(forward, msg.srcAddr);
        }
    }

    return result;
}

/**
 * FUNCTION NAME: sendPingAck
 *
 * DESCRIPTION: the node has received and PINGREQ and should respond
 * with a PINGACK. Contains the node's timestamp and any updates
 * that can be piggybacked.
 */
bool MP1Node::sendPingAck(Address &destAddr, Address &targetAddr)
{
    timestamp++; // sending message, so increment

    int updateCount{getPiggybackCount()};
    MessageHdr msg{MessageHdr(memberNode->addr, destAddr, PINGACK, timestamp, updateCount)};
    msg.piggyBack[0] = UpdateMsg(targetAddr);
    attachUpdates(msg, updateCount);

    int msgSize{msg.msgSize()};
    char *msgData{new char[msgSize]{}};
    msg.serialize(msgData, msgSize);
    emulNet->ENsend(&memberNode->addr, &destAddr, msgData, msgSize);
    delete[] msgData;
    msgData = nullptr;
    return true;
}

/**
 * FUNCTION NAME: processUpdate
 *
 * DESCRIPTION: reads the input UpdateMsg makes changes to memberList,
 * as long as the UpdateMsg is valid. UpdateMsg's timestamp must be greater
 * than the MLE's timestamp for any changes to be applied.
 */
void MP1Node::processUpdate(UpdateMsg &updateMsg)
{
    // increment the node's timestamp if newer
    if (updateMsg.timestamp > this->timestamp)
    {
        this->timestamp = updateMsg.timestamp;
    }

    // I'm not dead yet!
    if (updateMsg.nodeAddress == this->memberNode->addr && updateMsg.nodeState != ALIVE)
    {
        Address joinAddr = getJoinAddress();
        sendJoinReq(joinAddr);
        return;
    }

    int id{};
    short port{};
    memcpy(&id, updateMsg.nodeAddress.addr, sizeof(int));
    memcpy(&port, updateMsg.nodeAddress.addr + sizeof(int), sizeof(short));

    // Iterate over memberList to find the member
    std::vector<MemberListEntry>::iterator iter;
    for (iter = memberNode->memberList.begin(); iter != memberNode->memberList.end();)
    {
        // Check if id and port are the same
        if (iter->id == id && iter->port == port)
        {
            // Check that the update's timestamp is more recent than the MLE's
            if (updateMsg.timestamp > iter->timestamp)
            {
                switch (updateMsg.nodeState)
                {
                case FAIL:
                    addDead(updateMsg.nodeAddress, updateMsg.timestamp);
                    removeMember(updateMsg.nodeAddress);
                    break;
                case SUSPECT:
                    iter->timestamp = updateMsg.timestamp;
                    addSuspect(updateMsg.nodeAddress);
                    break;
                case ALIVE:
                    iter->timestamp = updateMsg.timestamp;
                    removeSuspect(updateMsg.nodeAddress);
                    break;
                }
            }
            // we found the member, so no reason to continue further
            return;
        }
        ++iter;
    }
    // If we made it this far, the node is not in the memberlist
    switch (updateMsg.nodeState)
    {
    case FAIL:
        addDead(updateMsg.nodeAddress, updateMsg.timestamp);
        break;
    case SUSPECT:
        addSuspect(updateMsg.nodeAddress);
        break;
    case ALIVE:
        removeSuspect(updateMsg.nodeAddress);
        removeDead(updateMsg.nodeAddress, updateMsg.timestamp);
        addMember(updateMsg.nodeAddress, updateMsg.timestamp);
        break;
    }
}

/**
 * FUNCTION NAME: stayinAlive
 *
 * DESCRIPTION: ah, ah, ah, ah, stayin' alive, stayin' alive
 * We know the node is healthy, so the function performs four duties:
 * 1. Checks the suspect list and creates an ALIVE UpdateMsg
 * 2. Checks the indirect ping list and removes it
 * 3. Checks the last direct ping was replied to
 * 4. resets the heartbeat of a member in the memberList
 */
void MP1Node::stayinAlive(Address &addr)
{
    // Add member if not already present
    addMember(addr);

    // Check suspect list
    if (removeSuspect(addr))
    {
        updateBuffer.insertUpdate(UpdateMsg(addr, ALIVE, timestamp));
    }
    // Check indirect list
    removeIndirect(addr);
    // Check last ping
    if (addr == lastPing)
    {
        lastPing = Address();
    }

    // Reset heartbeat
    int id{};
    short port{};
    memcpy(&id, &addr, sizeof(int));
    memcpy(&port, &addr + sizeof(int), sizeof(short));

    for (int i{0}; i < (int)memberNode->memberList.size(); i++)
    {
        if (memberNode->memberList.at(i).id == id && memberNode->memberList.at(i).port == port)
        {
            memberNode->memberList.at(i).heartbeat = 0;
            memberNode->memberList.at(i).timestamp = this->timestamp;
        }
    }
}

/**
 * FUNCTION NAME: addMember
 *
 * DESCRIPTION: Adds a new MemberListEntry to the Member's memberList
 * First checks if the MLE is already present. If so, update the timestamp.
 */
bool MP1Node::addMember(Address &newNode)
{
    // There should REALLY be a different constructor for the MLE...
    int id{};
    short port{};
    memcpy(&id, newNode.addr, sizeof(int));
    memcpy(&port, newNode.addr + sizeof(int), sizeof(short));
    MemberListEntry newMLE = MemberListEntry(id, port, 0, timestamp);

    for (int i{0}; i < (int)memberNode->memberList.size(); i++)
    {
        if (memberNode->memberList.at(i).id == newMLE.id && memberNode->memberList.at(i).port == newMLE.port)
        {
            memberNode->memberList.at(i).timestamp = timestamp;
            return false;
        }
    }

    memberNode->memberList.push_back(newMLE);
    log->logNodeAdd(&memberNode->addr, &newNode);
    return true;
}

bool MP1Node::addMember(Address &newNode, long msgTimestamp)
{
    // check if in deadlist. only remove and proceed if the timestamp is newer
    std::vector<std::pair<Address, long>>::iterator iter;
    for (iter = deadList.begin(); iter != deadList.end();)
    {
        if (iter->first == newNode)
        {
            if (iter->second < msgTimestamp)
            {
                deadList.erase(iter);
                break;
            }
            else
            {
                return false;
            }
        }
        ++iter;
    }

    // There should REALLY be a different constructor for the MLE...
    int id{};
    short port{};
    memcpy(&id, newNode.addr, sizeof(int));
    memcpy(&port, newNode.addr + sizeof(int), sizeof(short));
    MemberListEntry newMLE = MemberListEntry(id, port, 0, msgTimestamp);

    for (int i{0}; i < (int)memberNode->memberList.size(); i++)
    {
        if (memberNode->memberList.at(i).id == newMLE.id && memberNode->memberList.at(i).port == newMLE.port)
        {
            memberNode->memberList.at(i).timestamp = timestamp;
            return false;
        }
    }

    memberNode->memberList.push_back(newMLE);
    log->logNodeAdd(&memberNode->addr, &newNode);

    return true;
}

/**
 * FUNCTION NAME: removeMember
 *
 * DESCRIPTION: Removed an MLE from the Member's memberList
 * Return true if it was present and removed. False otherwise.
 */
bool MP1Node::removeMember(Address &deadNode)
{
    deadList.push_back(std::pair<Address, long>(deadNode, timestamp));

    int id{};
    short port{};
    memcpy(&id, deadNode.addr, sizeof(int));
    memcpy(&port, deadNode.addr + sizeof(int), sizeof(short));

    std::vector<MemberListEntry>::iterator iter;
    for (iter = memberNode->memberList.begin(); iter != memberNode->memberList.end();)
    {
        if (iter->id == id && iter->port == port)
        {
            memberNode->memberList.erase(iter);
            log->logNodeRemove(&memberNode->addr, &deadNode);
            return true;
        }
        ++iter;
    }
    return false;
}

/**
 * FUNCTION NAME: addIndirect
 *
 * DESCRIPTION: Adds a new address to the list of self-initiated
 * indirect pings.
 */
bool MP1Node::addIndirect(Address &addr)
{
    for (int i{0}; i < (int)indirectPings.size(); i++)
    {
        if (indirectPings.at(i).first == addr)
        {
            return false;
        }
    }
    indirectPings.push_back(std::pair<Address, short>(addr, 0));
    return true;
}

/**
 * FUNCTION NAME: removeIndirect
 *
 * DESCRIPTION: Removes and address from the list of self-initiated
 * indirect pings.
 */
bool MP1Node::removeIndirect(Address &addr)
{
    std::vector<std::pair<Address, short>>::iterator iter;
    for (iter = indirectPings.begin(); iter != indirectPings.end(); ++iter)
    {
        if (iter->first == addr)
        {
            indirectPings.erase(iter);
            return true;
        }
    }
    return false;
}

/**
 * FUNCTION NAME: addSuspect
 *
 * DESCRIPTION: adds the node address to the list of suspected
 * nodes. Return true if added, returns false if it was already there.
 */
bool MP1Node::addSuspect(Address &addr)
{
    for (int i{0}; i < (int)suspectList.size(); i++)
    {
        if (suspectList.at(i).first == addr)
        {
            return false;
        }
    }
    suspectList.push_back(std::pair<Address, short>(addr, 0));
    return true;
}

/**
 * FUNCTION NAME: removeSuspect
 *
 * DESCRIPTION: removes the node address to the list of suspected
 * nodes. Return true if removed, returns false if it was not present
 */
bool MP1Node::removeSuspect(Address &addr)
{
    std::vector<std::pair<Address, short>>::iterator iter;
    for (iter = suspectList.begin(); iter != suspectList.end();)
    {
        if (iter->first == addr)
        {
            suspectList.erase(iter);
            return true;
        }
        ++iter;
    }
    return false;
}

/**
 * FUNCTION NAME: addForward
 *
 * DESCRIPTION: track a new indirect ping request. Returns true if it
 * was not already present.
 */
bool MP1Node::addForward(Address &srcAddr, Address &targetAddr)
{
    for (int i{0}; i < (int)forwardedPings.size(); i++)
    {
        if (forwardedPings.at(i).first == srcAddr && forwardedPings.at(i).second == targetAddr)
        {
            return false;
        }
    }
    forwardedPings.push_back(std::pair<Address, Address>(srcAddr, targetAddr));
    return true;
}

/**
 * FUNCTION NAME: removeForward
 *
 * DESCRIPTION: checks the list of forwarded pings for a target and
 * returns the requestor's address after removing it from the list.
 */
Address MP1Node::removeForward(Address &targetAddr)
{
    Address src = Address();
    std::vector<std::pair<Address, Address>>::iterator iter;
    for (iter = forwardedPings.begin(); iter != forwardedPings.end();)
    {
        if (iter->second == targetAddr)
        {
            src = iter->first;
            forwardedPings.erase(iter);
            break;
        }
        ++iter;
    }
    return src;
}

/**
 * FUNCTION NAME: addDead
 *
 * DESCRIPTION: A node has been determined to be dead. We need
 * to track the time of the update and address of the dead node
 */
bool MP1Node::addDead(Address &deadAddr, long msgTimestamp)
{
    // Check if the node is already in the deadlist and update the timestamp
    std::vector<std::pair<Address, long>>::iterator iter;
    for (iter = deadList.begin(); iter != deadList.end();)
    {
        if (iter->first == deadAddr)
        {
            if (iter->second < msgTimestamp)
            {
                // the update is more recent
                iter->second = msgTimestamp;
                return true;
            }
            else
            {
                // the update is older
                return false;
            }
        }
        ++iter;
    }

    // the node is not in the deadlist and we can add it
    deadList.push_back(std::pair<Address, long>(deadAddr, msgTimestamp));
    return true;
}

/**
 * FUNCTION NAME: removeDead
 *
 * DESCRIPTION: A node has been determined to be alive. We need
 * to check if the update is more recent before removing the node
 * from the deadlist
 */
bool MP1Node::removeDead(Address &deadAddr, long msgTimestamp)
{
    // Check if the node is already in the deadlist
    std::vector<std::pair<Address, long>>::iterator iter;
    for (iter = deadList.begin(); iter != deadList.end();)
    {
        if (iter->first == deadAddr)
        {
            if (iter->second < msgTimestamp)
            {
                // the update is more recent
                deadList.erase(iter);
                return true;
            }
            else
            {
                // the update is older
                return false;
            }
        }
        ++iter;
    }
    // the node is not in the deadlist so nothing to be done
    return false;
}

/**
 * FUNCTION NAME: getPiggybackCount
 *
 * DESCRIPTION: returns the number of updates
 * that can be piggybacked on a message.
 * A node may not have enough members in its
 * list or buffered updates to send out a
 * fully loaded message.
 */
int MP1Node::getPiggybackCount()
{
    int sumUpdates = updateBuffer.getUpdateCount() + memberNode->memberList.size() - 1;
    if (sumUpdates < HEADER_SIZE)
    {
        return sumUpdates;
    }
    else
    {
        return HEADER_SIZE;
    }
}

/**
 * FUNCTION NAME: attachUpdates
 *
 * DESCRIPTION: attaches the updates to a message.
 * If the update buffer doesn't have enough to fill a whole
 * message, the node will add ALIVE updates for the nodes
 * in its memberlist
 */
void MP1Node::attachUpdates(MessageHdr &msg, int numUpdates)
{
    int i_p{0};                             // piggyback index
    int i_m{0};                             // memberlist index
    int u_b{updateBuffer.getUpdateCount()}; // number of updates from the buffer

    if (msg.msgType == PINGREQ || msg.msgType == PINGACK)
    {
        i_p = 1; // first UpdateMsg is the ping target
    }

    // pull messages from the buffer
    while (i_p < u_b)
    {
        msg.piggyBack[i_p] = updateBuffer.at(i_p);
        i_p++;
    }
    // generate ALIVE messages from the memberlist
    while (i_p < numUpdates)
    {
        long memberTimestamp = memberNode->memberList.at(i_m).timestamp;
        Address memberAddr = Address();
        *(int *)&memberAddr.addr = memberNode->memberList.at(i_m).id;
        *(short *)&memberAddr.addr[4] = memberNode->memberList.at(i_m).port;
        msg.piggyBack[i_p] = UpdateMsg(memberAddr, ALIVE, memberTimestamp);
        i_p++;
        i_m++;
    }
    updateBuffer.cleanupBuffer();
}

/**
 * FUNCTION NAME: loadMemberList
 *
 * DESCRIPTION: loads all nodes from the node's MemberList into a message's piggyback updates.
 * Intended to be used for a JOINACK message. The message must be initialized correctly
 * with the same number of piggyback messages as MemberListEntries.
 */
void MP1Node::loadMemberList(MessageHdr &msg, std::vector<MemberListEntry> &memberList)
{
    if (msg.updateCount != (int)memberList.size())
    {
        return;
    }
    for (int i{0}; i < (int)memberList.size(); i++)
    {
        *(int *)(&msg.piggyBack[i].nodeAddress.addr) = memberList.at(i).id;
        *(short *)(&msg.piggyBack[i].nodeAddress.addr[4]) = memberList.at(i).port;
        msg.piggyBack[i].nodeState = ALIVE;
        msg.piggyBack[i].timestamp = memberList.at(i).gettimestamp();
    }
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address &addr)
{
    return (memcmp(addr.addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress()
{
    Address joinaddr;

    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address &addr)
{
    printf("%d.%d.%d.%d:%d \n", addr.addr[0], addr.addr[1], addr.addr[2],
           addr.addr[3], *(short *)&addr.addr[4]);
}

bool MP1Node::isFailed() const
{
    return memberNode->bFailed;
}

void MP1Node::setFailed(bool nFailed)
{
    memberNode->bFailed = nFailed;
}

Address MP1Node::getAddress() const
{
    return memberNode->addr;
}

/**
 * STRUCT NAME: MessageHdr
 */

/**
 * MessageHdr overloaded constructors
 */
MessageHdr::MessageHdr(Address &s, Address &d, MsgType t, long ts, int c)
    : srcAddr(s), dstAddr(d), msgType(t), timestamp(ts), updateCount(c)
{
    if (c > 0)
    {
        piggyBack = new UpdateMsg[c]{};
    }
    else
    {
        piggyBack = nullptr;
    }
}
MessageHdr::MessageHdr(int c)
    : srcAddr(), dstAddr(), msgType(JOINREQ), timestamp(0), updateCount(c)
{
    if (c > 0)
    {
        piggyBack = new UpdateMsg[c]{};
    }
    else
    {
        piggyBack = nullptr;
    }
}
MessageHdr::MessageHdr()
    : srcAddr(), dstAddr(), msgType(JOINREQ), timestamp(0), updateCount(0)
{
    piggyBack = nullptr;
}
MessageHdr::MessageHdr(char *msgData, int msgSize)
{
    srcAddr = Address();
    dstAddr = Address();
    int index{0};
    memcpy(&(srcAddr.addr), msgData + index, sizeof(srcAddr.addr));
    index += sizeof(srcAddr.addr);
    memcpy(&(dstAddr.addr), msgData + index, sizeof(dstAddr.addr));
    index += sizeof(dstAddr.addr);
    memcpy(&msgType, msgData + index, sizeof(msgType));
    index += sizeof(msgType);
    memcpy(&timestamp, msgData + index, sizeof(timestamp));
    index += sizeof(timestamp);
    memcpy(&updateCount, msgData + index, sizeof(updateCount));
    index += sizeof(updateCount);

    if (updateCount > 0)
    {
        piggyBack = new UpdateMsg[updateCount]{};
        int addrSize{sizeof(piggyBack[0].nodeAddress.addr)};
        int stateSize{sizeof(piggyBack[0].nodeState)};
        int tsSize{sizeof(piggyBack[0].timestamp)};
        for (int i = 0; i < updateCount; i++)
        {
            memcpy(&(piggyBack[i].nodeAddress.addr), msgData + index, addrSize);
            index += addrSize;
            memcpy(&(piggyBack[i].nodeState), msgData + index, stateSize);
            index += stateSize;
            memcpy(&(piggyBack[i].timestamp), msgData + index, tsSize);
            index += tsSize;
        }
    }
    else
    {
        piggyBack = nullptr;
    }
}

/**
 * MessageHdr destructor
 */
MessageHdr::~MessageHdr()
{
    delete[] piggyBack;
}

/**
 *  MessageHdr copy constructor
 */
MessageHdr::MessageHdr(const MessageHdr &other)
    : srcAddr(other.srcAddr), dstAddr(other.dstAddr), msgType(other.msgType), timestamp(other.timestamp), updateCount(other.updateCount)
{
    piggyBack = new UpdateMsg[other.updateCount];
    for (int i = 0; i < other.updateCount; i++)
    {
        piggyBack[i] = other.piggyBack[i];
    }
}

/**
 * MessageHdr copy assignment operator
 */
MessageHdr &MessageHdr::operator=(const MessageHdr &other)
{
    if (this != &other)
    {
        delete[] piggyBack;
        this->srcAddr = other.srcAddr;
        this->dstAddr = other.dstAddr;
        this->msgType = other.msgType;
        this->timestamp = other.timestamp;
        this->updateCount = other.updateCount;
        this->piggyBack = new UpdateMsg[other.updateCount];
        for (int i = 0; i < other.updateCount; i++)
        {
            piggyBack[i] = other.piggyBack[i];
        }
    }
    return *this;
}

/**
 * MessageHdr move constructor
 */
MessageHdr::MessageHdr(MessageHdr &&other) noexcept
    : srcAddr(other.srcAddr), dstAddr(other.dstAddr), msgType(other.msgType), timestamp(other.timestamp), updateCount(other.updateCount)
{
    piggyBack = other.piggyBack;
    other.piggyBack = nullptr;
}

/**
 * MessageHdr move assignment operator
 */
MessageHdr &MessageHdr::operator=(MessageHdr &other) noexcept
{
    if (this != &other)
    {
        delete[] piggyBack;
        this->srcAddr = other.srcAddr;
        this->dstAddr = other.dstAddr;
        this->msgType = other.msgType;
        this->timestamp = other.timestamp;
        this->updateCount = other.updateCount;
        this->piggyBack = other.piggyBack;
        other.piggyBack = nullptr;
    }
    return *this;
}

/**
 * FUNCTION NAME: msgSize
 *
 * DESCRIPTION: returns the byte size of the object when serialized
 * into a char array. Used when serializing a MessageHdr and allocating
 * the correct buffer size, including the piggyback UpdateMsgs.
 */
int MessageHdr::msgSize()
{
    int headersize{sizeof(srcAddr.addr) + sizeof(dstAddr.addr) + sizeof(msgType) + sizeof(timestamp) + sizeof(updateCount)};
    int updateMsgSize{0};
    if (updateCount > 0)
    {
        updateMsgSize = piggyBack[0].msgSize();
    }
    return headersize + (updateMsgSize * updateCount);
}

/**
 * FUNCTION NAME: serialize
 *
 * DESCRIPTION: copies the MessageHdr data (with member UpdateMsgs)
 * into a provided char array. The char array is provided to the
 * function and should be the correct size by using the member
 * msgSize() function beforehand.
 */
void MessageHdr::serialize(char *msgData, int msgSize)
{
    int index{0};

    memcpy(msgData + index, &(srcAddr.addr), sizeof(srcAddr.addr));
    index += sizeof(srcAddr.addr);
    memcpy(msgData + index, &(dstAddr.addr), sizeof(dstAddr.addr));
    index += sizeof(dstAddr.addr);
    memcpy(msgData + index, &msgType, sizeof(msgType));
    index += sizeof(msgType);
    memcpy(msgData + index, &timestamp, sizeof(timestamp));
    index += sizeof(timestamp);
    memcpy(msgData + index, &updateCount, sizeof(updateCount));
    index += sizeof(updateCount);

    if (updateCount > 0)
    {
        int updateMsgSize = piggyBack[0].msgSize();
        for (int i = 0; i < updateCount; i++)
        {
            piggyBack[i].serialize(msgData + index, updateMsgSize);
            index += updateMsgSize;
        }
    }
}

/**
 * STRUCT NAME: UpdateMsg
 */

/**
 * UpdateMsg overloaded constructors
 */
UpdateMsg::UpdateMsg(Address &addr, MemberState state, long ts)
    : nodeAddress(addr), nodeState(state), timestamp(ts) {}
UpdateMsg::UpdateMsg(Address &addr)
    : nodeAddress(addr), nodeState(ALIVE), timestamp(0) {}
UpdateMsg::UpdateMsg()
    : nodeAddress(), nodeState(ALIVE), timestamp(0) {}

/**
 *  UpdateMsg Destructor
 */
UpdateMsg::~UpdateMsg() {}

/**
 * UpdateMsg copy constructor
 */
UpdateMsg::UpdateMsg(const UpdateMsg &other)
    : nodeAddress(other.nodeAddress), nodeState(other.nodeState), timestamp(other.timestamp) {}

/**
 * UpdateMsg copy assignment operator
 */
UpdateMsg &UpdateMsg::operator=(const UpdateMsg &other)
{
    if (this != &other)
    {
        this->nodeAddress = other.nodeAddress;
        this->nodeState = other.nodeState;
        this->timestamp = other.timestamp;
    }
    return *this;
}

/**
 * UpdateMsg move constructor
 */
UpdateMsg::UpdateMsg(UpdateMsg &&other) noexcept
    : nodeAddress(other.nodeAddress), nodeState(other.nodeState), timestamp(other.timestamp) {}

/**
 * UpdateMsg move assignment operator
 */
UpdateMsg &UpdateMsg::operator=(UpdateMsg &other) noexcept
{
    if (this != &other)
    {
        this->nodeAddress = other.nodeAddress;
        this->nodeState = other.nodeState;
        this->timestamp = other.timestamp;
    }
    return *this;
}

/**
 * UpdateMsg overloaded equality operator
 */
bool UpdateMsg::operator==(const UpdateMsg &anotherMsg) const
{
    if (this->nodeAddress != anotherMsg.nodeAddress)
    {
        return false;
    }
    if (this->nodeState != anotherMsg.nodeState)
    {
        return false;
    }
    if (this->timestamp != anotherMsg.timestamp)
    {
        return false;
    }
    return true;
}

/**
 * UpdateMsg overloaded inequality operator
 */
bool UpdateMsg::operator!=(const UpdateMsg &anotherMsg) const
{
    if (this->nodeAddress != anotherMsg.nodeAddress)
    {
        return true;
    }
    if (this->nodeState != anotherMsg.nodeState)
    {
        return true;
    }
    if (this->timestamp != anotherMsg.timestamp)
    {
        return true;
    }
    return false;
}

/**
 * FUNCTION NAME: msgSize
 *
 * DESCRIPTION: returns the byte size of the object when serialized
 * into a char array. Used when serializing a MessageHdr and allocating
 * the correct buffer size.
 */
int UpdateMsg::msgSize()
{
    return sizeof(nodeAddress.addr) + sizeof(nodeState) + sizeof(timestamp);
}

/**
 * FUNCTION NAME: serialize
 *
 * DESCRIPTION: copies the UpdateMsg data into a provided char array.
 * The char array is provided to the function and should be the correct
 * size by using the member msgSize() function beforehand.
 */
void UpdateMsg::serialize(char *msgData, int msgSize)
{
    int index = 0;

    memcpy(msgData + index, &nodeAddress.addr, sizeof(nodeAddress.addr));
    index += sizeof(nodeAddress.addr);
    memcpy(msgData + index, &nodeState, sizeof(nodeState));
    index += sizeof(nodeState);
    memcpy(msgData + index, &timestamp, sizeof(timestamp));
    index += sizeof(timestamp);
}

/**
 * CLASS NAME: UpdateBuffer
 */

/**
 * UpdateBuffer overloaded constructors
 */
UpdateBuffer::UpdateBuffer()
    : maxUpdates(HEADER_SIZE), maxSent(REPEAT_UPDATE), updateBuffer(0) {}
UpdateBuffer::UpdateBuffer(int max)
    : maxUpdates(max), maxSent(REPEAT_UPDATE), updateBuffer(0) {}
UpdateBuffer::UpdateBuffer(int maxU, int maxS)
    : maxUpdates(maxU), maxSent(maxS), updateBuffer(0) {}

/**
 * UpdateBuffer copy constructor
 */
UpdateBuffer::UpdateBuffer(const UpdateBuffer &other)
    : maxUpdates(other.maxUpdates), maxSent(other.maxSent), updateBuffer(other.updateBuffer) {}

/**
 * UpdateBuffer copy assignment operator
 */
UpdateBuffer &UpdateBuffer::operator=(const UpdateBuffer &other)
{
    if (this != &other)
    {
        this->maxUpdates = other.maxUpdates;
        this->maxSent = other.maxSent;
        this->updateBuffer = other.updateBuffer;
    }
    return *this;
}

/**
 * UpdateBuffer move constructor
 */
UpdateBuffer::UpdateBuffer(UpdateBuffer &&other) noexcept
    : maxUpdates(other.maxUpdates), maxSent(other.maxSent), updateBuffer(other.updateBuffer) {}

/**
 * UpdateBuffer move assignment operator
 */
UpdateBuffer &UpdateBuffer::operator=(UpdateBuffer &other) noexcept
{
    if (this != &other)
    {
        this->maxUpdates = other.maxUpdates;
        this->maxSent = other.maxSent;
        this->updateBuffer = other.updateBuffer;
    }
    return *this;
}

/**
 * FUNCTION NAME: getUpdateCount
 *
 * DESCRIPTION: Returns the number of updates that the
 * buffer wants to send out on piggyback
 */
int UpdateBuffer::getUpdateCount()
{
    if ((int)updateBuffer.size() <= maxUpdates)
    {
        return updateBuffer.size();
    }
    else
    {
        return maxUpdates;
    }
}

/**
 * FUNCTION NAME: cleanupBuffer
 *
 * DESCRIPTION: Removes updates that have been sent out more
 * than the maxSent threshold, and reorders the list based
 * on timestamp (oldest first). Only keeps the most recent
 * update for each node.
 *
 * Making use of the STL to keep this code more optimized
 * than the rest of my garbage.
 *
 * A map would be a way better data structure for my buffer
 * but I cant create a hash operator for Address struct so
 * here we are...
 */
void UpdateBuffer::cleanupBuffer()
{
    // Remove updates that have been sent more than max times
    auto endIter = std::remove_if(updateBuffer.begin(), updateBuffer.end(),
                                  [&](const std::pair<UpdateMsg, short> n)
                                  { return n.second >= maxSent; });
    if (endIter != updateBuffer.end())
    {
        updateBuffer.erase(endIter, updateBuffer.end());
    }

    // Sort the buffer by address (ascending) then timestamp (descending)
    std::sort(updateBuffer.begin(), updateBuffer.end(),
              [](const std::pair<UpdateMsg, short> &a, const std::pair<UpdateMsg, short> &b)
              {
                  if (a.first.nodeAddress == b.first.nodeAddress)
                      return a.first.timestamp > b.first.timestamp;
                  else
                      return a.first.nodeAddress.toString() < b.first.nodeAddress.toString();
              });

    std::vector<std::pair<UpdateMsg, short>> newBuffer;
    // Copy only the latest (first) update to the new buffer
    std::unique_copy(updateBuffer.begin(), updateBuffer.end(), std::back_inserter(newBuffer),
                     [](const std::pair<UpdateMsg, short> &a, const std::pair<UpdateMsg, short> &b)
                     { return a.first.nodeAddress == b.first.nodeAddress; });

    updateBuffer = newBuffer;
}

/**
 * FUNCTION NAME: insertUpdatesFromMsg
 *
 * DESCRIPTION: the node has received updates and needs to add
 * them to the update buffer for future processing. Checks that
 * the update hasn't already been received before adding to the
 * buffer.
 */
void UpdateBuffer::insertUpdatesFromMsg(MessageHdr &msg)
{
    int i{0};
    if (msg.msgType == PINGACK || msg.msgType == PINGREQ)
    {
        i = 1;
    }
    while (i < msg.updateCount)
    {
        if (!(this->contains(msg.piggyBack[i])))
        {
            updateBuffer.push_back(std::pair<UpdateMsg, short>{msg.piggyBack[i], 0});
        }
        i++;
    }
    cleanupBuffer();
}

/**
 * FUNCTION NAME: insertUpdate
 *
 * DESCRIPTION: adds a single update message to the buffer
 * The cleanup after every insert is excessive and should be optimized,
 * but we can save that for later...
 */
void UpdateBuffer::insertUpdate(UpdateMsg update)
{
    if (!(this->contains(update)))
    {
        updateBuffer.push_back(std::pair<UpdateMsg, short>{update, 0});
        cleanupBuffer();
    }
}

/**
 * FUNCTION NAME: contains
 *
 * DESCRIPTION: Checks the buffer if it contains an UpdateMsg
 * for a node that is newer than the one being checked.
 */
bool UpdateBuffer::contains(UpdateMsg &msg)
{
    for (int i = 0; i < (int)updateBuffer.size(); i++)
    {
        if (updateBuffer.at(i).first.nodeAddress == msg.nodeAddress)
        {
            if (updateBuffer.at(i).first.timestamp >= msg.timestamp)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
    }
    return false;
}

/**
 * FUNCTION NAME: getAllUpdates
 *
 * DESCRIPTION: Returns a vector with all updates in the buffer
 */
std::vector<UpdateMsg> UpdateBuffer::getAllUpdates()
{
    std::vector<UpdateMsg> updateVector(updateBuffer.size());
    for (int i{0}; i < (int)updateBuffer.size(); i++)
    {
        updateVector.at(i) = updateBuffer.at(i).first;
    }
    return updateVector;
}

/**
 * FUNCTION NAME: at
 *
 * DESCRIPTION: retrieves the UpdateMsg at the index of the
 * provided parameter. Also increments the send counter
 */
UpdateMsg UpdateBuffer::at(int i)
{
    updateBuffer.at(i).second++;
    return updateBuffer.at(i).first;
}