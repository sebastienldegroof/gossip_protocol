/**
 * FILE NAME: IMember.h
 * 
 * DESCRIPTION: Header file for the IMember interface
 */

#pragma once

#include "Address.h"
#include "Params.h"
#include "EmulNet.h"
#include "Log.h"

/**
 * CLASS NAME: IMember
 * 
 * DESCRIPTION: Interface that Application uses to 
 * orchestrate nodes.
 */
class IMember
{
public:
    virtual int finishUpThisNode() = 0;
    virtual int recvLoop() = 0;
    virtual int nodeLoop() = 0;
    virtual void nodeStart(char*, short) = 0;

    virtual bool isFailed() const = 0;
    virtual void setFailed(bool) = 0;
    virtual Address getAddress() const = 0;

    virtual ~IMember() = default;
};