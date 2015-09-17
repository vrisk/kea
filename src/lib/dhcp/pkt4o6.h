// Copyright (C) 2015 Internet Systems Consortium, Inc. ("ISC")
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#ifndef PKT4O6_H
#define PKT4O6_H

#include <dhcp/pkt4.h>
#include <dhcp/pkt6.h>

#include <boost/shared_ptr.hpp>

namespace isc {

namespace dhcp {

/// @brief Represents DHCPv4-over-DHCPv6 packet
///
/// This class derives from @c Pkt4 in order to be handled by
/// the DHCPv4 server code. It includes a shared pointer to the
/// DHCPv6 message too.
class Pkt4o6 : public Pkt4 {
public:

    /// @brief Constructor, used in message reception.
    ///
    /// @param data pointer to received data
    /// @param len size of buffer to be allocated for this packet
    /// @param pkt6 encapsulating DHCPv6 message.
    Pkt4o6(const uint8_t* data, size_t len, const Pkt6Ptr& pkt6);

    /// @brief Returns encapsulating DHCPv6 message
    const Pkt6Ptr& getPkt6() { return (pkt6_); }

protected:
    /// Encapsulating DHCPv6 message
    const Pkt6Ptr& pkt6_;

}; // Pkt4o6 class

/// @brief A pointer to Pkt4o6 object.
typedef boost::shared_ptr<Pkt4o6> Pkt4o6Ptr;

} // isc::dhcp namespace

} // isc namespace

#endif