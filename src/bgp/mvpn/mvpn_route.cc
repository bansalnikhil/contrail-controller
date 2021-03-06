/*
 * Copyright (c) 2017 Juniper Networks, Inc. All rights reserved.
 */

#include "bgp/mvpn/mvpn_route.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/string_util.h"
#include "bgp/mvpn/mvpn_table.h"

using std::copy;
using std::string;
using std::vector;

MvpnPrefix::MvpnPrefix() : type_(MvpnPrefix::Unspecified) {
}

// Create LeafADRoute(Type4) prefix from SPMSIAutoDiscoveryRoute(Type3)
MvpnPrefix::MvpnPrefix(uint8_t type, const MvpnPrefix &prefix) {
    if (type == LeafADRoute) {
        if (prefix.type() == SPMSIAutoDiscoveryRoute) {
            size_t rd_size = RouteDistinguisher::kSize;
	    size_t key_size = 0;
            copy(prefix.route_distinguisher().GetData(),
                    prefix.route_distinguisher().GetData() + rd_size,
                rt_key_.begin());
	    key_size = rd_size;
	    rt_key_[key_size] = Address::kMaxV4PrefixLen;
	    key_size += 1;
            const Ip4Address::bytes_type &source_bytes =
		prefix.source().to_bytes();
            copy(source_bytes.begin(), source_bytes.begin() +
		    Address::kMaxV4Bytes, rt_key_.begin() + key_size);
	    key_size += Address::kMaxV4PrefixLen;
	    rt_key_[key_size] = Address::kMaxV4PrefixLen;
            const Ip4Address::bytes_type &group_bytes =
		prefix.group().to_bytes();
            copy(group_bytes.begin(), group_bytes.begin() +
		    Address::kMaxV4Bytes, rt_key_.begin() + key_size);
	    key_size += Address::kMaxV4PrefixLen;
            const Ip4Address::bytes_type &originator_bytes =
		prefix.originator().to_bytes();
            copy(originator_bytes.begin(), originator_bytes.begin() +
		    Address::kMaxV4Bytes, rt_key_.begin() + key_size);
        }
    }
}

MvpnPrefix::MvpnPrefix(uint8_t type, const RouteDistinguisher &rd,
    const uint16_t &asn)
    : type_(type), rd_(rd), asn_(asn) {
}

MvpnPrefix::MvpnPrefix(uint8_t type, const RouteDistinguisher &rd,
    const Ip4Address &originator)
    : type_(type), rd_(rd), originator_(originator) {
}

MvpnPrefix::MvpnPrefix(uint8_t type, const RouteDistinguisher &rd,
    const Ip4Address &group, const Ip4Address &source)
    : type_(type), rd_(rd), group_(group), source_(source) {
}

MvpnPrefix::MvpnPrefix(uint8_t type, const RouteDistinguisher &rd,
    const Ip4Address &originator,
    const Ip4Address &group, const Ip4Address &source)
    : type_(type), rd_(rd), originator_(originator),
      group_(group), source_(source) {
}

int MvpnPrefix::FromProtoPrefix(const BgpProtoPrefix &proto_prefix,
    MvpnPrefix *prefix) {
    size_t rd_size = RouteDistinguisher::kSize;

    prefix->type_ = proto_prefix.type;
    switch (prefix->type_) {
    case IntraASPMSIAutoDiscoveryRoute: {
        size_t rd_offset = 0;
        prefix->rd_ = RouteDistinguisher(&proto_prefix.prefix[rd_offset]);
        size_t originator_offset = rd_offset + rd_size;
        prefix->originator_ = Ip4Address(get_value
                (&proto_prefix.prefix[originator_offset], Address::kMaxV4Bytes));
        break;
    }
    case InterASPMSIAutoDiscoveryRoute: {
        size_t rd_offset = 0;
        prefix->rd_ = RouteDistinguisher(&proto_prefix.prefix[rd_offset]);
        size_t asn_offset = rd_offset + rd_size;
        uint32_t asn = get_value(&proto_prefix.prefix[asn_offset],
                sizeof(uint32_t));
        prefix->asn_ = asn & 0xffff;
        break;
    }
    case SPMSIAutoDiscoveryRoute: {
        size_t nlri_size = proto_prefix.prefix.size();
        size_t expected_nlri_size = rd_size + 2 *
            (Address::kMaxV4Bytes + 1) + Address::kMaxV4Bytes;
        if (nlri_size != expected_nlri_size)
            return -1;
        size_t rd_offset = 0;
        prefix->rd_ = RouteDistinguisher(&proto_prefix.prefix[rd_offset]);
        size_t source_offset = rd_offset + rd_size + 1;
        if (proto_prefix.prefix[source_offset - 1] != Address::kMaxV4PrefixLen)
            return -1;
        prefix->source_ = Ip4Address(
            get_value(&proto_prefix.prefix[source_offset], Address::kMaxV4Bytes));

        size_t group_offset = source_offset + Address::kMaxV4Bytes + 1;
        if (proto_prefix.prefix[group_offset - 1] != Address::kMaxV4PrefixLen)
            return -1;
        prefix->group_ = Ip4Address(
            get_value(&proto_prefix.prefix[group_offset], Address::kMaxV4Bytes));
        size_t originator_offset = group_offset + Address::kMaxV4Bytes;
        prefix->originator_ = Ip4Address(get_value
                (&proto_prefix.prefix[originator_offset], Address::kMaxV4Bytes));
        break;
    }
    case LeafADRoute: {
        size_t key_offset = 0;
        size_t nlri_size = proto_prefix.prefix.size();
        size_t originator_offset = nlri_size - Address::kMaxV4Bytes;
        copy(proto_prefix.prefix.begin(), proto_prefix.prefix.begin() +
                originator_offset, prefix->rt_key_.begin() + key_offset);
        prefix->originator_ = Ip4Address(get_value
                (&proto_prefix.prefix[originator_offset], Address::kMaxV4Bytes));
        break;
    }
    case SourceActiveAutoDiscoveryRoute: {
        size_t rd_offset = 0;
        prefix->rd_ = RouteDistinguisher(&proto_prefix.prefix[rd_offset]);
        size_t source_offset = rd_offset + rd_size + 1;
        if (proto_prefix.prefix[source_offset - 1] != Address::kMaxV4PrefixLen)
            return -1;
        prefix->source_ = Ip4Address(
            get_value(&proto_prefix.prefix[source_offset], Address::kMaxV4Bytes));

        size_t group_offset = source_offset + Address::kMaxV4Bytes + 1;
        if (proto_prefix.prefix[group_offset - 1] != Address::kMaxV4PrefixLen)
            return -1;
        prefix->group_ = Ip4Address(
            get_value(&proto_prefix.prefix[group_offset], Address::kMaxV4Bytes));
        break;
    }
    case SourceTreeJoinRoute:
    case SharedTreeJoinRoute: {
        size_t rd_offset = 0;
        prefix->rd_ = RouteDistinguisher(&proto_prefix.prefix[rd_offset]);
        size_t asn_offset = rd_offset + rd_size;
        uint32_t asn;
        size_t asn_size = sizeof(asn);
               asn = get_value(&proto_prefix.prefix[asn_offset], asn_size);
        prefix->asn_ = asn & 0xffff;
        size_t source_offset = asn_offset + asn_size;
        if (proto_prefix.prefix[source_offset - 1] != Address::kMaxV4PrefixLen)
            return -1;
        prefix->source_ = Ip4Address(
            get_value(&proto_prefix.prefix[source_offset], Address::kMaxV4Bytes));

        size_t group_offset = source_offset + Address::kMaxV4Bytes + 1;
        if (proto_prefix.prefix[group_offset - 1] != Address::kMaxV4PrefixLen)
            return -1;
        prefix->group_ = Ip4Address(
            get_value(&proto_prefix.prefix[group_offset], Address::kMaxV4Bytes));
        break;
    }
    default: {
        return -1;
    }
    }

    return 0;
}

int MvpnPrefix::FromProtoPrefix(BgpServer *server,
                                  const BgpProtoPrefix &proto_prefix,
                                  const BgpAttr *attr, MvpnPrefix *prefix,
                                  BgpAttrPtr *new_attr, uint32_t *label,
                                  uint32_t *l3_label) {
    return FromProtoPrefix(proto_prefix, prefix);
}

void MvpnPrefix::BuildProtoPrefix(BgpProtoPrefix *proto_prefix) const {
    assert(IsValidForBgp(type_));

    size_t rd_size = RouteDistinguisher::kSize;

    proto_prefix->type = type_;
    proto_prefix->prefix.clear();


    switch (type_) {
    case IntraASPMSIAutoDiscoveryRoute: {
        size_t nlri_size = rd_size + Address::kMaxV4Bytes;
        proto_prefix->prefixlen = nlri_size * 8;
        proto_prefix->prefix.resize(nlri_size, 0);

        size_t rd_offset = 0;
        copy(rd_.GetData(), rd_.GetData() + rd_size,
            proto_prefix->prefix.begin() + rd_offset);
        size_t originator_offset = rd_offset + rd_size;
        const Ip4Address::bytes_type &source_bytes = originator_.to_bytes();
        copy(source_bytes.begin(), source_bytes.begin() + Address::kMaxV4Bytes,
            proto_prefix->prefix.begin() + originator_offset);
        break;
    }
    case InterASPMSIAutoDiscoveryRoute: {
        uint32_t asn = asn_;
        size_t asn_size = sizeof(asn);
        size_t nlri_size = rd_size + asn_size;
        proto_prefix->prefixlen = nlri_size * 8;
        proto_prefix->prefix.resize(nlri_size, 0);

        size_t rd_offset = 0;
        copy(rd_.GetData(), rd_.GetData() + rd_size,
            proto_prefix->prefix.begin() + rd_offset);
        size_t asn_offset = rd_offset + rd_size;
        put_value(&proto_prefix->prefix[asn_offset], asn_size, asn);
        break;
    }
    case SPMSIAutoDiscoveryRoute: {
        size_t nlri_size = rd_size + 2 * (1 + Address::kMaxV4Bytes) +
            Address::kMaxV4Bytes;
        proto_prefix->prefixlen = nlri_size * 8;
        proto_prefix->prefix.resize(nlri_size, 0);

        size_t rd_offset = 0;
        copy(rd_.GetData(), rd_.GetData() + rd_size,
            proto_prefix->prefix.begin() + rd_offset);
        size_t source_offset = rd_offset + rd_size + 1;
        proto_prefix->prefix[source_offset - 1] = Address::kMaxV4PrefixLen;
        const Ip4Address::bytes_type &source_bytes = source_.to_bytes();
        copy(source_bytes.begin(), source_bytes.begin() + Address::kMaxV4Bytes,
            proto_prefix->prefix.begin() + source_offset);

        size_t group_offset = source_offset + Address::kMaxV4Bytes + 1;
        proto_prefix->prefix[group_offset - 1] = Address::kMaxV4PrefixLen;
        const Ip4Address::bytes_type &group_bytes = group_.to_bytes();
        copy(group_bytes.begin(), group_bytes.begin() + Address::kMaxV4Bytes,
            proto_prefix->prefix.begin() + group_offset);

        size_t originator_offset = group_offset + Address::kMaxV4Bytes;
        const Ip4Address::bytes_type &originator_bytes = originator_.to_bytes();
        copy(originator_bytes.begin(), originator_bytes.begin() +
                Address::kMaxV4Bytes, proto_prefix->prefix.begin() +
                originator_offset);
        break;
    }
    case LeafADRoute: {
        size_t keySize = sizeof(rt_key_);
        size_t nlri_size = keySize + Address::kMaxV4Bytes;
        proto_prefix->prefixlen = nlri_size * 8;
        proto_prefix->prefix.resize(nlri_size, 0);

        size_t key_offset = 0;
        copy(rt_key_.begin(), rt_key_.begin() + keySize,
                proto_prefix->prefix.begin() + key_offset);

        size_t originator_offset = key_offset + keySize;
        const Ip4Address::bytes_type &originator_bytes = originator_.to_bytes();
        copy(originator_bytes.begin(), originator_bytes.begin() +
                Address::kMaxV4Bytes, proto_prefix->prefix.begin() +
                originator_offset);
        break;
    }
    case SourceActiveAutoDiscoveryRoute: {
        size_t nlri_size = rd_size + 2 * (1 + Address::kMaxV4Bytes);
        proto_prefix->prefixlen = nlri_size * 8;
        proto_prefix->prefix.resize(nlri_size, 0);

        size_t rd_offset = 0;
        copy(rd_.GetData(), rd_.GetData() + rd_size,
            proto_prefix->prefix.begin() + rd_offset);
        size_t source_offset = rd_offset + rd_size + 1;
        proto_prefix->prefix[source_offset - 1] = Address::kMaxV4PrefixLen;
        const Ip4Address::bytes_type &source_bytes = source_.to_bytes();
        copy(source_bytes.begin(), source_bytes.begin() + Address::kMaxV4Bytes,
            proto_prefix->prefix.begin() + source_offset);

        size_t group_offset = source_offset + Address::kMaxV4Bytes + 1;
        proto_prefix->prefix[group_offset - 1] = Address::kMaxV4PrefixLen;
        const Ip4Address::bytes_type &group_bytes = group_.to_bytes();
        copy(group_bytes.begin(), group_bytes.begin() + Address::kMaxV4Bytes,
            proto_prefix->prefix.begin() + group_offset);
        break;
    }
    case SourceTreeJoinRoute:
    case SharedTreeJoinRoute: {
        uint32_t asn = asn_;
        size_t asn_size = sizeof(asn);
        size_t nlri_size = rd_size + asn_size + 2 * (1 + Address::kMaxV4Bytes);
        proto_prefix->prefixlen = nlri_size * 8;
        proto_prefix->prefix.resize(nlri_size, 0);

        size_t rd_offset = 0;
        copy(rd_.GetData(), rd_.GetData() + rd_size,
            proto_prefix->prefix.begin() + rd_offset);
        size_t asn_offset = rd_offset + rd_size;
        put_value(&proto_prefix->prefix[asn_offset], asn_size, asn);
        size_t source_offset = asn_offset + asn_size + 1;
        proto_prefix->prefix[source_offset - 1] = Address::kMaxV4PrefixLen;
        const Ip4Address::bytes_type &source_bytes = source_.to_bytes();
        copy(source_bytes.begin(), source_bytes.begin() + Address::kMaxV4Bytes,
            proto_prefix->prefix.begin() + source_offset);

        size_t group_offset = source_offset + Address::kMaxV4Bytes + 1;
        proto_prefix->prefix[group_offset - 1] = Address::kMaxV4PrefixLen;
        const Ip4Address::bytes_type &group_bytes = group_.to_bytes();
        copy(group_bytes.begin(), group_bytes.begin() + Address::kMaxV4Bytes,
            proto_prefix->prefix.begin() + group_offset);
        break;
    }
    default: {
        assert(false);
        break;
    }
    }
}

MvpnPrefix MvpnPrefix::FromString(const string &str,
    boost::system::error_code *errorp) {
    MvpnPrefix prefix, null_prefix;
    string temp_str;

    // Look for Type.
    size_t pos1 = str.find('-');
    if (pos1 == string::npos) {
        if (errorp != NULL) {
            *errorp = make_error_code(boost::system::errc::invalid_argument);
        }
        return null_prefix;
    }
    temp_str = str.substr(0, pos1);
    stringToInteger(temp_str, prefix.type_);
    if (!IsValid(prefix.type_)) {
        if (errorp != NULL) {
            *errorp = make_error_code(boost::system::errc::invalid_argument);
        }
        return null_prefix;
    }

    if (prefix.type_ < MvpnPrefix::IntraASPMSIAutoDiscoveryRoute ||
        prefix.type_ > MvpnPrefix::SourceTreeJoinRoute) {
        if (errorp != NULL) {
            *errorp = make_error_code(boost::system::errc::invalid_argument);
        }
        return null_prefix;
    }


    switch (prefix.type_) {
    case IntraASPMSIAutoDiscoveryRoute: {
        // Look for RD.
        size_t pos2 = str.find(',', pos1 + 1);
        if (pos2 == string::npos) {
            if (errorp != NULL) {
                *errorp = make_error_code(boost::system::errc::invalid_argument);
            }
            return null_prefix;
        }
        temp_str = str.substr(pos1 + 1, pos2 - pos1 - 1);
        boost::system::error_code rd_err;
        prefix.rd_ = RouteDistinguisher::FromString(temp_str, &rd_err);
        if (rd_err != 0) {
            if (errorp != NULL) {
                *errorp = rd_err;
            }
            return null_prefix;
        }
        // rest is originator
        temp_str = str.substr(pos2 + 1, string::npos);
        boost::system::error_code originator_err;
        prefix.originator_ = Ip4Address::from_string(temp_str, originator_err);
        if (originator_err != 0) {
            if (errorp != NULL) {
                *errorp = originator_err;
            }
            return null_prefix;
        }
        break;
    }
    case InterASPMSIAutoDiscoveryRoute: {
        // Look for RD.
        size_t pos2 = str.find(',', pos1 + 1);
        if (pos2 == string::npos) {
            if (errorp != NULL) {
                *errorp = make_error_code(boost::system::errc::invalid_argument);
            }
            return null_prefix;
        }
        temp_str = str.substr(pos1 + 1, pos2 - pos1 - 1);
        boost::system::error_code rd_err;
        prefix.rd_ = RouteDistinguisher::FromString(temp_str, &rd_err);
        if (rd_err != 0) {
            if (errorp != NULL) {
                *errorp = rd_err;
            }
            return null_prefix;
        }
        // rest is asn
        temp_str = str.substr(pos2 + 1, string::npos);
        if (!stringToInteger(temp_str, prefix.asn_)) {
            return null_prefix;
        }
        break;
    }
    case SPMSIAutoDiscoveryRoute: {
        // Look for RD.
        size_t pos2 = str.find(',', pos1 + 1);
        if (pos2 == string::npos) {
            if (errorp != NULL) {
                *errorp = make_error_code(boost::system::errc::invalid_argument);
            }
            return null_prefix;
        }
        temp_str = str.substr(pos1 + 1, pos2 - pos1 - 1);
        boost::system::error_code rd_err;
        prefix.rd_ = RouteDistinguisher::FromString(temp_str, &rd_err);
        if (rd_err != 0) {
            if (errorp != NULL) {
                *errorp = rd_err;
            }
            return null_prefix;
        }
        // Look for source.
        size_t pos3 = str.find(',', pos2 + 1);
        if (pos3 == string::npos) {
            if (errorp != NULL) {
                *errorp = make_error_code(boost::system::errc::invalid_argument);
            }
            return null_prefix;
        }
        temp_str = str.substr(pos2 + 1, pos3 - pos2 - 1);
        boost::system::error_code source_err;
        prefix.source_ = Ip4Address::from_string(temp_str, source_err);
        if (source_err != 0) {
            if (errorp != NULL) {
                *errorp = source_err;
            }
            return null_prefix;
        }

        // Look for group.
        size_t pos4 = str.find(',', pos3 + 1);
        if (pos4 == string::npos) {
            if (errorp != NULL) {
                *errorp = make_error_code(boost::system::errc::invalid_argument);
            }
            return null_prefix;
        }
	temp_str = str.substr(pos3 + 1, pos4 - pos3 - 1);
	boost::system::error_code group_err;
        prefix.group_ = Ip4Address::from_string(temp_str, group_err);
        if (group_err != 0) {
            if (errorp != NULL) {
                *errorp = group_err;
            }
            return null_prefix;
        }

        // rest is originator
        temp_str = str.substr(pos4 + 1, string::npos);
        boost::system::error_code originator_err;
        prefix.originator_ = Ip4Address::from_string(temp_str, originator_err);
        if (originator_err != 0) {
            if (errorp != NULL) {
                *errorp = originator_err;
            }
            return null_prefix;
        }
        break;
    }
    case LeafADRoute: {
        // Look for route key
        size_t pos2 = str.find('-', pos1 + 1);
        temp_str = str.substr(pos1 + 1, pos2 - pos1 - 1);
        copy(temp_str.begin(), temp_str.begin() + pos2, prefix.rt_key_.begin());
        // rest is originator
        temp_str = str.substr(pos2 + 1, string::npos);
        boost::system::error_code originator_err;
        prefix.originator_ = Ip4Address::from_string(temp_str, originator_err);
        if (originator_err != 0) {
            if (errorp != NULL) {
                *errorp = originator_err;
            }
            return null_prefix;
        }
        break;
    }
    case SourceActiveAutoDiscoveryRoute: {
        // Look for RD.
        size_t pos2 = str.find(',', pos1 + 1);
        if (pos2 == string::npos) {
            if (errorp != NULL) {
                *errorp = make_error_code(boost::system::errc::invalid_argument);
            }
            return null_prefix;
        }
        temp_str = str.substr(pos1 + 1, pos2 - pos1 - 1);
        boost::system::error_code rd_err;
        prefix.rd_ = RouteDistinguisher::FromString(temp_str, &rd_err);
        if (rd_err != 0) {
            if (errorp != NULL) {
                *errorp = rd_err;
            }
            return null_prefix;
        }
        // Look for source.
        size_t pos3 = str.find(',', pos2 + 1);
        if (pos3 == string::npos) {
            if (errorp != NULL) {
                *errorp = make_error_code(boost::system::errc::invalid_argument);
            }
            return null_prefix;
        }
        temp_str = str.substr(pos2 + 1, pos3 - pos2 - 1);
        boost::system::error_code source_err;
        prefix.source_ = Ip4Address::from_string(temp_str, source_err);
        if (source_err != 0) {
            if (errorp != NULL) {
                *errorp = source_err;
            }
            return null_prefix;
        }

        // Look for group.
        size_t pos4 = str.find(',', pos3 + 1);
        if (pos4 == string::npos) {
            if (errorp != NULL) {
                *errorp = make_error_code(boost::system::errc::invalid_argument);
            }
            return null_prefix;
        }
	temp_str = str.substr(pos3 + 1, pos4 - pos3 - 1);
	boost::system::error_code group_err;
        prefix.group_ = Ip4Address::from_string(temp_str, group_err);
        if (group_err != 0) {
            if (errorp != NULL) {
                *errorp = group_err;
            }
            return null_prefix;
        }
        break;
    }
    case SharedTreeJoinRoute:
    case SourceTreeJoinRoute: {
        // Look for RD.
        size_t pos2 = str.find(',', pos1 + 1);
        if (pos2 == string::npos) {
            if (errorp != NULL) {
                *errorp = make_error_code(boost::system::errc::invalid_argument);
            }
            return null_prefix;
        }
        temp_str = str.substr(pos1 + 1, pos2 - pos1 - 1);
        boost::system::error_code rd_err;
        prefix.rd_ = RouteDistinguisher::FromString(temp_str, &rd_err);
        if (rd_err != 0) {
            if (errorp != NULL) {
                *errorp = rd_err;
            }
            return null_prefix;
        }
	// Look for asn
        size_t pos3 = str.find(',', pos2 + 1);
        if (pos3 == string::npos) {
            if (errorp != NULL) {
                *errorp = make_error_code(boost::system::errc::invalid_argument);
            }
            return null_prefix;
        }
        temp_str = str.substr(pos2 + 1, string::npos);
        if (!stringToInteger(temp_str, prefix.asn_)) {
            return null_prefix;
        }
        // Look for source.
        size_t pos4 = str.find(',', pos3 + 1);
        if (pos4 == string::npos) {
            if (errorp != NULL) {
                *errorp = make_error_code(boost::system::errc::invalid_argument);
            }
            return null_prefix;
        }
        temp_str = str.substr(pos3 + 1, pos4 - pos3 - 1);
        boost::system::error_code source_err;
        prefix.source_ = Ip4Address::from_string(temp_str, source_err);
        if (source_err != 0) {
            if (errorp != NULL) {
                *errorp = source_err;
            }
            return null_prefix;
        }

        // Look for group.
        size_t pos5 = str.find(',', pos4 + 1);
        if (pos5 == string::npos) {
            if (errorp != NULL) {
                *errorp = make_error_code(boost::system::errc::invalid_argument);
            }
            return null_prefix;
        }
	temp_str = str.substr(pos4 + 1, pos5 - pos4 - 1);
	boost::system::error_code group_err;
        prefix.group_ = Ip4Address::from_string(temp_str, group_err);
        if (group_err != 0) {
            if (errorp != NULL) {
                *errorp = group_err;
            }
            return null_prefix;
        }
        break;
    }
    }

    return prefix;
}

string MvpnPrefix::ToString() const {
    string repr = integerToString(type_);
    switch (type_) {
        case IntraASPMSIAutoDiscoveryRoute:
            repr += "-" + rd_.ToString();
            repr += "," + originator_.to_string();
            break;
        case InterASPMSIAutoDiscoveryRoute:
            repr += "-" + rd_.ToString();
            repr += "," + integerToString(asn_);
            break;
        case SPMSIAutoDiscoveryRoute:
            repr += "-" + rd_.ToString();
            repr += "," + source_.to_string();
            repr += "," + group_.to_string();
            repr += "," + originator_.to_string();
            break;
        case LeafADRoute: {
            string key(rt_key_.begin(), rt_key_.end());
            repr += "-" + key;
            repr += "," + originator_.to_string();
            break;
        }
        case SourceActiveAutoDiscoveryRoute:
            repr += "-" + rd_.ToString();
            repr += "," + source_.to_string();
            repr += "," + group_.to_string();
            break;
        case SharedTreeJoinRoute:
        case SourceTreeJoinRoute:
            repr += "-" + rd_.ToString();
            repr += "," + integerToString(asn_);
            repr += "," + source_.to_string();
            repr += "," + group_.to_string();
            break;
    }
    return repr;
}

int MvpnPrefix::CompareTo(const MvpnPrefix &rhs) const {
    KEY_COMPARE(type_, rhs.type_);

    switch (type_) {
    case IntraASPMSIAutoDiscoveryRoute: 
        KEY_COMPARE(rd_, rhs.rd_);
        KEY_COMPARE(originator_, rhs.originator_);
        break;
    case InterASPMSIAutoDiscoveryRoute:
        KEY_COMPARE(rd_, rhs.rd_);
        KEY_COMPARE(asn_, rhs.asn_);
        break;
    case SPMSIAutoDiscoveryRoute:
        KEY_COMPARE(rd_, rhs.rd_);
        KEY_COMPARE(source_, rhs.source_);
        KEY_COMPARE(group_, rhs.group_);
        KEY_COMPARE(originator_, rhs.originator_);
        break;
    case LeafADRoute:
        KEY_COMPARE(rt_key_, rhs.rt_key_);
        KEY_COMPARE(originator_, rhs.originator_);
        break;
    case SourceActiveAutoDiscoveryRoute:
        KEY_COMPARE(rd_, rhs.rd_);
        KEY_COMPARE(source_, rhs.source_);
        KEY_COMPARE(group_, rhs.group_);
        break;
    case SourceTreeJoinRoute:
    case SharedTreeJoinRoute:
        KEY_COMPARE(rd_, rhs.rd_);
        KEY_COMPARE(asn_, rhs.asn_);
        KEY_COMPARE(source_, rhs.source_);
        KEY_COMPARE(group_, rhs.group_);
        break;
    default:
        break;
    }
    return 0;
}

string MvpnPrefix::ToXmppIdString() const {
    //assert(type_ == 0);
    string repr = rd_.ToString();
    repr += ":" + group_.to_string();
    repr += "," + source_.to_string();
    return repr;
}

bool MvpnPrefix::IsValidForBgp(uint8_t type) {
    return true;//(type == LocalTreeRoute || type == GlobalTreeRoute);
}

bool MvpnPrefix::IsValid(uint8_t type) {
    return true;//(type == NativeRoute || IsValidForBgp(type));
}

bool MvpnPrefix::operator==(const MvpnPrefix &rhs) const {
    return (
        type_ == rhs.type_ &&
        rd_ == rhs.rd_ &&
        originator_ == rhs.originator_ &&
        group_ == rhs.group_ &&
        source_ == rhs.source_);
}

MvpnRoute::MvpnRoute(const MvpnPrefix &prefix) : prefix_(prefix) {
}

int MvpnRoute::CompareTo(const Route &rhs) const {
    const MvpnRoute &other = static_cast<const MvpnRoute &>(rhs);
    return prefix_.CompareTo(other.prefix_);
    KEY_COMPARE(prefix_.type(), other.prefix_.type());
    KEY_COMPARE(
        prefix_.route_distinguisher(), other.prefix_.route_distinguisher());
    KEY_COMPARE(prefix_.originator(), other.prefix_.originator());
    KEY_COMPARE(prefix_.source(), other.prefix_.source());
    KEY_COMPARE(prefix_.group(), other.prefix_.group());
    KEY_COMPARE(prefix_.asn(), other.prefix_.asn());
    return 0;
}

string MvpnRoute::ToString() const {
    return prefix_.ToString();
}

string MvpnRoute::ToXmppIdString() const {
    if (xmpp_id_str_.empty())
        xmpp_id_str_ = prefix_.ToXmppIdString();
    return xmpp_id_str_;
}

bool MvpnRoute::IsValid() const {
    if (!BgpRoute::IsValid())
        return false;

    return true;
}

void MvpnRoute::SetKey(const DBRequestKey *reqkey) {
    const MvpnTable::RequestKey *key =
        static_cast<const MvpnTable::RequestKey *>(reqkey);
    prefix_ = key->prefix;
}

void MvpnRoute::BuildProtoPrefix(BgpProtoPrefix *prefix,
    const BgpAttr *attr, uint32_t label, uint32_t l3_label) const {
    prefix_.BuildProtoPrefix(prefix);
}

void MvpnRoute::BuildBgpProtoNextHop(
    vector<uint8_t> &nh, IpAddress nexthop) const {
    nh.resize(4);
    const Ip4Address::bytes_type &addr_bytes = nexthop.to_v4().to_bytes();
    copy(addr_bytes.begin(), addr_bytes.end(), nh.begin());
}

DBEntryBase::KeyPtr MvpnRoute::GetDBRequestKey() const {
    MvpnTable::RequestKey *key;
    key = new MvpnTable::RequestKey(GetPrefix(), NULL);
    return KeyPtr(key);
}
