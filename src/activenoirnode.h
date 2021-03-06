// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ACTIVENOIRNODE_H
#define ACTIVENOIRNODE_H

#include "net.h"
#include "key.h"
#include "wallet/wallet.h"

class CActiveNoirnode;

static const int ACTIVE_NOIRNODE_INITIAL          = 0; // initial state
static const int ACTIVE_NOIRNODE_SYNC_IN_PROCESS  = 1;
static const int ACTIVE_NOIRNODE_INPUT_TOO_NEW    = 2;
static const int ACTIVE_NOIRNODE_NOT_CAPABLE      = 3;
static const int ACTIVE_NOIRNODE_STARTED          = 4;

extern CActiveNoirnode activeNoirnode;

// Responsible for activating the Noirnode and pinging the network
class CActiveNoirnode
{
public:
    enum noirnode_type_enum_t {
        NOIRNODE_UNKNOWN = 0,
        NOIRNODE_REMOTE  = 1,
        NOIRNODE_LOCAL   = 2
    };

private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

    noirnode_type_enum_t eType;

    bool fPingerEnabled;

    /// Ping Noirnode
    bool SendNoirnodePing();

public:
    // Keys for the active Noirnode
    CPubKey pubKeyNoirnode;
    CKey keyNoirnode;

    // Initialized while registering Noirnode
    CTxIn vin;
    CService service;

    int nState; // should be one of ACTIVE_NOIRNODE_XXXX
    std::string strNotCapableReason;

    CActiveNoirnode()
        : eType(NOIRNODE_UNKNOWN),
          fPingerEnabled(false),
          pubKeyNoirnode(),
          keyNoirnode(),
          vin(),
          service(),
          nState(ACTIVE_NOIRNODE_INITIAL)
    {}

    /// Manage state of active Noirnode
    void ManageState();

    std::string GetStateString() const;
    std::string GetStatus() const;
    std::string GetTypeString() const;

private:
    void ManageStateInitial();
    void ManageStateRemote();
    void ManageStateLocal();
};

#endif
