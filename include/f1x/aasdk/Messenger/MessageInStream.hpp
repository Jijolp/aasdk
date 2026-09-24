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

#include <map>
#include <f1x/aasdk/Transport/ITransport.hpp>
#include <boost/noncopyable.hpp>
#include <f1x/aasdk/Messenger/IMessageInStream.hpp>
#include <f1x/aasdk/Messenger/ICryptor.hpp>
#include <f1x/aasdk/Messenger/FrameHeader.hpp>
#include <f1x/aasdk/Messenger/FrameSize.hpp>

namespace f1x
{
namespace aasdk
{
namespace messenger
{

class MessageInStream: public IMessageInStream, public std::enable_shared_from_this<MessageInStream>, boost::noncopyable
{
public:
    MessageInStream(boost::asio::io_context& ioService, transport::ITransport::Pointer transport, ICryptor::Pointer cryptor);

    void startReceive(ReceivePromise::Pointer promise) override;

private:
    using std::enable_shared_from_this<MessageInStream>::shared_from_this;

    void receiveFrameHeaderHandler(const common::DataConstBuffer& buffer);
    void receiveFrameSizeHandler(const common::DataConstBuffer& buffer, int channelId);
    void receiveFramePayloadHandler(const common::DataConstBuffer& buffer, int channelId);

    boost::asio::io_context::strand strand_;
    transport::ITransport::Pointer transport_;
    ICryptor::Pointer cryptor_;
    ReceivePromise::Pointer promise_;

    // Per-channel reassembly (§47): recent phones (AAP 1.7+) interleave
    // frames of several channels on the wire (every frame carries its
    // channel id precisely so the HU can demultiplex). The old code kept
    // a single global partial message and killed the whole session with
    // MESSENGER_INTERTWINED_CHANNELS as soon as a second channel spoke
    // mid-message — fatal whenever the phone is used normally (music +
    // notifications/sensors). Each channel now accumulates its own
    // partial message + last frame type; completed messages resolve in
    // arrival order through the single stream promise. All runs on the
    // strand, strictly sequential: at most one transport receive is
    // outstanding, so no races on this map.
    struct PartialMessage
    {
        Message::Pointer message;
        FrameType recentFrameType = FrameType::BULK;
    };
    std::map<int, PartialMessage> partials_;
};

}
}
}
