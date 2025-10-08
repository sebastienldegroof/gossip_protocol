/**
 * CLASS NAME: UpdateBuffer
 */

#include "UpdateBuffer.h"

/**
 * UpdateBuffer overloaded constructors
 */
UpdateBuffer::UpdateBuffer()
    : maxUpdates(constants::MESSAGE_SIZE), maxSent(constants::REPEAT_UPDATE), updateBuffer(0) {}
UpdateBuffer::UpdateBuffer(int max)
    : maxUpdates(max), maxSent(constants::REPEAT_UPDATE), updateBuffer(0) {}
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
void UpdateBuffer::insertUpdatesFromMsg(Message &msg)
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