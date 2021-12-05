/* This file is part of the the dci project. Copyright (C) 2013-2021 vopl, shtoba.
   This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public
   License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
   You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>. */

#include "pch.hpp"
#include "endpoint2Address.hpp"

namespace dci::module::ppn::transport::net
{
    apit::Address endpoint2Address(const idl::net::Endpoint& ep)
    {
        apit::Address res;

        if(ep.holds<idl::net::NullEndpoint>())
        {
            res.value = "null://";
            return res;
        }
        else if(ep.holds<idl::net::Ip4Endpoint>())
        {
            const idl::net::Ip4Endpoint& ep4 = ep.get<idl::net::Ip4Endpoint>();
            res.value = "tcp4://" + utils::net::ip::toString(ep4.address.octets, ep4.port);
            return res;
        }
        else if(ep.holds<idl::net::Ip6Endpoint>())
        {
            const idl::net::Ip6Endpoint& ep6 = ep.get<idl::net::Ip6Endpoint>();
            res.value = "tcp6://" + utils::net::ip::toString(ep6.address.octets, ep6.address.linkId, ep6.port);
            return res;
        }
        else if(ep.holds<idl::net::LocalEndpoint>())
        {
            const idl::net::LocalEndpoint& epl = ep.get<idl::net::LocalEndpoint>();
            res.value = "local://" + epl.address.substr(1);
        }
        else
        {
            dbgFatal("never here");
        }

        return res;
    }
}
