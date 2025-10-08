/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions. (Revised 2020)
 *
 *  Starter code template
 **********************************/

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
    memberNode->timeOutCounter = constants::DEAD_COUNTER;
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

        Message msg{Message(memberNode->addr, destAddr, JOINACK, timestamp, 1)};
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
        if ((int)memberNode->memberList.size() < constants::K_NEIGHBORS + 2) // adding two so we dont count ourself or the target
        {
            k = (int)memberNode->memberList.size() - 2;
        }
        else
        {
            k = constants::K_NEIGHBORS;
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
    Message msg{Message(msgData, size)};

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
bool MP1Node::recvJoinReq(Message &msg)
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

    Message msg{Message(memberNode->addr, joinAddress, JOINREQ, timestamp, 0)};

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
bool MP1Node::recvJoinAck(Message &msg)
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

    Message msg{Message(memberNode->addr, newNodeAddress, JOINACK, timestamp, memberNode->memberList.size())};
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
bool MP1Node::recvPingReq(Message &msg)
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
    Message msg{Message(memberNode->addr, destAddr, PINGREQ, timestamp, updateCount)};
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
bool MP1Node::recvPingAck(Message &msg)
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
    Message msg{Message(memberNode->addr, destAddr, PINGACK, timestamp, updateCount)};
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
    if (sumUpdates < constants::MESSAGE_SIZE)
    {
        return sumUpdates;
    }
    else
    {
        return constants::MESSAGE_SIZE;
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
void MP1Node::attachUpdates(Message &msg, int numUpdates)
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
void MP1Node::loadMemberList(Message &msg, std::vector<MemberListEntry> &memberList)
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
