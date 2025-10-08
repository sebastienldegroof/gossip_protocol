
#include "Message.h"
#include "Constants.h"

#include <vector>
#include <algorithm>

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
	void insertUpdatesFromMsg(Message &);
	bool contains(UpdateMsg &msg);
	UpdateMsg at(int);
	std::vector<UpdateMsg> getAllUpdates();
	void cleanupBuffer();
};