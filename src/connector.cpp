/* This file is part of the the dci project. Copyright (C) 2013-2023 vopl, shtoba.
   This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public
   License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
   You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>. */

#include "pch.hpp"
#include "connector.hpp"
#include "channel.hpp"
#include "address2Endpoint.hpp"

namespace dci::module::ppn::transport::net
{
    /////////0/////////1/////////2/////////3/////////4/////////5/////////6/////////7
    Connector::Connector(host::Manager* hostManager)
        : apit::net::Connector<>::Opposite(idl::interface::Initializer())
        , _hostManager(hostManager)
    {
        //in address() -> transport::Address;
        methods()->address() += sol() * [this]
        {
            return cmt::readyFuture(_address);
        };

        //in cost() -> real64;
        methods()->cost() += sol() * []
        {
            return cmt::readyFuture(real64{0});
        };

        //in rtt() -> real64;
        methods()->rtt() += sol() * []
        {
            return cmt::readyFuture(real64{0});
        };

        //in bandwidth() -> real64;
        methods()->bandwidth() += sol() * []
        {
            return cmt::readyFuture(std::numeric_limits<real64>::max());
        };

        //in bind(Address) -> void;
        methods()->bind() += sol() * [this](apit::Address&& address)
        {
            auto scheme = utils::uri::scheme(address.value);
            using namespace std::literals;
            if("local"sv != scheme &&
               "tcp4"sv  != scheme &&
               "tcp6"sv  != scheme &&
               "tcp"sv   != scheme)
            {
                return cmt::readyFuture<None>(exception::buildInstance<api::BadAddress>(address.value));
            }

            return cmt::spawnv() += _tol * [this, address](cmt::Promise<None>& out)
            {
                try
                {
                    _netStreamClient.value()->bind(address2Endpoint(_netHost.value(), address));

                    _address = std::move(address);
                    methods()->addressChanged(_address);

                    out.resolveValue(None{});
                }
                catch(const cmt::task::Stop&)
                {
                    if(!out.resolved())
                    {
                        out.resolveCancel();
                    }
                }
                catch(...)
                {
                    if(!out.resolved())
                    {
                        out.resolveException(std::current_exception());
                    }
                }
            };
        };

        //in connect(Address) -> Channel;
        methods()->connect() += sol() * [this](const apit::Address& address)
        {
            return cmt::spawnv() += _tol * [this, address](cmt::Promise<apit::Channel<>>& out)
            {
                cmt::task::currentTask().stopOnResolvedCancel(out);//остановить этот воркер по отмене результата

                try
                {
                    cmt::Future<idl::net::stream::Channel<>> netStreamChannelFuture = _netStreamClient.value()->connect(address2Endpoint(_netHost.value(), address));
                    poll::WaitableTimer deadline{std::chrono::seconds{2}};
                    deadline.start();

                    if(0 == cmt::waitAny(deadline.waitable(), netStreamChannelFuture.waitable()))
                    {
                        netStreamChannelFuture.resolveCancel();
                        if(!out.resolved())
                        {
                            out.resolveException(exception::buildInstance<api::ConnectionTimeout>());
                        }
                    }
                    else
                    {
                        idl::net::stream::Channel<> netStreamChannel = netStreamChannelFuture.value();
                        netStreamChannel->setOption(idl::net::option::NoDelay{true});

                        if(!out.resolved())
                        {
                            Channel* impl = new Channel(apit::Address{address}, std::move(netStreamChannel));
                            impl->involvedChanged() += impl * [impl](bool v)
                            {
                                if(!v)
                                {
                                    delete impl;
                                }
                            };

                            out.resolveValue(impl->opposite());
                        }
                    }
                }
                catch(const cmt::task::Stop&)
                {
                    //empty is ok
                    if(!out.resolved())
                    {
                        out.resolveCancel();
                    }
                }
                catch(...)
                {
                    if(!out.resolved())
                    {
                        out.resolveException(std::current_exception());
                    }
                }
            };
        };

        _netHost = _hostManager->createService<idl::net::Host<>>();

        _netStreamClient = _netHost.apply(sol(), [](cmt::Future<idl::net::Host<>> in, cmt::Promise<idl::net::stream::Client<>> out)
        {
            in.value()->streamClient().then() += [out=std::move(out)](auto in) mutable
            {
                out.resolveAs(in);
            };
        });
    }

    /////////0/////////1/////////2/////////3/////////4/////////5/////////6/////////7
    Connector::~Connector()
    {
        sol().flush();
        _tol.stop();
    }
}
