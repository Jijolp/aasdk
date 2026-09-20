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

#include <cstdio>
#include <aasdk_proto/InputSourceChannelMessageIdsEnum.pb.h>
#include <aasdk_proto/ControlMessageIdsEnum.pb.h>
#include <f1x/aasdk/Channel/InputSource/InputSourceChannel.hpp>
#include <f1x/aasdk/Channel/InputSource/IInputSourceChannelEventHandler.hpp>
#include <f1x/aasdk/Common/Log.hpp>

namespace f1x
{
namespace aasdk
{
namespace channel
{
namespace inputsource
{

InputSourceChannel::InputSourceChannel(boost::asio::io_context::strand& strand, messenger::IMessenger::Pointer messenger)
    : ServiceChannel(strand, std::move(messenger), messenger::ChannelId::INPUT_SOURCE)
{

}

void InputSourceChannel::receive(IInputSourceChannelEventHandler::Pointer eventHandler)
{
    auto receivePromise = messenger::ReceivePromise::defer(strand_);
    receivePromise->then(std::bind(&InputSourceChannel::messageHandler, this->shared_from_this(), std::placeholders::_1, eventHandler),
                        std::bind(&IInputSourceChannelEventHandler::onChannelError, eventHandler, std::placeholders::_1));

    messenger_->enqueueReceive(channelId_, std::move(receivePromise));
}

messenger::ChannelId InputSourceChannel::getId() const
{
    return channelId_;
}

void InputSourceChannel::sendInputReport(const proto::inputsource::InputReport& report, SendPromise::Pointer promise)
{
    auto message(std::make_shared<messenger::Message>(channelId_, messenger::EncryptionType::ENCRYPTED, messenger::MessageType::SPECIFIC));
    message->insertPayload(messenger::MessageId(proto::ids::InputSourceChannelMessage::INPUT_REPORT).getData());
    message->insertPayload(report);

    this->send(std::move(message), std::move(promise));
}

void InputSourceChannel::sendKeyBindingResponse(const proto::messages::KeyBindingResponse& response, SendPromise::Pointer promise)
{
    auto message(std::make_shared<messenger::Message>(channelId_, messenger::EncryptionType::ENCRYPTED, messenger::MessageType::SPECIFIC));
    message->insertPayload(messenger::MessageId(proto::ids::InputSourceChannelMessage::KEY_BINDING_RESPONSE).getData());
    message->insertPayload(response);

    this->send(std::move(message), std::move(promise));
}

void InputSourceChannel::sendChannelOpenResponse(const proto::messages::ChannelOpenResponse& response, SendPromise::Pointer promise)
{
    auto message(std::make_shared<messenger::Message>(channelId_, messenger::EncryptionType::ENCRYPTED, messenger::MessageType::CONTROL));
    message->insertPayload(messenger::MessageId(proto::ids::ControlMessage::CHANNEL_OPEN_RESPONSE).getData());
    message->insertPayload(response);

    this->send(std::move(message), std::move(promise));
}

void InputSourceChannel::messageHandler(messenger::Message::Pointer message, IInputSourceChannelEventHandler::Pointer eventHandler)
{
    messenger::MessageId messageId(message->getPayload());
    common::DataConstBuffer payload(message->getPayload(), messageId.getSizeOf());

    switch(messageId.getId())
    {
    case proto::ids::InputSourceChannelMessage::KEY_BINDING_REQUEST:
        this->handleKeyBindingRequest(payload, std::move(eventHandler));
        break;
    case proto::ids::ControlMessage::CHANNEL_OPEN_REQUEST:
        this->handleChannelOpenRequest(payload, std::move(eventHandler));
        break;
    default:
    {
        // S3 diagnostic: dump unhandled payloads (the phone sends 0x8001 at
        // 1 Hz on this channel). Revisit/remove once decoded.
        std::string hex;
        char byte[4];
        for(size_t i = 0; i < payload.size && i < 16; ++i)
        {
            std::snprintf(byte, sizeof(byte), "%02x", payload.cdata[i]);
            hex += byte;
        }
        AASDK_LOG(error) << "[InputSourceChannel] message not handled: " << messageId.getId()
                         << ", size: " << payload.size << ", head: " << hex;
        this->receive(std::move(eventHandler));
        break;
    }
    }
}

void InputSourceChannel::handleKeyBindingRequest(const common::DataConstBuffer& payload, IInputSourceChannelEventHandler::Pointer eventHandler)
{
    proto::messages::KeyBindingRequest request;
    if(request.ParseFromArray(payload.cdata, payload.size))
    {
        eventHandler->onKeyBindingRequest(request);
    }
    else
    {
        eventHandler->onChannelError(error::Error(error::ErrorCode::PARSE_PAYLOAD));
    }
}

void InputSourceChannel::handleChannelOpenRequest(const common::DataConstBuffer& payload, IInputSourceChannelEventHandler::Pointer eventHandler)
{
    proto::messages::ChannelOpenRequest request;
    if(request.ParseFromArray(payload.cdata, payload.size))
    {
        eventHandler->onChannelOpenRequest(request);
    }
    else
    {
        eventHandler->onChannelError(error::Error(error::ErrorCode::PARSE_PAYLOAD));
    }
}

}
}
}
}
