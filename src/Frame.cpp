// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/Frame.h"
#include <folly/Memory.h>
#include <folly/Optional.h>
#include <folly/io/Cursor.h>
#include <bitset>
#include "src/ConnectionSetupPayload.h"

namespace reactivesocket {

std::unique_ptr<folly::IOBuf> FrameBufferAllocator::allocate(size_t size) {
  // Purposely leak the allocator, since it's hard to deterministically
  // guarantee that threads will stop using it before it would get statically
  // destructed.
  static auto* singleton = new FrameBufferAllocator;
  return singleton->allocateBuffer(size);
}

std::unique_ptr<folly::IOBuf> FrameBufferAllocator::allocateBuffer(
    size_t size) {
  return folly::IOBuf::createCombined(size);
}

std::string to_string(FrameType type) {
  switch (type) {
    case FrameType::REQUEST_STREAM:
      return "REQUEST_STREAM";
    case FrameType::REQUEST_CHANNEL:
      return "REQUEST_CHANNEL";
    case FrameType::REQUEST_N:
      return "REQUEST_N";
    case FrameType::REQUEST_RESPONSE:
      return "REQUEST_RESPONSE";
    case FrameType::REQUEST_FNF:
      return "REQUEST_FNF";
    case FrameType::CANCEL:
      return "CANCEL";
    case FrameType::PAYLOAD:
      return "PAYLOAD";
    case FrameType::ERROR:
      return "ERROR";
    case FrameType::RESERVED:
      return "RESERVED";
    case FrameType::KEEPALIVE:
      return "KEEPALIVE";
    case FrameType::SETUP:
      return "SETUP";
    case FrameType::LEASE:
      return "LEASE";
    case FrameType::METADATA_PUSH:
      return "METADATA_PUSH";
    case FrameType::RESUME:
      return "RESUME";
    case FrameType::RESUME_OK:
      return "RESUME_OK";
    case FrameType::EXT:
      return "EXT";
  }
  // this should be never hit because the switch is over all cases
  LOG(FATAL) << "unknown FrameType=" << static_cast<int>(type);
}

std::ostream& operator<<(std::ostream& os, FrameType type) {
  return os << to_string(type);
}

std::ostream& operator<<(std::ostream& os, ErrorCode errorCode) {
  switch (errorCode) {
    case ErrorCode::RESERVED:
      return os << "RESERVED";
    case ErrorCode::APPLICATION_ERROR:
      return os << "APPLICATION_ERROR";
    case ErrorCode::REJECTED:
      return os << "REJECTED";
    case ErrorCode::CANCELED:
      return os << "CANCELED";
    case ErrorCode::INVALID:
      return os << "INVALID";
    case ErrorCode::INVALID_SETUP:
      return os << "INVALID_SETUP";
    case ErrorCode::REJECTED_SETUP:
      return os << "REJECTED_SETUP";
    case ErrorCode::UNSUPPORTED_SETUP:
      return os << "UNSUPPORTED_SETUP";
    case ErrorCode::CONNECTION_ERROR:
      return os << "CONNECTION_ERROR";
  }
  return os << "ErrorCode(" << static_cast<uint32_t>(errorCode) << ")";
}

std::ostream& operator<<(std::ostream& os, FrameFlags frameFlags) {
  std::bitset<16> flags(static_cast<uint16_t>(frameFlags));
  return os << flags;
}

std::ostream& operator<<(std::ostream& os, const FrameHeader& header) {
  return os << header.type_ << "[" << header.flags_ << ", " << header.streamId_
            << "]";
}
/// @}

std::ostream& operator<<(std::ostream& os, const Frame_REQUEST_Base& frame) {
  return os << frame.header_ << "(" << frame.requestN_ << ", "
            << frame.payload_;
}

std::ostream& operator<<(std::ostream& os, const Frame_REQUEST_N& frame) {
  return os << frame.header_ << "(" << frame.requestN_ << ")";
}

std::ostream& operator<<(
    std::ostream& os,
    const Frame_REQUEST_RESPONSE& frame) {
  return os << frame.header_ << ", " << frame.payload_;
}

std::ostream& operator<<(std::ostream& os, const Frame_REQUEST_FNF& frame) {
  return os << frame.header_ << ", " << frame.payload_;
}

std::ostream& operator<<(std::ostream& os, const Frame_METADATA_PUSH& frame) {
  return os << frame.header_ << ", "
            << (frame.metadata_ ? frame.metadata_->computeChainDataLength()
                                : 0);
}

std::ostream& operator<<(std::ostream& os, const Frame_CANCEL& frame) {
  return os << frame.header_;
}

Frame_PAYLOAD Frame_PAYLOAD::complete(StreamId streamId) {
  return Frame_PAYLOAD(streamId, FrameFlags::COMPLETE, Payload());
}

std::ostream& operator<<(std::ostream& os, const Frame_PAYLOAD& frame) {
  return os << frame.header_ << ", (" << frame.payload_;
}

Frame_ERROR Frame_ERROR::unexpectedFrame() {
  return Frame_ERROR(
      0, ErrorCode::CONNECTION_ERROR, Payload("unexpected frame"));
}

Frame_ERROR Frame_ERROR::invalidFrame() {
  return Frame_ERROR(0, ErrorCode::CONNECTION_ERROR, Payload("invalid frame"));
}

Frame_ERROR Frame_ERROR::badSetupFrame(const std::string& message) {
  return Frame_ERROR(0, ErrorCode::INVALID_SETUP, Payload(message));
}

Frame_ERROR Frame_ERROR::connectionError(const std::string& message) {
  return Frame_ERROR(0, ErrorCode::CONNECTION_ERROR, Payload(message));
}

Frame_ERROR Frame_ERROR::invalid(
    StreamId streamId,
    const std::string& message) {
  DCHECK(streamId) << "streamId MUST be non-0";
  return Frame_ERROR(streamId, ErrorCode::INVALID, Payload(message));
}

Frame_ERROR Frame_ERROR::applicationError(
    StreamId streamId,
    const std::string& message) {
  DCHECK(streamId) << "streamId MUST be non-0";
  return Frame_ERROR(streamId, ErrorCode::APPLICATION_ERROR, Payload(message));
}

std::ostream& operator<<(std::ostream& os, const Frame_ERROR& frame) {
  return os << frame.header_ << ", " << frame.errorCode_ << ", "
            << frame.payload_;
}

std::ostream& operator<<(std::ostream& os, const Frame_KEEPALIVE& frame) {
  return os << frame.header_ << "(<"
            << (frame.data_ ? frame.data_->computeChainDataLength() : 0)
            << ">)";
}

std::ostream& operator<<(std::ostream& os, const Frame_SETUP& frame) {
  return os << frame.header_ << ", (" << frame.payload_;
}

void Frame_SETUP::moveToSetupPayload(ConnectionSetupPayload& setupPayload) {
  setupPayload.metadataMimeType = std::move(metadataMimeType_);
  setupPayload.dataMimeType = std::move(dataMimeType_);
  setupPayload.payload = std::move(payload_);
  setupPayload.token = std::move(token_);
  setupPayload.resumable = !!(header_.flags_ & FrameFlags::RESUME_ENABLE);
}

std::ostream& operator<<(std::ostream& os, const Frame_LEASE& frame) {
  return os << frame.header_ << ", ("
            << (frame.metadata_ ? frame.metadata_->computeChainDataLength() : 0)
            << ")";
}

std::ostream& operator<<(std::ostream& os, const Frame_RESUME& frame) {
  return os << frame.header_ << ", ("
            << "token"
            << ", @server " << frame.lastReceivedServerPosition_ << ", @client "
            << frame.clientPosition_ << ")";
}

std::ostream& operator<<(std::ostream& os, const Frame_RESUME_OK& frame) {
  return os << frame.header_ << ", (@" << frame.position_ << ")";
}
} // reactivesocket
