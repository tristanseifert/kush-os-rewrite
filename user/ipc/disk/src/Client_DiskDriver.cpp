/*
 * This RPC client stub was autogenerated by idlc. DO NOT EDIT!
 * Generated from DiskDriver.idl for interface DiskDriver at 2021-06-21T02:00:07-0500
 *
 * You may use these generated stubs directly as the RPC interface, or you can subclass it to
 * override the behavior of the function calls, or to perform some preprocessing to the data as
 * needed before sending it.
 *
 * See the full RPC documentation for more details.
 */
#include "Client_DiskDriver.hpp"
#include "DiskDriver.capnp.h"

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <capnp/message.h>
#include <capnp/serialize.h>

#include <rpc/rt/RpcIoStream.hpp>

using namespace rpc;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
using Client = DiskDriverClient;

/**
 * Creates a new client instance, with the given IO stream.
 */
Client::DiskDriverClient(const std::shared_ptr<IoStream> &stream) : io(stream) {
}

/**
 * Shuts down the RPC client, releasing any allocated resources.
 */
Client::~DiskDriverClient() {
    if(this->txBuf) {
        free(this->txBuf);
    }
}

/// Assemble a message with the given type.
uint32_t Client::_sendRequest(const uint64_t type, const std::span<std::byte> &payload) {
    const size_t len = sizeof(MessageHeader) + payload.size();
    this->_ensureTxBuf(len);

    const auto tag = __atomic_add_fetch(&this->nextTag, 1, __ATOMIC_RELAXED);
    auto hdr = reinterpret_cast<MessageHeader *>(this->txBuf);
    memset(hdr, 0, sizeof(*hdr));
    hdr->type = type;
    hdr->flags = MessageHeader::Flags::Request;
    hdr->tag = tag;

    memcpy(hdr->payload, payload.data(), payload.size());

    const std::span<std::byte> txBufSpan(reinterpret_cast<std::byte *>(this->txBuf), len);
    if(!this->io->sendRequest(txBufSpan)) {
        this->_HandleError(true, "Failed to send RPC request");
        return 0;
    }

    return tag;
}

// Allocates an aligned transmit buffer of the given size
void Client::_ensureTxBuf(const size_t len) {
    if(len > this->txBufSize) {
        if(this->txBuf) {
            free(this->txBuf);
        }
        int err = posix_memalign(&this->txBuf, 16, len);
        if(err) {
            return this->_HandleError(true, "Failed to allocate RPC send buffer");
        }
        this->txBufSize = len;
    }
}

/**
 * Handles an error that occurred on the client connection. Implementations may override this
 * method if they want to use exceptions, for example.
 *
 * @param fatal If set, the error precludes further operation on this RPC connection
 * @param what Descriptive string for the error
 */
void Client::_HandleError(const bool fatal, const std::string_view &what) {
    fprintf(stderr, "[RPC] %s: Encountered %s RPC error: %s\n", kServiceName.data(),
        fatal ? "fatal" : "recoverable", what.data());
    if(fatal) exit(-1);
}
/*
 * Autogenerated call method for 'GetCapacity' (id $91df49e5f38b0cb5)
 * Have 1 parameter(s), 3 return(s); method is sync
 */
Client::GetCapacityReturn Client::GetCapacity(uint64_t diskId) {
    uint32_t sentTag;
    {
        capnp::MallocMessageBuilder requestBuilder;
        auto request = requestBuilder.initRoot<rpc::_proto::messages::GetCapacityRequest>();
        request.setDiskId(diskId);
        auto rw = capnp::messageToFlatArray(requestBuilder);
        auto rb = rw.asBytes();
        const std::span<std::byte> rs(reinterpret_cast<std::byte *>(rb.begin()), rb.size());
        sentTag = this->_sendRequest(rpc::_proto::messages::MESSAGE_ID_GET_CAPACITY, rs);
    }
    {
        std::span<std::byte> buf;
        if(!this->io->receiveReply(buf)) this->_HandleError(false, "Failed to receive RPC reply");
        if(buf.size() < sizeof(MessageHeader)) this->_HandleError(false, "Received message too small");
        const auto hdr = reinterpret_cast<const MessageHeader *>(buf.data());
        if(hdr->tag != sentTag) this->_HandleError(false, "Invalid tag in reply RPC packet");
        else if(hdr->type != rpc::_proto::messages::MESSAGE_ID_GET_CAPACITY) this->_HandleError(false, "Invalid type in reply RPC packet");
        const auto payload = buf.subspan(offsetof(MessageHeader, payload));

        kj::ArrayPtr<const capnp::word> message(reinterpret_cast<const capnp::word *>(payload.data()),
                payload.size() / sizeof(capnp::word));
        capnp::FlatArrayMessageReader reader(message);
        auto reply = reader.getRoot<rpc::_proto::messages::GetCapacityResponse>();
        GetCapacityReturn r;
        r.status =  reply.getStatus();
        r.sectorSize =  reply.getSectorSize();
        r.numSectors =  reply.getNumSectors();
        return r;

    }
}
/*
 * Autogenerated call method for 'OpenSession' (id $f4e1aa89aee2c5f)
 * Have 0 parameter(s), 5 return(s); method is sync
 */
Client::OpenSessionReturn Client::OpenSession() {
    uint32_t sentTag;
    {
        capnp::MallocMessageBuilder requestBuilder;
        auto request = requestBuilder.initRoot<rpc::_proto::messages::OpenSessionRequest>();
        auto rw = capnp::messageToFlatArray(requestBuilder);
        auto rb = rw.asBytes();
        const std::span<std::byte> rs(reinterpret_cast<std::byte *>(rb.begin()), rb.size());
        sentTag = this->_sendRequest(rpc::_proto::messages::MESSAGE_ID_OPEN_SESSION, rs);
    }
    {
        std::span<std::byte> buf;
        if(!this->io->receiveReply(buf)) this->_HandleError(false, "Failed to receive RPC reply");
        if(buf.size() < sizeof(MessageHeader)) this->_HandleError(false, "Received message too small");
        const auto hdr = reinterpret_cast<const MessageHeader *>(buf.data());
        if(hdr->tag != sentTag) this->_HandleError(false, "Invalid tag in reply RPC packet");
        else if(hdr->type != rpc::_proto::messages::MESSAGE_ID_OPEN_SESSION) this->_HandleError(false, "Invalid type in reply RPC packet");
        const auto payload = buf.subspan(offsetof(MessageHeader, payload));

        kj::ArrayPtr<const capnp::word> message(reinterpret_cast<const capnp::word *>(payload.data()),
                payload.size() / sizeof(capnp::word));
        capnp::FlatArrayMessageReader reader(message);
        auto reply = reader.getRoot<rpc::_proto::messages::OpenSessionResponse>();
        OpenSessionReturn r;
        r.status =  reply.getStatus();
        r.sessionToken =  reply.getSessionToken();
        r.regionHandle =  reply.getRegionHandle();
        r.regionSize =  reply.getRegionSize();
        r.numCommands =  reply.getNumCommands();
        return r;

    }
}
/*
 * Autogenerated call method for 'CloseSession' (id $bdac3777974760fb)
 * Have 1 parameter(s), 1 return(s); method is sync
 */
int32_t Client::CloseSession(uint64_t session) {
    uint32_t sentTag;
    {
        capnp::MallocMessageBuilder requestBuilder;
        auto request = requestBuilder.initRoot<rpc::_proto::messages::CloseSessionRequest>();
        request.setSession(session);
        auto rw = capnp::messageToFlatArray(requestBuilder);
        auto rb = rw.asBytes();
        const std::span<std::byte> rs(reinterpret_cast<std::byte *>(rb.begin()), rb.size());
        sentTag = this->_sendRequest(rpc::_proto::messages::MESSAGE_ID_CLOSE_SESSION, rs);
    }
    {
        std::span<std::byte> buf;
        if(!this->io->receiveReply(buf)) this->_HandleError(false, "Failed to receive RPC reply");
        if(buf.size() < sizeof(MessageHeader)) this->_HandleError(false, "Received message too small");
        const auto hdr = reinterpret_cast<const MessageHeader *>(buf.data());
        if(hdr->tag != sentTag) this->_HandleError(false, "Invalid tag in reply RPC packet");
        else if(hdr->type != rpc::_proto::messages::MESSAGE_ID_CLOSE_SESSION) this->_HandleError(false, "Invalid type in reply RPC packet");
        const auto payload = buf.subspan(offsetof(MessageHeader, payload));

        kj::ArrayPtr<const capnp::word> message(reinterpret_cast<const capnp::word *>(payload.data()),
                payload.size() / sizeof(capnp::word));
        capnp::FlatArrayMessageReader reader(message);
        auto reply = reader.getRoot<rpc::_proto::messages::CloseSessionResponse>();
        return reply.getStatus();
    }
}
/*
 * Autogenerated call method for 'CreateReadBuffer' (id $5c63169ecba56263)
 * Have 2 parameter(s), 3 return(s); method is sync
 */
Client::CreateReadBufferReturn Client::CreateReadBuffer(uint64_t session, uint64_t requestedSize) {
    uint32_t sentTag;
    {
        capnp::MallocMessageBuilder requestBuilder;
        auto request = requestBuilder.initRoot<rpc::_proto::messages::CreateReadBufferRequest>();
        request.setSession(session);
        request.setRequestedSize(requestedSize);
        auto rw = capnp::messageToFlatArray(requestBuilder);
        auto rb = rw.asBytes();
        const std::span<std::byte> rs(reinterpret_cast<std::byte *>(rb.begin()), rb.size());
        sentTag = this->_sendRequest(rpc::_proto::messages::MESSAGE_ID_CREATE_READ_BUFFER, rs);
    }
    {
        std::span<std::byte> buf;
        if(!this->io->receiveReply(buf)) this->_HandleError(false, "Failed to receive RPC reply");
        if(buf.size() < sizeof(MessageHeader)) this->_HandleError(false, "Received message too small");
        const auto hdr = reinterpret_cast<const MessageHeader *>(buf.data());
        if(hdr->tag != sentTag) this->_HandleError(false, "Invalid tag in reply RPC packet");
        else if(hdr->type != rpc::_proto::messages::MESSAGE_ID_CREATE_READ_BUFFER) this->_HandleError(false, "Invalid type in reply RPC packet");
        const auto payload = buf.subspan(offsetof(MessageHeader, payload));

        kj::ArrayPtr<const capnp::word> message(reinterpret_cast<const capnp::word *>(payload.data()),
                payload.size() / sizeof(capnp::word));
        capnp::FlatArrayMessageReader reader(message);
        auto reply = reader.getRoot<rpc::_proto::messages::CreateReadBufferResponse>();
        CreateReadBufferReturn r;
        r.status =  reply.getStatus();
        r.readBufHandle =  reply.getReadBufHandle();
        r.readBufMaxSize =  reply.getReadBufMaxSize();
        return r;

    }
}
/*
 * Autogenerated call method for 'CreateWriteBuffer' (id $2647106809f005fc)
 * Have 2 parameter(s), 3 return(s); method is sync
 */
Client::CreateWriteBufferReturn Client::CreateWriteBuffer(uint64_t session, uint64_t requestedSize) {
    uint32_t sentTag;
    {
        capnp::MallocMessageBuilder requestBuilder;
        auto request = requestBuilder.initRoot<rpc::_proto::messages::CreateWriteBufferRequest>();
        request.setSession(session);
        request.setRequestedSize(requestedSize);
        auto rw = capnp::messageToFlatArray(requestBuilder);
        auto rb = rw.asBytes();
        const std::span<std::byte> rs(reinterpret_cast<std::byte *>(rb.begin()), rb.size());
        sentTag = this->_sendRequest(rpc::_proto::messages::MESSAGE_ID_CREATE_WRITE_BUFFER, rs);
    }
    {
        std::span<std::byte> buf;
        if(!this->io->receiveReply(buf)) this->_HandleError(false, "Failed to receive RPC reply");
        if(buf.size() < sizeof(MessageHeader)) this->_HandleError(false, "Received message too small");
        const auto hdr = reinterpret_cast<const MessageHeader *>(buf.data());
        if(hdr->tag != sentTag) this->_HandleError(false, "Invalid tag in reply RPC packet");
        else if(hdr->type != rpc::_proto::messages::MESSAGE_ID_CREATE_WRITE_BUFFER) this->_HandleError(false, "Invalid type in reply RPC packet");
        const auto payload = buf.subspan(offsetof(MessageHeader, payload));

        kj::ArrayPtr<const capnp::word> message(reinterpret_cast<const capnp::word *>(payload.data()),
                payload.size() / sizeof(capnp::word));
        capnp::FlatArrayMessageReader reader(message);
        auto reply = reader.getRoot<rpc::_proto::messages::CreateWriteBufferResponse>();
        CreateWriteBufferReturn r;
        r.status =  reply.getStatus();
        r.writeBufHandle =  reply.getWriteBufHandle();
        r.writeBufMaxSize =  reply.getWriteBufMaxSize();
        return r;

    }
}
/*
 * Autogenerated call method for 'ExecuteCommand' (id $aae5bbef0049e019)
 * Have 2 parameter(s), 0 return(s); method is async
 */
void Client::ExecuteCommand(uint64_t session, uint32_t slot) {
    {
        capnp::MallocMessageBuilder requestBuilder;
        auto request = requestBuilder.initRoot<rpc::_proto::messages::ExecuteCommandRequest>();
        request.setSession(session);
        request.setSlot(slot);
        auto rw = capnp::messageToFlatArray(requestBuilder);
        auto rb = rw.asBytes();
        const std::span<std::byte> rs(reinterpret_cast<std::byte *>(rb.begin()), rb.size());
        this->_sendRequest(rpc::_proto::messages::MESSAGE_ID_EXECUTE_COMMAND, rs);
    }
}
/*
 * Autogenerated call method for 'ReleaseReadCommand' (id $dcc360757768916a)
 * Have 2 parameter(s), 0 return(s); method is async
 */
void Client::ReleaseReadCommand(uint64_t session, uint32_t slot) {
    {
        capnp::MallocMessageBuilder requestBuilder;
        auto request = requestBuilder.initRoot<rpc::_proto::messages::ReleaseReadCommandRequest>();
        request.setSession(session);
        request.setSlot(slot);
        auto rw = capnp::messageToFlatArray(requestBuilder);
        auto rb = rw.asBytes();
        const std::span<std::byte> rs(reinterpret_cast<std::byte *>(rb.begin()), rb.size());
        this->_sendRequest(rpc::_proto::messages::MESSAGE_ID_RELEASE_READ_COMMAND, rs);
    }
}
/*
 * Autogenerated call method for 'AllocWriteMemory' (id $3dc1fae0d30f6af6)
 * Have 2 parameter(s), 3 return(s); method is sync
 */
Client::AllocWriteMemoryReturn Client::AllocWriteMemory(uint64_t session, uint64_t bytesRequested) {
    uint32_t sentTag;
    {
        capnp::MallocMessageBuilder requestBuilder;
        auto request = requestBuilder.initRoot<rpc::_proto::messages::AllocWriteMemoryRequest>();
        request.setSession(session);
        request.setBytesRequested(bytesRequested);
        auto rw = capnp::messageToFlatArray(requestBuilder);
        auto rb = rw.asBytes();
        const std::span<std::byte> rs(reinterpret_cast<std::byte *>(rb.begin()), rb.size());
        sentTag = this->_sendRequest(rpc::_proto::messages::MESSAGE_ID_ALLOC_WRITE_MEMORY, rs);
    }
    {
        std::span<std::byte> buf;
        if(!this->io->receiveReply(buf)) this->_HandleError(false, "Failed to receive RPC reply");
        if(buf.size() < sizeof(MessageHeader)) this->_HandleError(false, "Received message too small");
        const auto hdr = reinterpret_cast<const MessageHeader *>(buf.data());
        if(hdr->tag != sentTag) this->_HandleError(false, "Invalid tag in reply RPC packet");
        else if(hdr->type != rpc::_proto::messages::MESSAGE_ID_ALLOC_WRITE_MEMORY) this->_HandleError(false, "Invalid type in reply RPC packet");
        const auto payload = buf.subspan(offsetof(MessageHeader, payload));

        kj::ArrayPtr<const capnp::word> message(reinterpret_cast<const capnp::word *>(payload.data()),
                payload.size() / sizeof(capnp::word));
        capnp::FlatArrayMessageReader reader(message);
        auto reply = reader.getRoot<rpc::_proto::messages::AllocWriteMemoryResponse>();
        AllocWriteMemoryReturn r;
        r.status =  reply.getStatus();
        r.offset =  reply.getOffset();
        r.bytesAllocated =  reply.getBytesAllocated();
        return r;

    }
}
#pragma clang diagnostic pop