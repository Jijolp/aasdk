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

#include <boost/test/unit_test.hpp>
#include <f1x/aasdk/Transport/UT/Transport.mock.hpp>
#include <f1x/aasdk/Messenger/UT/Cryptor.mock.hpp>
#include <f1x/aasdk/Messenger/UT/ReceivePromiseHandler.mock.hpp>
#include <f1x/aasdk/Messenger/Promise.hpp>
#include <f1x/aasdk/Messenger/MessageInStream.hpp>

namespace f1x
{
namespace aasdk
{
namespace messenger
{
namespace ut
{

using ::testing::_;
using ::testing::SaveArg;
using ::testing::SetArgReferee;
using ::testing::Return;

class MessageInStreamUnitTest
{
protected:
    MessageInStreamUnitTest()
        : transport_(&transportMock_, [](auto*) {})
        , cryptor_(&cryptorMock_, [](auto*) {})
        , receivePromise_(ReceivePromise::defer(ioService_))
    {
        receivePromise_->then(std::bind(&ReceivePromiseHandlerMock::onResolve, &receivePromiseHandlerMock_, std::placeholders::_1),
                             std::bind(&ReceivePromiseHandlerMock::onReject, &receivePromiseHandlerMock_, std::placeholders::_1));
    }

    boost::asio::io_context ioService_;
    transport::ut::TransportMock transportMock_;
    transport::ITransport::Pointer transport_;
    CryptorMock cryptorMock_;
    ICryptor::Pointer cryptor_;
    ReceivePromiseHandlerMock receivePromiseHandlerMock_;
    ReceivePromise::Pointer receivePromise_;
};


ACTION(ThrowSSLReadException)
{
    throw error::Error(error::ErrorCode::SSL_READ, 123);
}

BOOST_FIXTURE_TEST_CASE(MessageInStream_ReceivePlainMessage, MessageInStreamUnitTest)
{
    MessageInStream::Pointer messageInStream(std::make_shared<MessageInStream>(ioService_, transport_, cryptor_));

    FrameHeader frameHeader(ChannelId::BLUETOOTH, FrameType::BULK, EncryptionType::PLAIN, MessageType::SPECIFIC);
    transport::ITransport::ReceivePromise::Pointer frameHeaderTransportPromise;
    EXPECT_CALL(transportMock_, receive(FrameHeader::getSizeOf(), _)).WillOnce(SaveArg<1>(&frameHeaderTransportPromise));

    messageInStream->startReceive(std::move(receivePromise_));

    ioService_.run();
    ioService_.reset();

    common::Data framePayload(1000, 0x5E);
    FrameSize frameSize(framePayload.size());
    transport::ITransport::ReceivePromise::Pointer frameSizeTransportPromise;
    EXPECT_CALL(transportMock_, receive(FrameSize::getSizeOf(FrameSizeType::SHORT), _)).WillOnce(SaveArg<1>(&frameSizeTransportPromise));
    frameHeaderTransportPromise->resolve(frameHeader.getData());

    ioService_.run();
    ioService_.reset();

    transport::ITransport::ReceivePromise::Pointer framePayloadTransportPromise;
    EXPECT_CALL(transportMock_, receive(framePayload.size(), _)).WillOnce(SaveArg<1>(&framePayloadTransportPromise));
    frameSizeTransportPromise->resolve(frameSize.getData());

    ioService_.run();
    ioService_.reset();

    Message::Pointer message;
    EXPECT_CALL(receivePromiseHandlerMock_, onReject(_)).Times(0);
    EXPECT_CALL(receivePromiseHandlerMock_, onResolve(_)).WillOnce(SaveArg<0>(&message));
    framePayloadTransportPromise->resolve(framePayload);

    ioService_.run();

    BOOST_CHECK(message->getChannelId() == ChannelId::BLUETOOTH);
    BOOST_CHECK(message->getEncryptionType() == EncryptionType::PLAIN);
    BOOST_CHECK(message->getType() == MessageType::SPECIFIC);

    const auto& payload = message->getPayload();
    BOOST_CHECK_EQUAL_COLLECTIONS(payload.begin(), payload.end(), framePayload.begin(), framePayload.end());
}

BOOST_FIXTURE_TEST_CASE(MessageInStream_ReceiveEncryptedMessage, MessageInStreamUnitTest)
{
    MessageInStream::Pointer messageInStream(std::make_shared<MessageInStream>(ioService_, transport_, cryptor_));

    FrameHeader frameHeader(ChannelId::VIDEO, FrameType::BULK, EncryptionType::ENCRYPTED, MessageType::CONTROL);
    transport::ITransport::ReceivePromise::Pointer frameHeaderTransportPromise;
    EXPECT_CALL(transportMock_, receive(FrameHeader::getSizeOf(), _)).WillOnce(SaveArg<1>(&frameHeaderTransportPromise));

    messageInStream->startReceive(std::move(receivePromise_));

    ioService_.run();
    ioService_.reset();

    common::Data framePayload(1000, 0x5E);
    FrameSize frameSize(framePayload.size());
    transport::ITransport::ReceivePromise::Pointer frameSizeTransportPromise;
    EXPECT_CALL(transportMock_, receive(FrameSize::getSizeOf(FrameSizeType::SHORT), _)).WillOnce(SaveArg<1>(&frameSizeTransportPromise));
    frameHeaderTransportPromise->resolve(frameHeader.getData());

    ioService_.run();
    ioService_.reset();

    transport::ITransport::ReceivePromise::Promise::Pointer framePayloadTransportPromise;
    EXPECT_CALL(transportMock_, receive(framePayload.size(), _)).WillOnce(SaveArg<1>(&framePayloadTransportPromise));

    common::Data decryptedPayload(500, 0x5F);
    EXPECT_CALL(cryptorMock_, decrypt(_, _)).WillOnce(DoAll(SetArgReferee<0>(decryptedPayload), Return(decryptedPayload.size())));
    frameSizeTransportPromise->resolve(frameSize.getData());

    ioService_.run();
    ioService_.reset();

    Message::Pointer message;
    EXPECT_CALL(receivePromiseHandlerMock_, onReject(_)).Times(0);
    EXPECT_CALL(receivePromiseHandlerMock_, onResolve(_)).WillOnce(SaveArg<0>(&message));
    framePayloadTransportPromise->resolve(framePayload);

    ioService_.run();

    BOOST_CHECK(message->getChannelId() == ChannelId::VIDEO);
    BOOST_CHECK(message->getEncryptionType() == EncryptionType::ENCRYPTED);
    BOOST_CHECK(message->getType() == MessageType::CONTROL);

    const auto& payload = message->getPayload();
    BOOST_CHECK_EQUAL_COLLECTIONS(payload.begin(), payload.end(), decryptedPayload.begin(), decryptedPayload.end());
}

BOOST_FIXTURE_TEST_CASE(MessageInStream_MessageDecryptionFailed, MessageInStreamUnitTest)
{
    MessageInStream::Pointer messageInStream(std::make_shared<MessageInStream>(ioService_, transport_, cryptor_));

    FrameHeader frameHeader(ChannelId::VIDEO, FrameType::BULK, EncryptionType::ENCRYPTED, MessageType::CONTROL);
    transport::ITransport::ReceivePromise::Pointer frameHeaderTransportPromise;
    EXPECT_CALL(transportMock_, receive(FrameHeader::getSizeOf(), _)).WillOnce(SaveArg<1>(&frameHeaderTransportPromise));

    messageInStream->startReceive(std::move(receivePromise_));

    ioService_.run();
    ioService_.reset();

    common::Data framePayload(1000, 0x5E);
    FrameSize frameSize(framePayload.size());
    transport::ITransport::ReceivePromise::Pointer frameSizeTransportPromise;
    EXPECT_CALL(transportMock_, receive(FrameSize::getSizeOf(FrameSizeType::SHORT), _)).WillOnce(SaveArg<1>(&frameSizeTransportPromise));
    frameHeaderTransportPromise->resolve(frameHeader.getData());

    ioService_.run();
    ioService_.reset();

    transport::ITransport::ReceivePromise::Pointer framePayloadTransportPromise;
    EXPECT_CALL(transportMock_, receive(framePayload.size(), _)).WillOnce(SaveArg<1>(&framePayloadTransportPromise));

    common::Data decryptedPayload(500, 0x5F);
    EXPECT_CALL(cryptorMock_, decrypt(_, _)).WillOnce(ThrowSSLReadException());
    frameSizeTransportPromise->resolve(frameSize.getData());

    ioService_.run();
    ioService_.reset();

    EXPECT_CALL(receivePromiseHandlerMock_, onReject(error::Error(error::ErrorCode::SSL_READ, 123)));
    EXPECT_CALL(receivePromiseHandlerMock_, onResolve(_)).Times(0);
    framePayloadTransportPromise->resolve(framePayload);

    ioService_.run();
}

BOOST_FIXTURE_TEST_CASE(MessageInStream_FramePayloadReceiveFailed, MessageInStreamUnitTest)
{
    MessageInStream::Pointer messageInStream(std::make_shared<MessageInStream>(ioService_, transport_, cryptor_));

    FrameHeader frameHeader(ChannelId::BLUETOOTH, FrameType::BULK, EncryptionType::PLAIN, MessageType::SPECIFIC);
    transport::ITransport::ReceivePromise::Pointer frameHeaderTransportPromise;
    EXPECT_CALL(transportMock_, receive(FrameHeader::getSizeOf(), _)).WillOnce(SaveArg<1>(&frameHeaderTransportPromise));

    messageInStream->startReceive(std::move(receivePromise_));

    ioService_.run();
    ioService_.reset();

    common::Data framePayload(1000, 0x5E);
    FrameSize frameSize(framePayload.size());
    transport::ITransport::ReceivePromise::Pointer frameSizeTransportPromise;
    EXPECT_CALL(transportMock_, receive(FrameSize::getSizeOf(FrameSizeType::SHORT), _)).WillOnce(SaveArg<1>(&frameSizeTransportPromise));
    frameHeaderTransportPromise->resolve(frameHeader.getData());

    ioService_.run();
    ioService_.reset();

    transport::ITransport::ReceivePromise::Pointer framePayloadTransportPromise;
    EXPECT_CALL(transportMock_, receive(framePayload.size(), _)).WillOnce(SaveArg<1>(&framePayloadTransportPromise));
    frameSizeTransportPromise->resolve(frameSize.getData());

    ioService_.run();
    ioService_.reset();

    error::Error e(error::ErrorCode::USB_TRANSFER, 5);
    EXPECT_CALL(receivePromiseHandlerMock_, onReject(e));
    EXPECT_CALL(receivePromiseHandlerMock_, onResolve(_)).Times(0);
    framePayloadTransportPromise->reject(e);

    ioService_.run();
}

BOOST_FIXTURE_TEST_CASE(MessageInStream_FramePayloadSizeReceiveFailed, MessageInStreamUnitTest)
{
    MessageInStream::Pointer messageInStream(std::make_shared<MessageInStream>(ioService_, transport_, cryptor_));

    FrameHeader frameHeader(ChannelId::BLUETOOTH, FrameType::BULK, EncryptionType::PLAIN, MessageType::SPECIFIC);
    transport::ITransport::ReceivePromise::Pointer frameHeaderTransportPromise;
    EXPECT_CALL(transportMock_, receive(FrameHeader::getSizeOf(), _)).WillOnce(SaveArg<1>(&frameHeaderTransportPromise));

    messageInStream->startReceive(std::move(receivePromise_));

    ioService_.run();
    ioService_.reset();

    transport::ITransport::ReceivePromise::Pointer frameSizeTransportPromise;
    EXPECT_CALL(transportMock_, receive(FrameSize::getSizeOf(FrameSizeType::SHORT), _)).WillOnce(SaveArg<1>(&frameSizeTransportPromise));
    frameHeaderTransportPromise->resolve(frameHeader.getData());

    ioService_.run();
    ioService_.reset();

    error::Error e(error::ErrorCode::USB_TRANSFER, 5);
    EXPECT_CALL(receivePromiseHandlerMock_, onReject(e));
    EXPECT_CALL(receivePromiseHandlerMock_, onResolve(_)).Times(0);
    frameSizeTransportPromise->reject(e);

    ioService_.run();
}

BOOST_FIXTURE_TEST_CASE(MessageInStream_FrameHeaderReceiveFailed, MessageInStreamUnitTest)
{
    MessageInStream::Pointer messageInStream(std::make_shared<MessageInStream>(ioService_, transport_, cryptor_));

    FrameHeader frameHeader(ChannelId::BLUETOOTH, FrameType::BULK, EncryptionType::PLAIN, MessageType::SPECIFIC);
    transport::ITransport::ReceivePromise::Pointer frameHeaderTransportPromise;
    EXPECT_CALL(transportMock_, receive(FrameHeader::getSizeOf(), _)).WillOnce(SaveArg<1>(&frameHeaderTransportPromise));

    messageInStream->startReceive(std::move(receivePromise_));

    ioService_.run();
    ioService_.reset();

    error::Error e(error::ErrorCode::USB_TRANSFER, 5);
    EXPECT_CALL(receivePromiseHandlerMock_, onReject(e));
    EXPECT_CALL(receivePromiseHandlerMock_, onResolve(_)).Times(0);
    frameHeaderTransportPromise->reject(e);

    ioService_.run();
}

BOOST_FIXTURE_TEST_CASE(MessageInStream_ReceiveSplittedMessage, MessageInStreamUnitTest)
{
    MessageInStream::Pointer messageInStream(std::make_shared<MessageInStream>(ioService_, transport_, cryptor_));
    FrameHeader frame1Header(ChannelId::BLUETOOTH, FrameType::FIRST, EncryptionType::PLAIN, MessageType::SPECIFIC);

    transport::ITransport::ReceivePromise::Pointer frameHeaderTransportPromise;
    EXPECT_CALL(transportMock_, receive(FrameHeader::getSizeOf(), _)).Times(2).WillRepeatedly(SaveArg<1>(&frameHeaderTransportPromise));

    messageInStream->startReceive(std::move(receivePromise_));

    ioService_.run();
    ioService_.reset();

    common::Data frame1Payload(1000, 0x5E);
    common::Data frame2Payload(2000, 0x5F);
    common::Data expectedPayload(frame1Payload.begin(), frame1Payload.end());
    expectedPayload.insert(expectedPayload.end(), frame2Payload.begin(), frame2Payload.end());

    transport::ITransport::ReceivePromise::Pointer frame1SizeTransportPromise;
    EXPECT_CALL(transportMock_, receive(FrameSize::getSizeOf(FrameSizeType::EXTENDED), _)).WillOnce(SaveArg<1>(&frame1SizeTransportPromise));
    frameHeaderTransportPromise->resolve(frame1Header.getData());

    ioService_.run();
    ioService_.reset();

    transport::ITransport::ReceivePromise::Pointer frame1PayloadTransportPromise;
    EXPECT_CALL(transportMock_, receive(frame1Payload.size(), _)).WillOnce(SaveArg<1>(&frame1PayloadTransportPromise));
    FrameSize frame1Size(frame1Payload.size(), frame1Payload.size() + frame2Payload.size());
    frame1SizeTransportPromise->resolve(frame1Size.getData());

    ioService_.run();
    ioService_.reset();

    frame1PayloadTransportPromise->resolve(frame1Payload);

    ioService_.run();
    ioService_.reset();

    transport::ITransport::ReceivePromise::Pointer frame2SizeTransportPromise;
    EXPECT_CALL(transportMock_, receive(FrameSize::getSizeOf(FrameSizeType::SHORT), _)).WillOnce(SaveArg<1>(&frame2SizeTransportPromise));

    FrameHeader frame2Header(ChannelId::BLUETOOTH, FrameType::LAST, EncryptionType::PLAIN, MessageType::SPECIFIC);
    frameHeaderTransportPromise->resolve(frame2Header.getData());

    ioService_.run();
    ioService_.reset();

    transport::ITransport::ReceivePromise::Pointer frame2PayloadTransportPromise;
    EXPECT_CALL(transportMock_, receive(frame2Payload.size(), _)).WillOnce(SaveArg<1>(&frame2PayloadTransportPromise));
    FrameSize frame2Size(frame2Payload.size());
    frame2SizeTransportPromise->resolve(frame2Size.getData());

    ioService_.run();
    ioService_.reset();

    Message::Pointer message;
    EXPECT_CALL(receivePromiseHandlerMock_, onReject(_)).Times(0);
    EXPECT_CALL(receivePromiseHandlerMock_, onResolve(_)).WillOnce(SaveArg<0>(&message));
    frame2PayloadTransportPromise->resolve(frame2Payload);

    ioService_.run();

    BOOST_CHECK(message->getChannelId() == ChannelId::BLUETOOTH);
    BOOST_CHECK(message->getEncryptionType() == EncryptionType::PLAIN);
    BOOST_CHECK(message->getType() == MessageType::SPECIFIC);

    const auto& payload = message->getPayload();
    BOOST_CHECK_EQUAL_COLLECTIONS(payload.begin(), payload.end(), expectedPayload.begin(), expectedPayload.end());
}

BOOST_FIXTURE_TEST_CASE(MessageInStream_InterleavedChannelsResolve, MessageInStreamUnitTest)
{
    // §47: recent phones (AAP 1.7+) interleave channels on the wire. A
    // BLUETOOTH FIRST frame interrupted by a complete VIDEO message must
    // NOT kill the stream: VIDEO resolves at once, BLUETOOTH completes
    // on the next receive with its full payload intact.
    // NOTE: header receives and SHORT size receives are both 2 bytes, so
    // every transport EXPECT is declared WillOnce immediately before the
    // resolve that triggers it — at most one receive(2) expectation is
    // ever active, no gmock matching ambiguity.
    MessageInStream::Pointer messageInStream(std::make_shared<MessageInStream>(ioService_, transport_, cryptor_));

    common::Data frame1Payload(1000, 0x5E);
    common::Data frame2Payload(2000, 0x5F);
    common::Data videoPayload(500, 0x5A);

    transport::ITransport::ReceivePromise::Pointer frameHeaderTransportPromise;
    EXPECT_CALL(transportMock_, receive(FrameHeader::getSizeOf(), _)).WillOnce(SaveArg<1>(&frameHeaderTransportPromise));
    messageInStream->startReceive(std::move(receivePromise_));

    ioService_.run();
    ioService_.reset();

    transport::ITransport::ReceivePromise::Pointer frame1SizeTransportPromise;
    EXPECT_CALL(transportMock_, receive(FrameSize::getSizeOf(FrameSizeType::EXTENDED), _)).WillOnce(SaveArg<1>(&frame1SizeTransportPromise));
    FrameHeader frame1Header(ChannelId::BLUETOOTH, FrameType::FIRST, EncryptionType::PLAIN, MessageType::SPECIFIC);
    frameHeaderTransportPromise->resolve(frame1Header.getData());

    ioService_.run();
    ioService_.reset();

    transport::ITransport::ReceivePromise::Pointer frame1PayloadTransportPromise;
    EXPECT_CALL(transportMock_, receive(frame1Payload.size(), _)).WillOnce(SaveArg<1>(&frame1PayloadTransportPromise));
    FrameSize frame1Size(frame1Payload.size(), frame1Payload.size() + frame2Payload.size());
    frame1SizeTransportPromise->resolve(frame1Size.getData());

    ioService_.run();
    ioService_.reset();

    // Next header receive armed BEFORE resolving the payload that
    // triggers it, so it is the only active receive(2) expectation.
    EXPECT_CALL(transportMock_, receive(FrameHeader::getSizeOf(), _)).WillOnce(SaveArg<1>(&frameHeaderTransportPromise));
    frame1PayloadTransportPromise->resolve(frame1Payload);

    ioService_.run();
    ioService_.reset();

    // Interrupting VIDEO single-frame message resolves immediately —
    // no reject, stream stays alive, BLUETOOTH partial survives.
    FrameHeader videoHeader(ChannelId::VIDEO, FrameType::LAST, EncryptionType::PLAIN, MessageType::SPECIFIC);

    transport::ITransport::ReceivePromise::Pointer videoSizeTransportPromise;
    EXPECT_CALL(transportMock_, receive(FrameSize::getSizeOf(FrameSizeType::SHORT), _)).WillOnce(SaveArg<1>(&videoSizeTransportPromise));
    frameHeaderTransportPromise->resolve(videoHeader.getData());

    ioService_.run();
    ioService_.reset();

    transport::ITransport::ReceivePromise::Pointer videoPayloadTransportPromise;
    EXPECT_CALL(transportMock_, receive(videoPayload.size(), _)).WillOnce(SaveArg<1>(&videoPayloadTransportPromise));
    FrameSize videoSize(videoPayload.size());
    videoSizeTransportPromise->resolve(videoSize.getData());

    ioService_.run();
    ioService_.reset();

    Message::Pointer videoMessage;
    EXPECT_CALL(receivePromiseHandlerMock_, onReject(_)).Times(0);
    EXPECT_CALL(receivePromiseHandlerMock_, onResolve(_)).WillOnce(SaveArg<0>(&videoMessage));
    videoPayloadTransportPromise->resolve(videoPayload);

    ioService_.run();
    ioService_.reset();

    BOOST_CHECK(videoMessage->getChannelId() == ChannelId::VIDEO);
    const auto& videoBytes = videoMessage->getPayload();
    BOOST_CHECK_EQUAL_COLLECTIONS(videoBytes.begin(), videoBytes.end(), videoPayload.begin(), videoPayload.end());

    // BLUETOOTH partial survived the interruption: complete it on a
    // fresh receive with the full concatenated payload.
    ReceivePromiseHandlerMock secondReceivePromiseHandlerMock;
    auto secondReceivePromise = ReceivePromise::defer(ioService_);
    secondReceivePromise->then(std::bind(&ReceivePromiseHandlerMock::onResolve, &secondReceivePromiseHandlerMock, std::placeholders::_1),
                               std::bind(&ReceivePromiseHandlerMock::onReject, &secondReceivePromiseHandlerMock, std::placeholders::_1));
    EXPECT_CALL(transportMock_, receive(FrameHeader::getSizeOf(), _)).WillOnce(SaveArg<1>(&frameHeaderTransportPromise));
    messageInStream->startReceive(std::move(secondReceivePromise));

    ioService_.run();
    ioService_.reset();

    FrameHeader frame2Header(ChannelId::BLUETOOTH, FrameType::LAST, EncryptionType::PLAIN, MessageType::SPECIFIC);

    transport::ITransport::ReceivePromise::Pointer frame2SizeTransportPromise;
    EXPECT_CALL(transportMock_, receive(FrameSize::getSizeOf(FrameSizeType::SHORT), _)).WillOnce(SaveArg<1>(&frame2SizeTransportPromise));
    frameHeaderTransportPromise->resolve(frame2Header.getData());

    ioService_.run();
    ioService_.reset();

    transport::ITransport::ReceivePromise::Pointer frame2PayloadTransportPromise;
    EXPECT_CALL(transportMock_, receive(frame2Payload.size(), _)).WillOnce(SaveArg<1>(&frame2PayloadTransportPromise));
    FrameSize frame2Size(frame2Payload.size());
    frame2SizeTransportPromise->resolve(frame2Size.getData());

    ioService_.run();
    ioService_.reset();

    common::Data expectedPayload(frame1Payload.begin(), frame1Payload.end());
    expectedPayload.insert(expectedPayload.end(), frame2Payload.begin(), frame2Payload.end());

    Message::Pointer message;
    EXPECT_CALL(secondReceivePromiseHandlerMock, onReject(_)).Times(0);
    EXPECT_CALL(secondReceivePromiseHandlerMock, onResolve(_)).WillOnce(SaveArg<0>(&message));
    frame2PayloadTransportPromise->resolve(frame2Payload);

    ioService_.run();

    BOOST_CHECK(message->getChannelId() == ChannelId::BLUETOOTH);
    const auto& payload = message->getPayload();
    BOOST_CHECK_EQUAL_COLLECTIONS(payload.begin(), payload.end(), expectedPayload.begin(), expectedPayload.end());
}

BOOST_FIXTURE_TEST_CASE(MessageInStream_RejectWhenInProgress, MessageInStreamUnitTest)
{
    MessageInStream::Pointer messageInStream(std::make_shared<MessageInStream>(ioService_, transport_, cryptor_));

    FrameHeader frameHeader(ChannelId::BLUETOOTH, FrameType::BULK, EncryptionType::PLAIN, MessageType::SPECIFIC);
    transport::ITransport::ReceivePromise::Pointer frameHeaderTransportPromise;
    EXPECT_CALL(transportMock_, receive(FrameHeader::getSizeOf(), _)).WillOnce(SaveArg<1>(&frameHeaderTransportPromise));

    messageInStream->startReceive(std::move(receivePromise_));

    ReceivePromiseHandlerMock secondReceivePromiseHandlerMock;
    auto secondReceivePromise = ReceivePromise::defer(ioService_);

    secondReceivePromise->then(std::bind(&ReceivePromiseHandlerMock::onResolve, &secondReceivePromiseHandlerMock, std::placeholders::_1),
                              std::bind(&ReceivePromiseHandlerMock::onReject, &secondReceivePromiseHandlerMock, std::placeholders::_1));

    EXPECT_CALL(secondReceivePromiseHandlerMock, onReject(error::Error(error::ErrorCode::OPERATION_IN_PROGRESS)));
    EXPECT_CALL(secondReceivePromiseHandlerMock, onResolve(_)).Times(0);
    messageInStream->startReceive(std::move(secondReceivePromise));
    ioService_.run();
}


}
}
}
}
