/*
*  This file is part of aasdk library project.
*  Copyright (C) 2018 f1x.studio (Michal Szwaj)
*
*  aasdk is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 3 of the License, or
*  (at your option) any later version.

*  aasdk is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with aasdk. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <memory>
#include <f1x/aasdk/Error/Error.hpp>
#include <aasdk_proto/KeyBindingRequestMessage.pb.h>
#include <aasdk_proto/ChannelOpenRequestMessage.pb.h>

namespace f1x
{
namespace aasdk
{
namespace channel
{
namespace inputsource
{

class IInputSourceChannelEventHandler
{
public:
    typedef std::shared_ptr<IInputSourceChannelEventHandler> Pointer;

    IInputSourceChannelEventHandler() = default;
    virtual ~IInputSourceChannelEventHandler() = default;

    virtual void onChannelOpenRequest(const proto::messages::ChannelOpenRequest& request) = 0;
    virtual void onKeyBindingRequest(const proto::messages::KeyBindingRequest& request) = 0;
    virtual void onChannelError(const error::Error& e) = 0;
};

}
}
}
}
