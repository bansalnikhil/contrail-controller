/*
 * Copyright (c) 2013 Juniper Networks, Inc. All rights reserved.
 */

#include "bgp/mvpn/mvpn_table.h"

class MvpnManager {
public:
    MvpnManager();
    virtual ~MvpnManager();

private:
    void Initialize();

    MvpnTable *table_;
};

MvpnManager::MvpnManager() : table_(new MvpnTable()) {
}

MvpnManager::~MvpnManager() {
}

void Initialize() {
    // Originate Type-1 Internal Auto-Discovery Route
    // Originate Type-2 External Auto-Discovery Route
}
