/* This file is part of the the dci project. Copyright (C) 2013-2023 vopl, shtoba.
   This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public
   License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
   You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>. */

#include "pch.hpp"
#include "address2Endpoint.hpp"

namespace dci::module::ppn::transport::net
{
    /////////0/////////1/////////2/////////3/////////4/////////5/////////6/////////7
    idl::net::Endpoint address2Endpoint(idl::net::Host<>& host, const apit::Address& target)
    {
        utils::URI<> uri;
        if(!utils::uri::parse(target.value, uri))
            throw api::BadAddress(target.value);

        return std::visit([&]<class Alt>(const Alt& alt)
                          {
                              idl::net::Endpoint ep {};

                              if constexpr(std::is_same_v<utils::uri::TCP<>, Alt>)
                              {
                                  idl::net::IpEndpoint epIp = host->resolveIp(utils::uri::hostPort(alt)).value();
                                  if(epIp.holds<idl::net::Ip4Endpoint>())
                                      ep = epIp.get<idl::net::Ip4Endpoint>();
                                  else if(epIp.holds<idl::net::Ip6Endpoint>())
                                      ep = epIp.get<idl::net::Ip6Endpoint>();
                              }
                              else if constexpr(std::is_same_v<utils::uri::TCP4<>, Alt>)
                                  ep = host->resolveIp4(utils::uri::hostPort(alt)).value();
                              else if constexpr(std::is_same_v<utils::uri::TCP6<>, Alt>)
                                  ep = host->resolveIp6(utils::uri::hostPort(alt)).value();
                              else if constexpr(std::is_same_v<utils::uri::Local<>, Alt>)
                              {
                                  std::string authority = utils::uri::hostPort(alt);
                                  if(!authority.empty())
                                  {
                                      authority.insert(0, 1, '\0');
                                      ep = idl::net::LocalEndpoint{std::move(authority)};
                                  }
                                  else
                                      ep = idl::net::LocalEndpoint{};
                              }
                              else
                                  throw api::BadAddress(target.value);

                              return ep;
                          }, uri);
    }
}
