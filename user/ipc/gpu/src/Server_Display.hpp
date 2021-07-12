/*
 * This RPC server stub was autogenerated by idlc (version c8601c6e). DO NOT EDIT!
 * Generated from Display.idl for interface Display at 2021-07-08T01:00:48-0500
 *
 * You should subclass this implementation and define the required abstract methods to complete
 * implementing the interface. Note that there are several helper methods available to simplify
 * this task, or to retrieve more information about the caller.
 *
 * See the full RPC documentation for more details.
 */
#ifndef RPC_SERVER_GENERATED_10322778102778773913
#define RPC_SERVER_GENERATED_10322778102778773913

#include <string>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#define RPC_USER_TYPES_INCLUDES
#include <DriverSupport/gfx/Types.h>
#undef RPC_USER_TYPES_INCLUDES

namespace rpc {
namespace rt { class ServerRpcIoStream; }
class DisplayServer {
    struct MessageHeader {
        enum Flags: uint32_t {
            Request                     = (1 << 0),
            Response                    = (1 << 1),
        };

        uint64_t type;
        Flags flags;
        uint32_t tag;

        std::byte payload[];
    };
    static_assert(!(offsetof(MessageHeader, payload) % sizeof(uintptr_t)),
        "message header's payload is not word aligned");

    constexpr static const std::string_view kServiceName{"Display"};

    protected:
        using IoStream = rt::ServerRpcIoStream;
        // Return types for method 'GetDeviceCapabilities'
        struct GetDeviceCapabilitiesReturn {
            int32_t status;
            uint32_t caps;
        };
        // Return types for method 'GetFramebuffer'
        struct GetFramebufferReturn {
            int32_t status;
            uint64_t handle;
            uint64_t size;
        };
        // Return types for method 'GetFramebufferInfo'
        struct GetFramebufferInfoReturn {
            int32_t status;
            uint32_t w;
            uint32_t h;
            uint32_t pitch;
        };

    public:
        DisplayServer(const std::shared_ptr<IoStream> &stream);
        virtual ~DisplayServer();

        // Server's main loop; continuously read and handle messages.
        bool run(const bool block = true);
        // Process a single message.
        bool runOne(const bool block);

    // These are methods the implementation provides to complete implementation of the interface
    protected:
        virtual GetDeviceCapabilitiesReturn implGetDeviceCapabilities() = 0;
        virtual int32_t implSetOutputEnabled(bool enabled) = 0;
        virtual int32_t implSetOutputMode(const DriverSupport::gfx::DisplayMode &mode) = 0;
        virtual int32_t implRegionUpdated(int32_t x, int32_t y, uint32_t w, uint32_t h) = 0;
        virtual GetFramebufferReturn implGetFramebuffer() = 0;
        virtual GetFramebufferInfoReturn implGetFramebufferInfo() = 0;

    // Helpers provided to subclasses for implementation of interface methods
    protected:
        constexpr inline auto &getIo() {
            return this->io;
        }

        /// Handles errors occurring during server operations
        virtual void _HandleError(const bool fatal, const std::string_view &what);

    // Implementation details; pretend this does not exist
    private:
        std::shared_ptr<IoStream> io;
        size_t txBufSize{0};
        void *txBuf{nullptr};

        void _ensureTxBuf(const size_t);
        void _sendReply(const MessageHeader &, const size_t);

        void _marshallGetDeviceCapabilities(const MessageHeader &, const std::span<std::byte> &payload);
        void _marshallSetOutputEnabled(const MessageHeader &, const std::span<std::byte> &payload);
        void _marshallSetOutputMode(const MessageHeader &, const std::span<std::byte> &payload);
        void _marshallRegionUpdated(const MessageHeader &, const std::span<std::byte> &payload);
        void _marshallGetFramebuffer(const MessageHeader &, const std::span<std::byte> &payload);
        void _marshallGetFramebufferInfo(const MessageHeader &, const std::span<std::byte> &payload);
}; // class DisplayServer
} // namespace rpc
#endif // defined(RPC_SERVER_GENERATED_10322778102778773913)