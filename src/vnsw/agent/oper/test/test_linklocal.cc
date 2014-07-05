/*
 * Copyright (c) 2013 Juniper Networks, Inc. All rights reserved.
 */

#include "testing/gunit.h"

#include <base/logging.h>
#include <pugixml/pugixml.hpp>
#include <io/event_manager.h>
#include <cmn/agent_cmn.h>
#include <oper/operdb_init.h>
#include <oper/global_vrouter.h>
#include <test/test_cmn_util.h>

#define MAX_SERVICES 5
#define MAX_COUNT 100
#define VHOST_IP "10.1.2.1"

std::string linklocal_name[] = 
    { "metadata", "myservice", "testservice", "newservice", "experimental" };

std::string linklocal_ip[] = 
    { "169.254.169.254", "169.254.1.1", "169.254.10.20",
      "169.254.20.30", "169.254.30.40" };

uint16_t linklocal_port[] = { 80, 8000, 12125, 22000, 34567 };

std::string fabric_dns_name[] =
    { "localhost", "www.juniper.net", "www.google.com",
      "www.cnn.com", "github.com" };

std::string fabric_ip[] = 
    { "10.1.2.3", "10.1.2.4", "10.1.2.5", "10.1.2.6", "10.1.2.7" };

struct PortInfo input[] = { 
    {"vnet1", 1, "1.1.1.1", "00:00:00:01:01:01", 1, 1}, 
};  

uint16_t fabric_port[] = { 8775, 8080, 9000, 12345, 8000 };

IpamInfo ipam_info[] = {
    {"1.1.1.0", 24, "1.1.1.200", true},
    {"1.2.3.128", 27, "1.2.3.129", true},
    {"7.8.9.0", 24, "7.8.9.12", true},
};

class LinkLocalTest : public ::testing::Test {
public:
    LinkLocalTest() : response_count_(0) {}
    ~LinkLocalTest() {}

    void FillServices() {
        for (int i = 0; i < MAX_SERVICES; ++i) {
            services_[i].linklocal_name = linklocal_name[i];
            services_[i].linklocal_ip = linklocal_ip[i];
            services_[i].linklocal_port = linklocal_port[i];
            services_[i].fabric_dns_name = fabric_dns_name[i];
            for (int j = 0; j < i + 1; ++j) {
                services_[i].fabric_ip.push_back(fabric_ip[j]);
            }
            services_[i].fabric_port = fabric_port[i];
        }
    }

    void CheckSandeshResponse(Sandesh *sandesh) {
        response_count_++;
    }

    int sandesh_response_count() { return response_count_; }
    void ClearSandeshResponseCount() { response_count_ = 0; }

    virtual void SetUp() {
        client->Reset();
        Agent::GetInstance()->SetRouterId(Ip4Address::from_string(VHOST_IP));
        FillServices();
        AddLinkLocalConfig(services_, MAX_SERVICES);
        CreateVmportEnv(input, 1, 0);
        client->WaitForIdle();
    }

    virtual void TearDown() {
        client->Reset();
        client->WaitForIdle();
        DelIPAM("ipam1");
        client->WaitForIdle();
        DeleteVmportEnv(input, 1, 1, 0); 
        client->WaitForIdle();
    }
    TestLinkLocalService services_[MAX_SERVICES];

private:
    int response_count_;
};

TEST_F(LinkLocalTest, LinkLocalReqTest) {
    AddIPAM("ipam1", ipam_info, 3);
    client->WaitForIdle();
    struct PortInfo new_input[] = { 
        {"vnet2", 2, "2.2.2.2", "00:00:00:02:02:02", 2, 2}, 
    };  
    CreateVmportEnv(new_input, 1, 0);
    client->WaitForIdle();
    client->Reset();

    // check that all expected routes are added
    for (int i = 0; i < MAX_SERVICES; ++i) {
        Inet4UnicastRouteEntry *rt =
            RouteGet("vrf1", Ip4Address::from_string(linklocal_ip[i]), 32);
        EXPECT_TRUE(rt != NULL);
        EXPECT_TRUE(rt->GetActiveNextHop()->GetType() == NextHop::RECEIVE);
    }
    for (int i = 0; i < MAX_SERVICES; ++i) {
        Inet4UnicastRouteEntry *rt =
            RouteGet("vrf2", Ip4Address::from_string(linklocal_ip[i]), 32);
        EXPECT_TRUE(rt != NULL);
        EXPECT_TRUE(rt->GetActiveNextHop()->GetType() == NextHop::RECEIVE);
    }
    for (int i = 0; i < MAX_SERVICES; ++i) {
        Inet4UnicastRouteEntry *rt =
            RouteGet(Agent::GetInstance()->GetDefaultVrf(),
                     Ip4Address::from_string(fabric_ip[i]), 32);
        EXPECT_TRUE(rt != NULL);
        EXPECT_TRUE(rt->GetActiveNextHop()->GetType() == NextHop::ARP);
    }
    int count = 0;
    while (Agent::GetInstance()->oper_db()->global_vrouter()->IsAddressInUse(
                Ip4Address::from_string("127.0.0.1")) == false &&
           count++ < MAX_COUNT)
        usleep(1000);
    EXPECT_TRUE(count < MAX_COUNT);

    // Delete linklocal config
    Agent::GetInstance()->oper_db()->global_vrouter()->
        LinkLocalCleanup();
    client->WaitForIdle();
    DelLinkLocalConfig();
    client->WaitForIdle();

    // check that all routes are deleted
    for (int i = 0; i < MAX_SERVICES; ++i) {
        Inet4UnicastRouteEntry *rt =
            RouteGet("vrf1", Ip4Address::from_string(linklocal_ip[i]), 32);
        EXPECT_TRUE(rt == NULL);
    }
    for (int i = 0; i < MAX_SERVICES; ++i) {
        Inet4UnicastRouteEntry *rt =
            RouteGet("vrf2", Ip4Address::from_string(linklocal_ip[i]), 32);
        EXPECT_TRUE(rt == NULL);
    }
    EXPECT_FALSE(Agent::GetInstance()->oper_db()->global_vrouter()->IsAddressInUse(
                 Ip4Address::from_string("127.0.0.1")));

    client->Reset();
    DeleteVmportEnv(new_input, 1, 1, 0); 
    client->WaitForIdle();
}

TEST_F(LinkLocalTest, LinkLocalChangeTest) {
    // change linklocal request and send request for 3 services_
    services_[2].linklocal_ip = "169.254.100.100";
    services_[2].fabric_ip.clear();
    AddLinkLocalConfig(services_, 3);
    client->WaitForIdle();
    for (int i = 0; i < MAX_SERVICES; ++i) {
        Inet4UnicastRouteEntry *rt =
            RouteGet("vrf1", Ip4Address::from_string(linklocal_ip[i]), 32);
        if (i < 2) {
            EXPECT_TRUE(rt != NULL);
            EXPECT_TRUE(rt->GetActiveNextHop()->GetType() == NextHop::RECEIVE);
        } else {
            EXPECT_TRUE(rt == NULL);
        }
    }
    Inet4UnicastRouteEntry *local_rt =
        RouteGet("vrf1", Ip4Address::from_string("169.254.100.100"), 32);
    EXPECT_TRUE(local_rt != NULL);
    EXPECT_TRUE(local_rt->GetActiveNextHop()->GetType() == NextHop::RECEIVE);
    for (int i = 0; i < 3; ++i) {
        Inet4UnicastRouteEntry *rt =
            RouteGet(Agent::GetInstance()->GetDefaultVrf(),
                     Ip4Address::from_string(fabric_ip[i]), 32);
        EXPECT_TRUE(rt != NULL);
        EXPECT_TRUE(rt->GetActiveNextHop()->GetType() == NextHop::ARP);
    }

    // call introspect
    LinkLocalServiceInfo *sandesh_info = new LinkLocalServiceInfo();
    Sandesh::set_response_callback(boost::bind(&LinkLocalTest::CheckSandeshResponse, this, _1));
    sandesh_info->HandleRequest();
    client->WaitForIdle();
    sandesh_info->Release();
    EXPECT_TRUE(sandesh_response_count() == 1);
    ClearSandeshResponseCount();

    // Delete linklocal config
    Agent::GetInstance()->oper_db()->global_vrouter()->
        LinkLocalCleanup();
    client->WaitForIdle();
    DelLinkLocalConfig();
    client->WaitForIdle();

    // check that all routes are deleted
    for (int i = 0; i < MAX_SERVICES; ++i) {
        Inet4UnicastRouteEntry *rt =
            RouteGet("vrf1", Ip4Address::from_string(linklocal_ip[i]), 32);
        EXPECT_TRUE(rt == NULL);
    }
    local_rt = RouteGet("vrf1", Ip4Address::from_string("169.254.100.100"), 32);
    EXPECT_TRUE(local_rt == NULL);
}

TEST_F(LinkLocalTest, GlobalVrouterDeleteTest) {
    AddIPAM("ipam1", ipam_info, 3);
    client->WaitForIdle();
    struct PortInfo new_input[] = { 
        {"vnet2", 2, "2.2.2.2", "00:00:00:02:02:02", 2, 2}, 
    };  
    CreateVmportEnv(new_input, 1, 0);
    client->WaitForIdle();
    client->Reset();

    // check that all expected routes are added
    for (int i = 0; i < MAX_SERVICES; ++i) {
        Inet4UnicastRouteEntry *rt =
            RouteGet("vrf1", Ip4Address::from_string(linklocal_ip[i]), 32);
        EXPECT_TRUE(rt != NULL);
        EXPECT_TRUE(rt->GetActiveNextHop()->GetType() == NextHop::RECEIVE);
    }
    for (int i = 0; i < MAX_SERVICES; ++i) {
        Inet4UnicastRouteEntry *rt =
            RouteGet("vrf2", Ip4Address::from_string(linklocal_ip[i]), 32);
        EXPECT_TRUE(rt != NULL);
        EXPECT_TRUE(rt->GetActiveNextHop()->GetType() == NextHop::RECEIVE);
    }
    for (int i = 0; i < MAX_SERVICES; ++i) {
        Inet4UnicastRouteEntry *rt =
            RouteGet(Agent::GetInstance()->GetDefaultVrf(),
                     Ip4Address::from_string(fabric_ip[i]), 32);
        EXPECT_TRUE(rt != NULL);
        EXPECT_TRUE(rt->GetActiveNextHop()->GetType() == NextHop::ARP);
    }

    // Delete global vrouter config; this is different from the earlier case
    // where we delete the link local entries
    Agent::GetInstance()->oper_db()->global_vrouter()->
        LinkLocalCleanup();
    client->WaitForIdle();
    DeleteGlobalVrouterConfig();
    client->WaitForIdle();

    // check that all routes are deleted
    for (int i = 0; i < MAX_SERVICES; ++i) {
        Inet4UnicastRouteEntry *rt =
            RouteGet("vrf1", Ip4Address::from_string(linklocal_ip[i]), 32);
        EXPECT_TRUE(rt == NULL);
    }
    for (int i = 0; i < MAX_SERVICES; ++i) {
        Inet4UnicastRouteEntry *rt =
            RouteGet("vrf2", Ip4Address::from_string(linklocal_ip[i]), 32);
        EXPECT_TRUE(rt == NULL);
    }

    client->Reset();
    DeleteVmportEnv(new_input, 1, 1, 0); 
    client->WaitForIdle();
    DelLinkLocalConfig();
    client->WaitForIdle();
}

void RouterIdDepInit(Agent *agent) {
}

int main(int argc, char *argv[]) {
    GETUSERARGS();

    client = TestInit(init_file, ksync_init, false, false, false);
    client->WaitForIdle();

    int ret = RUN_ALL_TESTS();
    client->WaitForIdle();
    TestShutdown();
    delete client;
    return ret;
}
