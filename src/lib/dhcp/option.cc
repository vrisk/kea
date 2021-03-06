// Copyright (C) 2011-2016 Internet Systems Consortium, Inc. ("ISC")
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <config.h>
#include <dhcp/libdhcp++.h>
#include <dhcp/option.h>
#include <exceptions/exceptions.h>
#include <util/encode/hex.h>
#include <util/io_utilities.h>

#include <iomanip>
#include <sstream>

#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>

using namespace std;
using namespace isc::util;

namespace isc {
namespace dhcp {

OptionPtr
Option::factory(Option::Universe u,
        uint16_t type,
        const OptionBuffer& buf) {
    return(LibDHCP::optionFactory(u, type, buf));
}


Option::Option(Universe u, uint16_t type)
    :universe_(u), type_(type) {

    // END option (type 255 is forbidden as well)
    if ((u == V4) && ((type == 0) || (type > 254))) {
        isc_throw(BadValue, "Can't create V4 option of type "
                  << type << ", V4 options are in range 1..254");
    }
}

Option::Option(Universe u, uint16_t type, const OptionBuffer& data)
    :universe_(u), type_(type), data_(data) {
    check();
}

Option::Option(Universe u, uint16_t type, OptionBufferConstIter first,
               OptionBufferConstIter last)
    :universe_(u), type_(type), data_(first, last) {
    check();
}

void
Option::check() {
    if ( (universe_ != V4) && (universe_ != V6) ) {
        isc_throw(BadValue, "Invalid universe type specified. "
                  << "Only V4 and V6 are allowed.");
    }

    if (universe_ == V4) {

        if (type_ > 255) {
            isc_throw(OutOfRange, "DHCPv4 Option type " << type_ << " is too big. "
                      << "For DHCPv4 allowed type range is 0..255");
        } else if (data_.size() > 255) {
            isc_throw(OutOfRange, "DHCPv4 Option " << type_ << " is too big.");
            /// TODO Larger options can be stored as separate instances
            /// of DHCPv4 options. Clients MUST concatenate them.
            /// Fortunately, there are no such large options used today.
        }
    }

    // no need to check anything for DHCPv6. It allows full range (0-64k) of
    // both types and data size.
}

void Option::pack(isc::util::OutputBuffer& buf) {
    // Write a header.
    packHeader(buf);
    // Write data.
    if (!data_.empty()) {
        buf.writeData(&data_[0], data_.size());
    }
    // Write sub-options.
    packOptions(buf);
}

void
Option::packHeader(isc::util::OutputBuffer& buf) {
    if (universe_ == V4) {
        if (len() > 255) {
            isc_throw(OutOfRange, "DHCPv4 Option " << type_ << " is too big. "
                      << "At most 255 bytes are supported.");
            /// TODO Larger options can be stored as separate instances
            /// of DHCPv4 options. Clients MUST concatenate them.
            /// Fortunately, there are no such large options used today.
        }

        buf.writeUint8(type_);
        buf.writeUint8(len() - getHeaderLen());

    } else {
        buf.writeUint16(type_);
        buf.writeUint16(len() - getHeaderLen());
    }
}

void
Option::packOptions(isc::util::OutputBuffer& buf) {
    switch (universe_) {
    case V4:
        LibDHCP::packOptions4(buf, options_);
        return;
    case V6:
        LibDHCP::packOptions6(buf, options_);
        return;
    default:
        isc_throw(isc::BadValue, "Invalid universe type " << universe_);
    }
}

void Option::unpack(OptionBufferConstIter begin,
                    OptionBufferConstIter end) {
    setData(begin, end);
}

void
Option::unpackOptions(const OptionBuffer& buf) {
    switch (universe_) {
    case V4:
        LibDHCP::unpackOptions4(buf, getEncapsulatedSpace(), options_);
        return;
    case V6:
        LibDHCP::unpackOptions6(buf, getEncapsulatedSpace(), options_);
        return;
    default:
        isc_throw(isc::BadValue, "Invalid universe type " << universe_);
    }
}

uint16_t Option::len() {
    // Returns length of the complete option (data length + DHCPv4/DHCPv6
    // option header)

    // length of the whole option is header and data stored in this option...
    size_t length = getHeaderLen() + data_.size();

    // ... and sum of lengths of all suboptions
    for (OptionCollection::iterator it = options_.begin();
         it != options_.end();
         ++it) {
        length += (*it).second->len();
    }

    // note that this is not equal to length field. This value denotes
    // number of bytes required to store this option. length option should
    // contain (len()-getHeaderLen()) value.
    return (static_cast<uint16_t>(length));
}

bool
Option::valid() {
    if (universe_ != V4 &&
        universe_ != V6) {
        return (false);
    }

    return (true);
}

OptionPtr Option::getOption(uint16_t opt_type) {
    isc::dhcp::OptionCollection::const_iterator x =
        options_.find(opt_type);
    if ( x != options_.end() ) {
        return (*x).second;
    }
    return OptionPtr(); // NULL
}

bool Option::delOption(uint16_t opt_type) {
    isc::dhcp::OptionCollection::iterator x = options_.find(opt_type);
    if ( x != options_.end() ) {
        options_.erase(x);
        return true; // delete successful
    }
    return (false); // option not found, can't delete
}


std::string Option::toText(int indent) {
    std::stringstream output;
    output << headerToText(indent) << ": ";

    for (unsigned int i = 0; i < data_.size(); i++) {
        if (i) {
            output << ":";
        }
        output << setfill('0') << setw(2) << hex
            << static_cast<unsigned short>(data_[i]);
    }

    // Append suboptions.
    output << suboptionsToText(indent + 2);

    return (output.str());
}

std::string
Option::toString() {
    /// @todo: Implement actual conversion in derived classes.
    return (toText(0));
}

std::vector<uint8_t>
Option::toBinary(const bool include_header) {
    OutputBuffer buf(len());
    try {
        // If the option is too long, exception will be thrown. We allow
        // for this exception to propagate to not mask this error.
        pack(buf);

    } catch (const std::exception &ex) {
        isc_throw(OutOfRange, "unable to obtain hexadecimal representation"
                  " of option " << getType() << ": " << ex.what());
    }
    const uint8_t* option_data = static_cast<const uint8_t*>(buf.getData());

    // Assign option data to a vector, with or without option header depending
    // on the value of "include_header" flag.
    std::vector<uint8_t> option_vec(option_data + (include_header ? 0 : getHeaderLen()),
                                    option_data + buf.getLength());
    return (option_vec);
}

std::string
Option::toHexString(const bool include_header) {
    // Prepare binary version of the option.
    std::vector<uint8_t> option_vec = toBinary(include_header);

    // Return hexadecimal representation prepended with 0x or empty string
    // if option has no payload and the header fields are excluded.
    std::ostringstream s;
    if (!option_vec.empty()) {
        s << "0x" << encode::encodeHex(option_vec);
    }
    return (s.str());
}

std::string
Option::headerToText(const int indent, const std::string& type_name) {
    std::stringstream output;
    for (int i = 0; i < indent; i++)
        output << " ";

    int field_len = (getUniverse() == V4 ? 3 : 5);
    output << "type=" << std::setw(field_len) << std::setfill('0')
           << type_;

    if (!type_name.empty()) {
        output << "(" << type_name << ")";
    }

    output << ", len=" << std::setw(field_len) << std::setfill('0')
           << len()-getHeaderLen();
    return (output.str());
}

std::string
Option::suboptionsToText(const int indent) const {
    std::stringstream output;

    if (!options_.empty()) {
        output << "," << std::endl << "options:";
        for (OptionCollection::const_iterator opt = options_.begin();
             opt != options_.end(); ++opt) {
            output << std::endl << (*opt).second->toText(indent);
        }
    }

    return (output.str());
}

uint16_t
Option::getHeaderLen() {
    switch (universe_) {
    case V4:
        return OPTION4_HDR_LEN; // header length for v4
    case V6:
        return OPTION6_HDR_LEN; // header length for v6
    }
    return 0; // should not happen
}

void Option::addOption(OptionPtr opt) {
    if (universe_ == V4) {
        // check for uniqueness (DHCPv4 options must be unique)
        if (getOption(opt->getType())) {
            isc_throw(BadValue, "Option " << opt->getType()
                      << " already present in this message.");
        }
    }
    options_.insert(make_pair(opt->getType(), opt));
}

uint8_t Option::getUint8() {
    if (data_.size() < sizeof(uint8_t) ) {
        isc_throw(OutOfRange, "Attempt to read uint8 from option " << type_
                  << " that has size " << data_.size());
    }
    return (data_[0]);
}

uint16_t Option::getUint16() {
    // readUint16() checks and throws OutOfRange if data_ is too small.
    return (readUint16(&data_[0], data_.size()));
}

uint32_t Option::getUint32() {
    // readUint32() checks and throws OutOfRange if data_ is too small.
    return (readUint32(&data_[0], data_.size()));
}

void Option::setUint8(uint8_t value) {
    data_.resize(sizeof(value));
    data_[0] = value;
}

void Option::setUint16(uint16_t value) {
    data_.resize(sizeof(value));
    writeUint16(value, &data_[0], data_.size());
}

void Option::setUint32(uint32_t value) {
    data_.resize(sizeof(value));
    writeUint32(value, &data_[0], data_.size());
}

bool Option::equals(const OptionPtr& other) const {
    return (equals(*other));
}

bool Option::equals(const Option& other) const {
    return ( (getType() == other.getType()) &&
             (getData() == other.getData()) );
}

Option::~Option() {

}

} // end of isc::dhcp namespace
} // end of isc namespace
