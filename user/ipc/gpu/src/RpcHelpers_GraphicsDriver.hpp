/*
 * This RPC serialization code was autogenerated by idlc (version c8601c6e). DO NOT EDIT!
 * Generated from GraphicsDriver.idl for interface GraphicsDriver at 2021-07-07T18:30:29-0500
 *
 * The structs and methods within are used by the RPC system to serialize and deserialize the
 * arguments and return values on method calls. They work internally in the same way that encoding
 * custom types in RPC messages works.
 *
 * See the full RPC documentation for more details.
 */
#ifndef RPC_HELPERS_GENERATED_438003604959851420
#define RPC_HELPERS_GENERATED_438003604959851420

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <span>
#include <string_view>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"

#define RPC_USER_TYPES_INCLUDES
#include <DriverSupport/gfx/Types.h>
#undef RPC_USER_TYPES_INCLUDES


namespace rpc {
/**
 * Method to handle a failure to deserialize a field; this will log the failure if built in debug
 * mode.
 */
static inline void HandleDecodeError(const char *typeName, const char *fieldName,
    const uintptr_t offset) {
    fprintf(stderr, "[RPC] Decode error for type %s, field %s at offset $%x\n", typeName, fieldName,
        offset);
}
static inline void HandleDecodeError(const char *typeName, const char *fieldName,
    const uintptr_t offset, const uint32_t blobDataOffset, const uint32_t blobSz) {
    fprintf(stderr, "[RPC] Decode error for type %s, field %s at offset $%x "
        "(blob offset $%x, $%x bytes)\n", typeName, fieldName, offset, blobDataOffset, blobSz);
}

/*
 * Serialization functions for various C++ built in types
 */
inline size_t bytesFor(const std::string &s) {
    return s.length();
}
inline bool serialize(std::span<std::byte> &out, const std::string &str) {
    if(str.empty()) return true;
    else if(out.size() < str.length()) return false;
    memcpy(out.data(), str.c_str(), str.length());
    return true;
}
inline bool deserialize(const std::span<std::byte> &in, std::string &outStr) {
    if(in.empty()) {
        outStr = "";
    } else {
        outStr = std::string(reinterpret_cast<const char *>(in.data()), in.size());
    }
    return true;
}

// XXX: this only works for POD types!
template<typename T>
inline size_t bytesFor(const std::vector<T> &s) {
    return s.size() * sizeof(T);
}
template<typename T>
inline bool serialize(std::span<std::byte> &out, const std::vector<T> &vec) {
    const size_t numBytes = vec.size() * sizeof(T);
    if(out.size() < numBytes) return false;
    memcpy(out.data(), vec.data(), out.size());
    return true;
}
template <typename T>
inline bool deserialize(const std::span<std::byte> &in, std::vector<T> &outVec) {
    const size_t elements = in.size() / sizeof(T);
    outVec.resize(elements);
    memcpy(outVec.data(), in.data(), in.size());
    return true;
}


/*
 * Definitions of serialization structures for messages and message replies. These use the
 * autogenerated stubs to convert to/from the wire format.
 */
namespace internals {
/// Message ids for each of the RPC messages
enum class Type: uint64_t {
                               GetDeviceCapabilities = 0xb3be16a171616697ULL,
                                       GetNumOutputs = 0x5da3ac5140d7492dULL,
                                    SetOutputEnabled = 0xd3ddaaa17cd66af0ULL,
                                       SetOutputMode = 0xf472a05edc874b12ULL,
};
/**
 * Request structure for method 'GetDeviceCapabilities'
 */
struct GetDeviceCapabilitiesRequest {

    constexpr static const size_t kElementSizes[0] {
    
    };
    constexpr static const size_t kElementOffsets[0] {
    
    };
    constexpr static const size_t kScalarBytes{0};
    constexpr static const size_t kBlobStartOffset{0};
};
/**
 * Reply structure for method 'GetDeviceCapabilities'
 */
struct GetDeviceCapabilitiesResponse {
    int32_t status;
    uint32_t caps;

    constexpr static const size_t kElementSizes[2] {
     4,  4
    };
    constexpr static const size_t kElementOffsets[2] {
     0,  4
    };
    constexpr static const size_t kScalarBytes{8};
    constexpr static const size_t kBlobStartOffset{8};
};

/**
 * Request structure for method 'GetNumOutputs'
 */
struct GetNumOutputsRequest {

    constexpr static const size_t kElementSizes[0] {
    
    };
    constexpr static const size_t kElementOffsets[0] {
    
    };
    constexpr static const size_t kScalarBytes{0};
    constexpr static const size_t kBlobStartOffset{0};
};
/**
 * Reply structure for method 'GetNumOutputs'
 */
struct GetNumOutputsResponse {
    int32_t status;
    uint32_t count;

    constexpr static const size_t kElementSizes[2] {
     4,  4
    };
    constexpr static const size_t kElementOffsets[2] {
     0,  4
    };
    constexpr static const size_t kScalarBytes{8};
    constexpr static const size_t kBlobStartOffset{8};
};

/**
 * Request structure for method 'SetOutputEnabled'
 */
struct SetOutputEnabledRequest {
    uint32_t displayId;
    bool enabled;

    constexpr static const size_t kElementSizes[2] {
     4,  1
    };
    constexpr static const size_t kElementOffsets[2] {
     0,  4
    };
    constexpr static const size_t kScalarBytes{5};
    constexpr static const size_t kBlobStartOffset{8};
};
/**
 * Reply structure for method 'SetOutputEnabled'
 */
struct SetOutputEnabledResponse {
    int32_t status;

    constexpr static const size_t kElementSizes[1] {
     4
    };
    constexpr static const size_t kElementOffsets[1] {
     0
    };
    constexpr static const size_t kScalarBytes{4};
    constexpr static const size_t kBlobStartOffset{8};
};

/**
 * Request structure for method 'SetOutputMode'
 */
struct SetOutputModeRequest {
    uint32_t displayId;
    DriverSupport::gfx::DisplayMode mode;

    constexpr static const size_t kElementSizes[2] {
     4,  8
    };
    constexpr static const size_t kElementOffsets[2] {
     0,  4
    };
    constexpr static const size_t kScalarBytes{12};
    constexpr static const size_t kBlobStartOffset{16};
};
/**
 * Reply structure for method 'SetOutputMode'
 */
struct SetOutputModeResponse {
    int32_t status;

    constexpr static const size_t kElementSizes[1] {
     4
    };
    constexpr static const size_t kElementOffsets[1] {
     0
    };
    constexpr static const size_t kScalarBytes{4};
    constexpr static const size_t kBlobStartOffset{8};
};

} // namespace rpc::internals


inline size_t bytesFor(const internals::GetDeviceCapabilitiesRequest &x) {
    using namespace internals;
    size_t len = GetDeviceCapabilitiesRequest::kBlobStartOffset;

    return len;
}
inline bool serialize(std::span<std::byte> &out, const internals::GetDeviceCapabilitiesRequest &x) {
    using namespace internals;
    uint32_t blobOff = GetDeviceCapabilitiesRequest::kBlobStartOffset;

    return true;
}
inline bool deserialize(const std::span<std::byte> &in, internals::GetDeviceCapabilitiesRequest &x) {
    using namespace internals;
    if(in.size() < GetDeviceCapabilitiesRequest::kScalarBytes) return false;
    const auto blobRegion = in.subspan(GetDeviceCapabilitiesRequest::kBlobStartOffset);

    return true;
}

inline size_t bytesFor(const internals::GetDeviceCapabilitiesResponse &x) {
    using namespace internals;
    size_t len = GetDeviceCapabilitiesResponse::kBlobStartOffset;

    return len;
}
inline bool serialize(std::span<std::byte> &out, const internals::GetDeviceCapabilitiesResponse &x) {
    using namespace internals;
    uint32_t blobOff = GetDeviceCapabilitiesResponse::kBlobStartOffset;
    {
        const auto off = GetDeviceCapabilitiesResponse::kElementOffsets[0];
        const auto size = GetDeviceCapabilitiesResponse::kElementSizes[0];
        auto range = out.subspan(off, size);
        memcpy(range.data(), &x.status, range.size());
    }
    {
        const auto off = GetDeviceCapabilitiesResponse::kElementOffsets[1];
        const auto size = GetDeviceCapabilitiesResponse::kElementSizes[1];
        auto range = out.subspan(off, size);
        memcpy(range.data(), &x.caps, range.size());
    }

    return true;
}
inline bool deserialize(const std::span<std::byte> &in, internals::GetDeviceCapabilitiesResponse &x) {
    using namespace internals;
    if(in.size() < GetDeviceCapabilitiesResponse::kScalarBytes) return false;
    const auto blobRegion = in.subspan(GetDeviceCapabilitiesResponse::kBlobStartOffset);
    {
        const auto off = GetDeviceCapabilitiesResponse::kElementOffsets[0];
        const auto size = GetDeviceCapabilitiesResponse::kElementSizes[0];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        memcpy(&x.status, range.data(), range.size());
    }
    {
        const auto off = GetDeviceCapabilitiesResponse::kElementOffsets[1];
        const auto size = GetDeviceCapabilitiesResponse::kElementSizes[1];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        memcpy(&x.caps, range.data(), range.size());
    }

    return true;
}

inline size_t bytesFor(const internals::GetNumOutputsRequest &x) {
    using namespace internals;
    size_t len = GetNumOutputsRequest::kBlobStartOffset;

    return len;
}
inline bool serialize(std::span<std::byte> &out, const internals::GetNumOutputsRequest &x) {
    using namespace internals;
    uint32_t blobOff = GetNumOutputsRequest::kBlobStartOffset;

    return true;
}
inline bool deserialize(const std::span<std::byte> &in, internals::GetNumOutputsRequest &x) {
    using namespace internals;
    if(in.size() < GetNumOutputsRequest::kScalarBytes) return false;
    const auto blobRegion = in.subspan(GetNumOutputsRequest::kBlobStartOffset);

    return true;
}

inline size_t bytesFor(const internals::GetNumOutputsResponse &x) {
    using namespace internals;
    size_t len = GetNumOutputsResponse::kBlobStartOffset;

    return len;
}
inline bool serialize(std::span<std::byte> &out, const internals::GetNumOutputsResponse &x) {
    using namespace internals;
    uint32_t blobOff = GetNumOutputsResponse::kBlobStartOffset;
    {
        const auto off = GetNumOutputsResponse::kElementOffsets[0];
        const auto size = GetNumOutputsResponse::kElementSizes[0];
        auto range = out.subspan(off, size);
        memcpy(range.data(), &x.status, range.size());
    }
    {
        const auto off = GetNumOutputsResponse::kElementOffsets[1];
        const auto size = GetNumOutputsResponse::kElementSizes[1];
        auto range = out.subspan(off, size);
        memcpy(range.data(), &x.count, range.size());
    }

    return true;
}
inline bool deserialize(const std::span<std::byte> &in, internals::GetNumOutputsResponse &x) {
    using namespace internals;
    if(in.size() < GetNumOutputsResponse::kScalarBytes) return false;
    const auto blobRegion = in.subspan(GetNumOutputsResponse::kBlobStartOffset);
    {
        const auto off = GetNumOutputsResponse::kElementOffsets[0];
        const auto size = GetNumOutputsResponse::kElementSizes[0];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        memcpy(&x.status, range.data(), range.size());
    }
    {
        const auto off = GetNumOutputsResponse::kElementOffsets[1];
        const auto size = GetNumOutputsResponse::kElementSizes[1];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        memcpy(&x.count, range.data(), range.size());
    }

    return true;
}

inline size_t bytesFor(const internals::SetOutputEnabledRequest &x) {
    using namespace internals;
    size_t len = SetOutputEnabledRequest::kBlobStartOffset;

    return len;
}
inline bool serialize(std::span<std::byte> &out, const internals::SetOutputEnabledRequest &x) {
    using namespace internals;
    uint32_t blobOff = SetOutputEnabledRequest::kBlobStartOffset;
    {
        const auto off = SetOutputEnabledRequest::kElementOffsets[0];
        const auto size = SetOutputEnabledRequest::kElementSizes[0];
        auto range = out.subspan(off, size);
        memcpy(range.data(), &x.displayId, range.size());
    }
    {
        const auto off = SetOutputEnabledRequest::kElementOffsets[1];
        const auto size = SetOutputEnabledRequest::kElementSizes[1];
        auto range = out.subspan(off, size);
        memcpy(range.data(), &x.enabled, range.size());
    }

    return true;
}
inline bool deserialize(const std::span<std::byte> &in, internals::SetOutputEnabledRequest &x) {
    using namespace internals;
    if(in.size() < SetOutputEnabledRequest::kScalarBytes) return false;
    const auto blobRegion = in.subspan(SetOutputEnabledRequest::kBlobStartOffset);
    {
        const auto off = SetOutputEnabledRequest::kElementOffsets[0];
        const auto size = SetOutputEnabledRequest::kElementSizes[0];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        memcpy(&x.displayId, range.data(), range.size());
    }
    {
        const auto off = SetOutputEnabledRequest::kElementOffsets[1];
        const auto size = SetOutputEnabledRequest::kElementSizes[1];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        memcpy(&x.enabled, range.data(), range.size());
    }

    return true;
}

inline size_t bytesFor(const internals::SetOutputEnabledResponse &x) {
    using namespace internals;
    size_t len = SetOutputEnabledResponse::kBlobStartOffset;

    return len;
}
inline bool serialize(std::span<std::byte> &out, const internals::SetOutputEnabledResponse &x) {
    using namespace internals;
    uint32_t blobOff = SetOutputEnabledResponse::kBlobStartOffset;
    {
        const auto off = SetOutputEnabledResponse::kElementOffsets[0];
        const auto size = SetOutputEnabledResponse::kElementSizes[0];
        auto range = out.subspan(off, size);
        memcpy(range.data(), &x.status, range.size());
    }

    return true;
}
inline bool deserialize(const std::span<std::byte> &in, internals::SetOutputEnabledResponse &x) {
    using namespace internals;
    if(in.size() < SetOutputEnabledResponse::kScalarBytes) return false;
    const auto blobRegion = in.subspan(SetOutputEnabledResponse::kBlobStartOffset);
    {
        const auto off = SetOutputEnabledResponse::kElementOffsets[0];
        const auto size = SetOutputEnabledResponse::kElementSizes[0];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        memcpy(&x.status, range.data(), range.size());
    }

    return true;
}

inline size_t bytesFor(const internals::SetOutputModeRequest &x) {
    using namespace internals;
    size_t len = SetOutputModeRequest::kBlobStartOffset;
    len += bytesFor(x.mode);

    return len;
}
inline bool serialize(std::span<std::byte> &out, const internals::SetOutputModeRequest &x) {
    using namespace internals;
    uint32_t blobOff = SetOutputModeRequest::kBlobStartOffset;
    {
        const auto off = SetOutputModeRequest::kElementOffsets[0];
        const auto size = SetOutputModeRequest::kElementSizes[0];
        auto range = out.subspan(off, size);
        memcpy(range.data(), &x.displayId, range.size());
    }
    {
        const auto off = SetOutputModeRequest::kElementOffsets[1];
        const auto size = SetOutputModeRequest::kElementSizes[1];
        auto range = out.subspan(off, size);
        const uint32_t blobSz = bytesFor(x.mode);
        const uint32_t blobDataOffset = blobOff;
        auto blobRange = out.subspan(blobDataOffset, blobSz);
        if(!serialize(blobRange, x.mode)) return false;
        blobOff += blobSz;
        memcpy(range.data(), &blobDataOffset, sizeof(blobDataOffset));
        memcpy(range.data()+sizeof(blobDataOffset), &blobSz, sizeof(blobSz));
    }

    return true;
}
inline bool deserialize(const std::span<std::byte> &in, internals::SetOutputModeRequest &x) {
    using namespace internals;
    if(in.size() < SetOutputModeRequest::kScalarBytes) return false;
    const auto blobRegion = in.subspan(SetOutputModeRequest::kBlobStartOffset);
    {
        const auto off = SetOutputModeRequest::kElementOffsets[0];
        const auto size = SetOutputModeRequest::kElementSizes[0];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        memcpy(&x.displayId, range.data(), range.size());
    }
    {
        const auto off = SetOutputModeRequest::kElementOffsets[1];
        const auto size = SetOutputModeRequest::kElementSizes[1];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        uint32_t blobSz{0}, blobDataOffset{0};
        memcpy(&blobDataOffset, range.data(), sizeof(blobDataOffset));
        memcpy(&blobSz, range.data()+sizeof(blobDataOffset), sizeof(blobSz));
        auto blobRange = in.subspan(blobDataOffset, blobSz);
       if(!deserialize(blobRange, x.mode)) {
            HandleDecodeError("SetOutputModeRequest", "mode", off, blobDataOffset, blobSz);
            return false;
        }
    }

    return true;
}

inline size_t bytesFor(const internals::SetOutputModeResponse &x) {
    using namespace internals;
    size_t len = SetOutputModeResponse::kBlobStartOffset;

    return len;
}
inline bool serialize(std::span<std::byte> &out, const internals::SetOutputModeResponse &x) {
    using namespace internals;
    uint32_t blobOff = SetOutputModeResponse::kBlobStartOffset;
    {
        const auto off = SetOutputModeResponse::kElementOffsets[0];
        const auto size = SetOutputModeResponse::kElementSizes[0];
        auto range = out.subspan(off, size);
        memcpy(range.data(), &x.status, range.size());
    }

    return true;
}
inline bool deserialize(const std::span<std::byte> &in, internals::SetOutputModeResponse &x) {
    using namespace internals;
    if(in.size() < SetOutputModeResponse::kScalarBytes) return false;
    const auto blobRegion = in.subspan(SetOutputModeResponse::kBlobStartOffset);
    {
        const auto off = SetOutputModeResponse::kElementOffsets[0];
        const auto size = SetOutputModeResponse::kElementSizes[0];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        memcpy(&x.status, range.data(), range.size());
    }

    return true;
}

}; // namespace rpc

#pragma clang diagnostic push

#endif // defined(RPC_HELPERS_GENERATED_438003604959851420)