#include "rpc/dispensary.h"
#include "sys/infopage.h"
#include "sys/syscalls.h"

#include "helpers/Send.h"

#include <malloc.h>
#include <threads.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <span>
#include <vector>

#include <cista/serialization.h>
#include <rpc/RpcPacket.hpp>
#include <rpc/RootSrvDispensaryEndpoint.hpp>

using namespace rpc;

/// Memory to allocate for the message receive struct
constexpr static const size_t kMaxMsgLen = 512 + sizeof(struct MessageHeader);

/// Lock to protect the port from concurrent use
static mtx_t gLookupReplyPortLock;
/// Flag to ensure the receive port for lookups is only created once.
static once_flag gLookupReplyPortCreatedFlag;
/// Port handle for service lookups
static uintptr_t gLookupReplyPort = 0;

/// Receive buffer for replies from the port
static void *gRxBuffer = nullptr;

/**
 * Performs one-time initialization of the lookup machinery.
 *
 * This entails allocating a port to receive replies on, and a lock to ensure only one thread will
 * go through this code at once. Lookups will entail a context switch anyways so the performance
 * impact of this isn't too great. Handles should be cached whenever possible.
 */
static void InitDispensary() {
    int err;

    // create the port
    err = PortCreate(&gLookupReplyPort);
    assert(!err);

    // and the lock
    err = mtx_init(&gLookupReplyPortLock, mtx_plain);
    assert(err == thrd_success);

    // then, allocate the receive buffer
    err = posix_memalign(&gRxBuffer, 16, kMaxMsgLen);
    assert(!err);

    memset(gRxBuffer, 0, kMaxMsgLen);
}

/**
 * Attempts to resolve a name into a port.
 *
 * All RPC requests will block forever. This is in theory not a problem, assuming the root server
 * never goes away...
 *
 * @param name Service name to look up; this is a zero-terminated UTF-8 string.
 * @param outPort If a port is found, its handle is written to this variable.
 *
 * @return 0 if the request was completed, but the port was not found; 1 if the port was found; or
 * a negative error code.
 */
int LookupService(const char * _Nonnull _name, uintptr_t * _Nonnull outPort) {\
    int err;
    std::vector<uint8_t> buf;

    // fail if no dispensary port
    if(!__kush_infopg->dispensaryPort) {
        return -420;
    }

    // create the port for replies, if needed
    call_once(&gLookupReplyPortCreatedFlag, []{
        InitDispensary();
    });

    // serialize request
    const std::string name(_name);
    RootSrvDispensaryLookup req;
    req.name = name;

    buf = cista::serialize(req);

    // send it
    err = mtx_lock(&gLookupReplyPortLock);
    if(err != thrd_success) {
        // cannot goto fail since the lock isn't locked
        return err;
    }

    err = _RpcSend(__kush_infopg->dispensaryPort,
            static_cast<uint32_t>(RootSrvDispensaryEpType::Lookup), buf, gLookupReplyPort);
    if(err) {
        goto fail;
    }

    // receive the reply
    err = PortReceive(gLookupReplyPort, reinterpret_cast<struct MessageHeader *>(gRxBuffer),
            kMaxMsgLen, UINTPTR_MAX);

    if(err < 0) {
        goto fail;
    }

    // interpret the reply
    {
        const auto rxMsg = reinterpret_cast<struct MessageHeader *>(gRxBuffer);

        // deserialize the request
        auto packet = reinterpret_cast<RpcPacket *>(rxMsg->data);

        auto data = std::span(packet->payload, std::min(err - sizeof(RpcPacket), (unsigned int) err));
        auto reply = cista::deserialize<RootSrvDispensaryLookupReply>(data);

        // make sure we got the right reply message
        if(reply->name != name) {
            err = -1;
            goto fail;
        }

        // extract handle
        if(!reply->status) {
            err = 1;
            *outPort = reply->port;
        } else {
            err = 0;
            *outPort = 0;
        }
    }

    // clean up
    mtx_unlock(&gLookupReplyPortLock);
    return err;

fail:;
    // failure handler; ensures we release memory and unlock again
    mtx_unlock(&gLookupReplyPortLock);
    return err;
}

/**
 * Registers a named service.
 *
 * @param name Service name to register under; this is a zero-terminated UTF-8 string.
 * @param port Port handle to register
 *
 * @return 0 if request was completed and port registered successfully; an error code otherwise.
 */
int RegisterService(const char * _Nonnull _name, const uintptr_t port) {
    int err;
    std::vector<uint8_t> buf;

    // fail if no dispensary port
    if(!__kush_infopg->dispensaryPort) {
        return -420;
    }
    // create the port for replies, if needed
    call_once(&gLookupReplyPortCreatedFlag, []{
        InitDispensary();
    });

    // serialize request
    const std::string name(_name);
    RootSrvDispensaryRegister req;
    req.name = name;
    req.portHandle = port;

    buf = cista::serialize(req);

    // send it
    err = mtx_lock(&gLookupReplyPortLock);
    if(err != thrd_success) {
        // cannot goto fail since the lock isn't locked
        return err;
    }

    err = _RpcSend(__kush_infopg->dispensaryPort,
            static_cast<uint32_t>(RootSrvDispensaryEpType::Register), buf, gLookupReplyPort);
    if(err) {
        goto fail;
    }

    // receive the reply
    err = PortReceive(gLookupReplyPort, reinterpret_cast<struct MessageHeader *>(gRxBuffer),
            kMaxMsgLen, UINTPTR_MAX);

    if(err < 0) {
        goto fail;
    }

    // interpret the reply
    {
        const auto rxMsg = reinterpret_cast<struct MessageHeader *>(gRxBuffer);

        // deserialize the request
        auto packet = reinterpret_cast<RpcPacket *>(rxMsg->data);

        auto data = std::span(packet->payload, std::min(err - sizeof(RpcPacket), (unsigned int) err));
        auto reply = cista::deserialize<RootSrvDispensaryRegisterReply>(data);

        // make sure we got the right reply message
        if(reply->name != name) {
            err = -1;
            goto fail;
        }

        // extract handle
        // XXX: do we care about the "did overwrite" flag?
        err = reply->status;
    }

    // clean up
    mtx_unlock(&gLookupReplyPortLock);

    return err;

fail:;
    // failure handler; ensures we release memory and unlock again
    mtx_unlock(&gLookupReplyPortLock);
    return err;
}
