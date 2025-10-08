#pragma once

namespace constants
{
    const int BANDWIDTH     = 1;    // Number of messages sent per time period// TODO: implement this variable in the algo
    const int MESSAGE_SIZE  = 10;   // Number of updates piggybacked on a message
    const int REPEAT_UPDATE = 3;    // Number of times an update will be piggybacked
    const int DEAD_COUNTER  = 1;    // Number of time periods before declaring a node dead
    const int K_NEIGHBORS  = 3;    // Number of other nodes to send indirect PINGREQ to

}