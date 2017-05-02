/*
 * Copyright (c) 2017 Juniper Networks, Inc. All rights reserved.
 */

#include <boost/asio.hpp>
#include <boost/asio/ip/address.hpp>
#include <testing/gunit.h>

#include "bfd/bfd_client.h"
#include "bfd/bfd_control_packet.h"
#include "bfd/bfd_server.h"
#include "bfd/bfd_session.h"
#include "bfd/test/bfd_test_utils.h"

#include "base/test/task_test_util.h"

using namespace BFD;
using namespace std;

class Communicator : public Connection {
public:
    typedef std::map<Connection *, Connection *> Links;
    static Links links_;

    Communicator() { }
    virtual ~Communicator() { }

    virtual void SendPacket(const boost::asio::ip::address &dstAddr,
                            const boost::asio::mutable_buffer &buffer,
                            int pktSize) {
        // Find other end-point from the links map.
        Links::const_iterator it = links_.find(this);
        if (it != links_.end()) {
            boost::system::error_code error;
            it->second->HandleReceive(buffer,
                boost::asio::ip::udp::endpoint(dstAddr, 1234), pktSize, error);
        }
    }
    virtual void HandleReceive(const boost::asio::const_buffer &recv_buffer,
                               boost::asio::ip::udp::endpoint remote_endpoint,
                               std::size_t bytes_transferred,
                               const boost::system::error_code& error) {
        Connection::HandleReceive(recv_buffer, remote_endpoint,
                                  bytes_transferred, error);
    }
    virtual void NotifyStateChange(const boost::asio::ip::address& remoteHost,
                                   const bool &up) {
    }
    virtual Server *GetServer() const { return server_; }
    virtual void SetServer(Server *server) { server_ = server; }

private:
    Server *server_;
};

Communicator::Links Communicator::links_;

class ClientTest : public ::testing::Test {
 protected:
    ClientTest() :
        server_(&evm_, &cm_), client_(&cm_),
        server_test_(&evm_, &cm_test_), client_test_(&cm_test_) {
    }

    EventManager evm_;
    Communicator cm_;
    Server server_;
    Client client_;

    // Test BFD end-points
    Communicator cm_test_;
    Server server_test_;
    Client client_test_;
};


TEST_F(ClientTest, Basic) {
    // Connect two bfd links
    Communicator::links_.insert(make_pair(&cm_, &cm_test_));
    Communicator::links_.insert(make_pair(&cm_test_, &cm_));

    boost::asio::ip::address client_address =
        boost::asio::ip::address::from_string("10.10.10.1");
    boost::asio::ip::address client_test_address =
        boost::asio::ip::address::from_string("192.168.0.1");
    SessionConfig sc;
    EXPECT_EQ(kResultCode_Ok, client_.AddConnection( client_address, sc));

    SessionConfig sc_t;
    EXPECT_EQ(kResultCode_Ok, client_test_.AddConnection(client_test_address,
                                                         sc_t));
    TASK_UTIL_EXPECT_TRUE(client_.Up(client_address));
    TASK_UTIL_EXPECT_TRUE(client_test_.Up(client_test_address));
}

int main(int argc, char **argv) {
    LoggingInit();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
