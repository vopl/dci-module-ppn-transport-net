/* This file is part of the the dci project. Copyright (C) 2013-2021 vopl, shtoba.
   This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public
   License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
   You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>. */

#include "pch.hpp"
#include "connector.hpp"
#include "acceptor.hpp"

namespace dci::module::ppn::transport::net
{
    namespace
    {
        struct Manifest
            : public dci::host::module::Manifest
        {
            Manifest()
            {
                _valid = true;
                _name = dciModuleName;
                _mainBinary = dciUnitTargetFile;

                pushServiceId<api::Connector>();
                pushServiceId<api::Acceptor>();
            }
        } manifest_;


        struct Entry
            : public dci::host::module::Entry
        {
            const Manifest& manifest() override
            {
                return manifest_;
            }

            cmt::Future<idl::Interface> createService(idl::ILid ilid) override
            {
                if(auto s = tryCreateService<Connector>(ilid, manager())) return cmt::readyFuture(s);
                if(auto s = tryCreateService<Acceptor>(ilid, manager())) return cmt::readyFuture(s);

                return dci::host::module::Entry::createService(ilid);
            }
        } entry_;
    }
}

extern "C"
{
    DCI_INTEGRATION_APIDECL_EXPORT dci::host::module::Entry* dciModuleEntry = &dci::module::ppn::transport::net::entry_;
}
