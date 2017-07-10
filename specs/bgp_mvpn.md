# 1. Introduction
Provide BGP NGEN MVpn support to contrail software

# 2. Problem statement
Currently, multicast is supported using ErmVpn (Edge replicated multicast).
This solution is limited to a single virtual-network. i.e., senders and
receivers cannot span across different virtual-networks. Also, this solution
is not inter-operable (yet) with any of the known bgp implementations.

# 3. Proposed solution
Use NGEN-MVpn design to solve intra-vn and inter-vn multicast capabilities

## 3.1 Alternatives considered
None

## 3.2 API schema changes
Do we need to support for a new set of import and export route-targets for
mvpn which overrides those configured for unicast (?) e.g.
set routing-instances v protocols mvpn route-target import-target|export-target

```
diff --git a/src/schema/bgp_schema.xsd b/src/schema/bgp_schema.xsd
--- a/src/schema/bgp_schema.xsd
+++ b/src/schema/bgp_schema.xsd
@@ -289,6 +289,8 @@
         <xsd:enumeration value="inet-vpn"/>
         <xsd:enumeration value="e-vpn"/>
         <xsd:enumeration value="erm-vpn"/>
+        <xsd:enumeration value="inet-mvpn"/>
         <xsd:enumeration value="route-target"/>
         <xsd:enumeration value="inet6"/>
         <xsd:enumeration value="inet6-vpn"/>
diff --git a/src/schema/vnc_cfg.xsd b/src/schema/vnc_cfg.xsd
index 2804d8b..732282b 100644
--- a/src/schema/vnc_cfg.xsd
+++ b/src/schema/vnc_cfg.xsd
@@ -1363,6 +1363,8 @@ targetNamespace="http://www.contrailsystems.com/2012/VNC-CONFIG/0">
          <!-- Enable or disable Mirroring for virtual-network -->
          <xsd:element name='mirror-destination' type="xsd:boolean" default="false" required='optional'
              description='Flag to mark the virtual network as mirror destination network'/>
+         <!-- Enable or disable ipv4-multicast for virtual-network -->
+         <xsd:element name='ipv4-multicast' type="xsd:boolean" default="false" required='optional' description='Flag to enable ipv4 multicast service'/>
     </xsd:all>
 </xsd:complexType>

```

## 3.3 User workflow impact
####Describe how users will use the feature.

## 3.4 UI changes
UI shall provide a way to configure MVpn for bgp and virtual-networks.

## 3.5 Notification impact
####Describe any log, UVE, alarm changes

# 4. Implementation

## 4.1 Capability negotiation
When mvpn AFI is configured, BGP shall exchange Capability with MCAST_VPN NLRI
for IPv4 multicast vn routes. AFI(1)/SAFI(5). This is not enabled by default.

## 4.2 MVpnManager

```
class MVpnManager {
public:

private:
};
```

1. There shall be one instance of MVpnManager per vrf.mvpn.0 table
2. Maintains list of auto-discovered mvpn [bgp] neighbors
3. Manages locally originated mvpn routes (source: local) such as
   Type-1 (AD), and Type-2 AD
4. Handles initialization or cleanup when mvpn is configured or de-configured
5. Introspect

## 4.3 General MVPN Routes Flow

```
Route-Type      CreateWhen         Primary   ReplicateWhen Secondary
================================================================================
Without Inclusive PMSI
T1 AD          Configure Mvpn      vrf.mvpn  RightAway   bgp.mvpn, vrf[s].mvpn
T1 AD          Receive via bgp     bgp.mvpn  RightAway   vrf[s].mvpn
                                                        Send only to I-BGP Peers

Without Inclusive PMSI
T2 AD          Configure Mvpn      vrf.mvpn  RightAway   bgp.mvpn, vrf[s].mvpn
T2 AD          Receive via bgp     bgp.mvpn  RightAway   vrf[s].mvpn
                                                        Send only to E-BGP Peers

Source-Active AD
T5 S-Active AD Receive via xmpp    vrf.mvpn  RightAway   bgp.mvpn, vrf[s].mvpn
T5 S-Active AD Receive via bgp     bgp.mvpn  RightAway   vrf[s].mvpn

SharedTreeJoin
T6 C-<*, G>    Receive via xmpp    vrf.mvpn  Src-Active  bgp.mvpn, vrf[s].mvpn
               PIM-Register                  is present
T6 C-<*, G>    Receive via bgp     bgp.mvpn  RightAway   vrf[s].mvpn

SourceTreeJoin
T7 C-<S, G>    Receive via xmpp    vrf.mvpn  Source is   bgp.mvpn, vrf[s].mvpn
                                             resolvable
T7 C-<S, G>    Receive via bgp     bgp.mvpn  RightAway   vrf[s].mvpn
                                                         Send T3 S-PMSI

With Leaf Info required
T3 S-PMSI      T6/T7 create in vrf vrf.mvpn  RightAway   bgp.mvpn, vrf[s].mvpn
T3 S-PMSI      Receive via bgp     bgp.mvpn  RightAway   vrf[s].mvpn

With PMSI (Ingress Replication + GlobalTreeRootLabel + Encap: MPLS over GRE/UDP)
T4 Leaf-AD    T3 replicated in vrf vrf.mvpn  GlobalErmRt bgp.mvpn, vrf[s].mvpn
                                             available   Update GlobalErmRt with
                                                         Input Tunnel Attribute
T4 Leaf-AD     Receive via bgp     bgp.mvpn  RightAway   vrf[s].mvpn
               or local replication                 Send xmpp update for ingress
                                                    vrouter with PMSI info

```

## 4.4 Phase-1 Events (For Release 4.1)

In Phase-1 receivers always inside the contrail cluster and sender always
outside the cluster. Support for Source specific multicast <C-S,G> only
(No support for C-<*, G> ASM). No support for Inclusive I-PMSI either.

```
Route-Type     CreateWhen          Primary   ReplicateWhen Secondary
================================================================================
Without Inclusive PMSI
T1 AD          Configure Mvpn      vrf.mvpn  RightAway   bgp.mvpn, vrf[s].mvpn
T1 AD          Receive via bgp     bgp.mvpn  RightAway   vrf[s].mvpn
                                                        Send only to I-BGP Peers

Without Inclusive PMSI
T2 AD          Configure Mvpn      vrf.mvpn  RightAway   bgp.mvpn, vrf[s].mvpn
T2 AD          Receive via bgp     bgp.mvpn  RightAway   vrf[s].mvpn
                                                        Send only to E-BGP Peers

SourceTreeJoin
T7 C-<S, G>    Receive via xmpp    vrf.mvpn  Source is   bgp.mvpn, vrf[s].mvpn
                                             resolvable

With Leaf Info required
T3 S-PMSI      Receive via bgp     bgp.mvpn  RightAway   vrf[s].mvpn

With PMSI (Ingress Replication + GlobalTreeRootLabel + Encap: MPLS over GRE/UDP)
T4 Leaf-AD    T3 replicated in vrf vrf.mvpn  GlobalErmRt bgp.mvpn, vrf[s].mvpn
                                             available   Update GlobalErmRt with
                                                         Input Tunnel Attribute
```

Note: Whenever a route is replicated in bgp.mvpn.0, it is expected to be sent
to all remote bgp peers (subjected to route-target-filtering) and imported into
vrf[-s].mvpn.0 based on the export route-target of the route and import route
target of the table.

MVpn work flow is essentially handled completely based on route change
notification. (MVpn Manager is listener to multiple tables)

## 4.5 Auto Discovery

MVpnManager of each vpn.mvpn.0 generates Type-1 and Type-2 A-D Route in each of
the vrf.mvpn.0 (When ever mvpn is configured/enabled in the VN) (Note: There is
no PMSI info encoded)

Originator control-node IP address, router-id and asn are used where ever
originator information is encoded.


```
1:RD:OriginatorIpAddr  (RD in asn:vn-id or router-id:vn-id format)
  1:self-control-node-router-id:vn-id:originator-control-node-ip-address OR
  1:asn:vn-id:originator-control-node-ip-address
```
  export route target is export route target of the of the routing-instance.
  These routes would get imported to all mvpn tables whose import route-targets
  list contains this exported route-target (Similar to how vpn-unicast routes
  get imported) (aka JUNOS auto-export)

```
1:RD:SourceAs  (RD in asn:vn-id or router-id:vn-id format)
  1:self-control-node-router-id:vn-id:source-as OR
  1:asn:vn-id:source-as
```

These routes should get replicated to bgp.mvpn.0 and then shall be advertised to
all BGP neighbors with whom mvpn AFI is exchanged as part of the initial
capability negotiation. This is bgp based mvpn-site auto discovery.

Note: Intra-AS route is only advertised to IBGP neighbors and Inter-AS route
is advertised to only e-bgp neighbors. Since, those neighbors are already part
of distinct group on the outbound side, simple hard-coded filtering can be
applied to get this functionality.

## 4.6 Source Tree Join C-<S,G> Routes learning via XMPP/IGMP

Agent sends over XMPP, IGMP joins over VMI as C-<S,G> routes and keeps track of
map of all S-G routes => List of VMIs (and VRFs) (for mapping to tree-id). These
C-<S,G> routes are added to vrf.mvpn.0 table with source protocol XMPP as Type7
route in vrf.mvpn.0. This shall have zero-rd as the source-root-rd and 0 as the
root-as asn (since these values are unknown/NA)


Format of Type 7 <C-S, G> route added to vrf.mvpn.0 with protocol local/MVpn
```
  7:<zero-router-id>:<vn-id>:<zero-as>:<C, G>
```

/32 Source address is registered for resolution via resolver.

When ever this address is resolvable (or otherwise), notification is expected
to be called back into MVpnManager, under this Type-7 route db-task context.

o If the route is resolvable (over BGP), next-hop address and rt-import
  extended rtarget community associated with the route is retrieved from the
  resolver data structure (which holds a copy of the information necessary such
  as path-attributes).

  If the next-hop (i.e, Multicast Source) is resolvable, then this Type-7 path
  is replicated into all vrfs applicable, including bgp.mvpn.0.

o If the route is not resolvable any more, then any already replicated type-7
  path is deleted

Format of replicated path for Type 7 C-<S, G> replicated path with protocol MVpn
```
  7:<source-root-rd>:<root-as>:<C, G>
  7:source-root-router-id:vn-id:<root-as>:<C, G>
  export route-target should be rt-import route-target of route towards source
  (as advertised by ingress PE)
```

This should should get replicated to bgp.mvpn.0, and then shall be advertised to
all other mvpn neighbors. (Route Target Filtering will ensure that it is only
sent to the ingress PE). For phase 1 this route only needs to be replicated to
bgp.mvpn.0 (Sender is always outside the contrail cluster)

Any change to readability to Source shall be dealt as delete of old type-7
secondary path and add of new type-7 secondary path

Note: This requires advertising IGMP Routes as XMPP routes into different table
vrf.mvpn.0 (instead of vrf.ermvpn.0). Hence requires changes to agent code as
well.

## 4.7 Nexthop resolver for source

As mentioned in previous section, when C-<S, G> route is received and installed
in vrf.mvpn.0 table (protocol: XMPP), MVpnManager would get notified. One of the
actions to take for this particular event is to monitor for route resolution
towards the source. PathResolver code can be used mostly  as is to get this
requirement fulfilled.

[Reference Changes](https://github.com/rombie/contrail-controller/pull/10/files)

In phase 1, Senders are _always_ outside the cluster and receivers are always
inside the cluster. In this phase, source is expected to resolve always over a
BGP path, with nexthop pointing to one of the SDN gateways.

Note: If sender support within data-center is supported, then code should handle
the case when next-hop is resolvable directly through this vrf or a different
vrf (via XMPP). If it is through bgp, it is also possible that this is still an
xmpp path (in peer control-node). We do not need to join to the peer towards the
source if the source is already part of the same virtual network as the ermvpn
tree built is already a complete bi-directional tree which includes all intended
multicast senders and receivers for a given <C-S, G>.

When <S,G> Type-7 route sent by egress PE is successfully imported into
vrf.mvpn.0 (by matching auto-generated rt-import route-target) by the ingress
PE, if provider-tunnel is configured (as shown above), then ingress PE should
generate Type 3 C-<S, G> S-PMSI AD Route into vrf.mvpn.0 table. From here, this
would replicated to bgp.mvpn.0 and is advertised to remote (egress) PEs.

PathResolver code can be modified to
1. Support longest prefix match
2. Add resolved paths into rt only conditionally. mpvn does not need explicit
   resolved paths in the route. It only needs unicast route resolution
3. When unicast RPF Source route is [un-]resolved, route is notified. In the
   replication logic, Type-7 C-<S,G> route can be generated into bgp.mvpn.0
   table with the correct RD and export route target (or delete the route if
   it was replicated before and the RPF Source is no longer resolvable, or its
   resolution parameters change)

## 4.8 Type-3 S-PMSI

```3:<root-rd>:<C,G>:<Sender-PE-Router-Id> (Page 240)```

Target would be the export target of the vrf (so it would get imported into
vrf.mvpn.0 in the egress pe)

PMSI Flags: Leaf Information Required 1 (So that egress can initiate the join
for the pmsi tunnel) (Similar to RSVP based S-PMSI, Page 251)
TunnelType: Ingress-Replication, Label:0

This route is originated when ever Type-6/Type-7 Customer Join routes are
received and replicated in vrf.mvpn.0 (at the ingress). Origination of this
route is not required for Phase 1.

## 4.8 C-<*, G> Routes learning via Agents/IGMP

This is not targeted for Phase 1. Please refer to Section 4.3 for general
Routes flow for these ASM routes.

## 4.9 Type-4 PMSI Tunnel advertisement from egress (Leaf AD)

When type-3 route is imported into vrf.mvpn.0 table at the egress, (such as
control-node), a Type-4 Leaf AD route can be originated inside the same
vrf.mvpn.0. This is route response to the received type-3 route. However, this
route can be actually sent out only when PMSI information is also available
(for use by forwarding at the ingress). Hence, this route is only replicated
into bgp.mvpn.0 and other applicable vrfs when the PMSI tunnel information is
available. On the other hand, if PMSI tunnel information changes, then this
replicated type-4 Lead AD path must be updated (delete and add) with updated
PMSI tunnel information (Page 254)


```
4:<S-PMSI-Type-3-Route-Prefix>:<Receive-PE-Router-ID>
Route target: <Ingress-PE-Router-ID>:0
PMSI Tunnel: Tunnel type: Ingress-Replication and label:Tree-Label

```

Similar to how Type-7 routes are replicated only when Source is resolvable,
Type-4 paths can be replicated only when GlobalErmVpn route is available. Any
replicated path can be deleted if GlobalErmVpn route is no longer available.

This is handled by listening to changes to GlobalErmVpnRoute. Root node input
label is used as the label value for the PMSI tunnel attribute

? It goes into correct vrf only because the prefix itself is copied ?

## 4.10 Source AS extended community
This is mainly applicable for inter-as mvpn stitched over segmented tunnels.
This is not targeted for Phase 1.

## 4.11 Multicast Edge Replicated Tree
Build one tree for each unique combination of C-<S, G> under each tenant.
Note: <S,G> is expected to be unique with a tenant (project) space.

Tree can be built using very similar logic used in ermvpn.

Master control-node which builds level-0 and level-1 tree shall also be solely
responsible for advertising mvpn routes. Today, this is simply decided based on
control-node with lowest router-id.

Agent shall continue to advertise routes related to BUM traffic over xmpp to
vrf.ermvpn.0 table. However, all mvpn routes (which are based on IGMP joins
or leaves) are always advertised over XMPP to <project>.__default__.mvpn.0
table. (i.e, the name of the VN/VRF is an internally defined constant)

Control-node would build an edge replicated multicast tree like how it does so
already. PMSI Tunnel information is gathered from root-node of each of these
trees. (1:1 mapping between root node of a tree and mvpn Type-3 leaf AD route)
based on C-<S, G>.

When advertising Type3 Leaf AD Route (Section 4.7), egress must also advertise
PMSI tunnel information for forwarding to happen. MVpn Manager shall use the
root node ingress label as the label value in PMSI tunnel attribute

When ever tree is recomputed (and only if root node changes), Type3 route is
updated and re-advertised as necessary. If a new node is selected as root,
Type-3 route can be simply updated and re-advertised (implicit withdraw and
new update)

ErmVpn Tree built inside the contrail cluster is fully bi-directional and self
contained. Vrouter would flood the packets within the tree only so long as the
packet was originated from one of the nodes inside the tree (in the oif list)

This is no longer true when a single node of the tree is stitched with the SDN
gateway using ingress replication. The stitched node should be programmed to
accept multicast packets from SDN gateway (over the GRE/UDP tunnel) and then
flood them among all the nodes contained inside the tree. This is done by
encoding a new "Input Tunnel Attribute" to the xmpp route sent to the agent.
This attribute shall contain the IP address of the tunnel end point (SDN GW)
as well as the Tunnel Type (MPLS over GR/MPLS over UDP) as appropriate.

Vrouter should relax its checks and indeed accept received multicast packets
from this SDN gateway even though that incoming interface may not be part of the
oif list.

When ever GlobalTreeRoute is added/modified, MVpn Manager would get notified as
a listener. This callback happens in the context of ErmVpn table. MVpn Manager
should find associated MVpn Route by inspecting the DB State (of the
GlobalTreeRoute) and notify associated MVpn Type-3 (Leaf AD) if any. During
this operation, necessary forwarding information such as Input label, tunnel
type must be found from the GlobalTreeRoute and stored inside the DB State
associated with the route.

# 5. Performance and scaling impact

##5.2 Forwarding performance

# 6. Upgrade

# 7. Deprecations
####If this feature deprecates any older feature or API then list it here.

# 8. Dependencies
####Describe dependent features or components.

# 9. Testing
## 9.1 Unit tests

## 9.2 Dev tests
## 9.3 System tests

# 10. Documentation Impact

# 11. References
1. [Multicast in MPLS/BGP IP VPNs](https://tools.ietf.org/html/rfc6513)
2. [BGP Encodings and Procedures for Multicast in MPLS/BGP IP VPNs](https://tools.ietf.org/html/rfc6514)
3. [Ingress Replication Tunnels in Multicast VPN](https://tools.ietf.org/html/rfc7988)
4. [Extranet Multicast in BGP/IP MPLS VPNs](https://tools.ietf.org/html/rfc7900)
5. [BGP/MPLS IP Virtual Private Networks (VPNs)](https://tools.ietf.org/html/rfc4364)
