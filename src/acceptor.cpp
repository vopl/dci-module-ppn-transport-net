/* This file is part of the the dci project. Copyright (C) 2013-2023 vopl, shtoba.
   This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public
   License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
   You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>. */

#include "pch.hpp"
#include "acceptor.hpp"
#include "channel.hpp"
#include "address2Endpoint.hpp"
#include "endpoint2Address.hpp"
#include <dci/utils/net/url.hpp>

namespace dci::module::ppn::transport::net
{
    /////////0/////////1/////////2/////////3/////////4/////////5/////////6/////////7
    Acceptor::Acceptor(host::Manager* hostManager)
        : apit::net::Acceptor<>::Opposite(idl::interface::Initializer())
        , _hostManager(hostManager)
    {
        //in address() -> transport::Address;
        methods()->address() += sol() * [this]
        {
            return cmt::readyFuture(_boundAddress);
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
            if(_started)
            {
                return cmt::readyFuture<None>(exception::buildInstance<api::AlreadyBound>("unable to bind after acceptor started"));
            }

            auto scheme = utils::net::url::scheme(address.value);
            using namespace std::literals;
            if("local"sv != scheme &&
               "tcp4"sv  != scheme &&
               "tcp6"sv  != scheme &&
               "tcp"sv   != scheme)
            {
                return cmt::readyFuture<None>(exception::buildInstance<api::BadAddress>(address.value));
            }

            _bindAddress = std::move(address);
            return cmt::readyFuture(None{});
        };

        //in start();
        methods()->start() += sol() * [this]
        {
            if(_started)
            {
                return;
            }

            _started = true;

            cmt::spawn() += _tow * [this]()
            {
                try
                {
                    idl::net::Host<> host = _hostManager->createService<idl::net::Host<>>().value();

                    _netStreamServer = host->streamServer().value();

                    _netStreamServer->accepted() += _sow * [this](idl::net::stream::Channel<>&& netStreamChannel)
                    {
                        netStreamChannel->setOption(idl::net::option::NoDelay{true});

                        Channel* impl = new Channel(apit::Address{}, std::move(netStreamChannel));
                        impl->involvedChanged() += impl * [impl](bool v)
                        {
                            if(!v)
                            {
                                delete impl;
                            }
                        };

                        methods()->accepted(impl->opposite());
                    };

                    _netStreamServer->failed() += _sow * [this](ExceptionPtr&& e)
                    {
                        methods()->failed(_bindAddress, _boundAddress, std::move(e));
                    };

                    _netStreamServer->closed() += _sow * [this]()
                    {
                        if(_listenDeclared)
                        {
                            _listenDeclared = false;
                            methods()->stopped(_bindAddress, _boundAddress);
                        }
                    };

                    _netStreamServer->setOption(idl::net::option::ReuseAddr{true}).value();

                    _netStreamServer->listen(address2Endpoint(host, _bindAddress)).value();
                    _boundAddress = endpoint2Address(_netStreamServer->localEndpoint().value());
                    methods()->addressChanged(_boundAddress);

                    _listenDeclared = true;
                    methods()->started(_bindAddress, _boundAddress);
                }
                catch(cmt::task::Stop&)
                {
                    _started = false;
                    _sow.flush();

                    if(_netStreamServer)
                    {
                        _netStreamServer->close();
                        _netStreamServer.reset();
                    }

                    if(_listenDeclared)
                    {
                        _listenDeclared = false;
                        methods()->stopped(_bindAddress, _boundAddress);
                    }

                    return;
                }
                catch(...)
                {
                    methods()->failed(_bindAddress, _boundAddress, std::current_exception());
                    if(_listenDeclared)
                    {
                        _listenDeclared = false;
                        methods()->stopped(_bindAddress, _boundAddress);
                    }
                    return;
                }
            };
        };

        //in stop();
        methods()->stop() += sol() * [this]
        {
            _started = false;

            _sow.flush();
            _tow.flush();

            if(_netStreamServer)
            {
                _netStreamServer->close();
                _netStreamServer.reset();
            }

            if(_listenDeclared)
            {
                _listenDeclared = false;
                methods()->stopped(_bindAddress, _boundAddress);
            }
        };
    }

    /////////0/////////1/////////2/////////3/////////4/////////5/////////6/////////7
    Acceptor::~Acceptor()
    {
        _sow.flush();
        _tow.stop();

        if(_netStreamServer)
        {
            _netStreamServer->close();
            _netStreamServer.reset();
        }

        if(_listenDeclared)
        {
            _listenDeclared = false;
            methods()->stopped(_bindAddress, _boundAddress);
        }
    }
}
