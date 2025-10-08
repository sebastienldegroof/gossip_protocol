
#include "Message.h"

/**
 * STRUCT NAME: Message
 */

/**
 * Message overloaded constructors
 */
Message::Message(Address &s, Address &d, MsgType t, long ts, int c)
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
Message::Message(int c)
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
Message::Message()
    : srcAddr(), dstAddr(), msgType(JOINREQ), timestamp(0), updateCount(0)
{
    piggyBack = nullptr;
}
Message::Message(char *msgData, int msgSize)
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
 * Message destructor
 */
Message::~Message()
{
    delete[] piggyBack;
}

/**
 *  Message copy constructor
 */
Message::Message(const Message &other)
    : srcAddr(other.srcAddr), dstAddr(other.dstAddr), msgType(other.msgType), timestamp(other.timestamp), updateCount(other.updateCount)
{
    piggyBack = new UpdateMsg[other.updateCount];
    for (int i = 0; i < other.updateCount; i++)
    {
        piggyBack[i] = other.piggyBack[i];
    }
}

/**
 * Message copy assignment operator
 */
Message &Message::operator=(const Message &other)
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
 * Message move constructor
 */
Message::Message(Message &&other) noexcept
    : srcAddr(other.srcAddr), dstAddr(other.dstAddr), msgType(other.msgType), timestamp(other.timestamp), updateCount(other.updateCount)
{
    piggyBack = other.piggyBack;
    other.piggyBack = nullptr;
}

/**
 * Message move assignment operator
 */
Message &Message::operator=(Message &other) noexcept
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
 * into a char array. Used when serializing a Message and allocating
 * the correct buffer size, including the piggyback UpdateMsgs.
 */
int Message::msgSize()
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
 * DESCRIPTION: copies the Message data (with member UpdateMsgs)
 * into a provided char array. The char array is provided to the
 * function and should be the correct size by using the member
 * msgSize() function beforehand.
 */
void Message::serialize(char *msgData, int msgSize)
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
 * into a char array. Used when serializing a Message and allocating
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