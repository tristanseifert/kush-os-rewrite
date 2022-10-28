#ifndef KERNEL_VM_ZONEALLOCATOR_H
#define KERNEL_VM_ZONEALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

#include <Logging/Console.h>
#include <Runtime/String.h>
#include <Vm/Alloc.h>
#include <Vm/Types.h>

#include <new>

namespace Kernel::Vm {
/**
 * @brief Zone allocator
 *
 * A zone allocator pre-allocates memory for fixed size objects. In turn, it receives its memory
 * from the system's virtual page allocator. As a result, it is capable of vending out fixed size
 * allocations extremely quickly, while rarely needing to actually allocate new memory from the
 * kernel's memory pool.
 *
 * @tparam T Name of the type to allocate
 *
 * @TODO Optimizations for finding free regions
 *
 * @TODO Thread safety stuff
 */
template<class T, const char *ZoneName, size_t RegionSize>
class ZoneAllocator {
    private:
        struct Region;

        /**
         * @brief Allocation region metadata
         *
         * Structure containing metadata about a region, placed at the top end of the region.
         */
        struct RegionMetadata {
            /**
             * @brief Default magic value
             */
            constexpr static const uint64_t kMagic{0xf849a50c9e0f8139};

            /**
             * @brief Number of (64 bit) bitmap entries
             */
            constexpr static const size_t kBitmapEntries{6};

            /**
             * @brief Pointer to the next region
             *
             * All regions in a zone allocator are attached together in a linked list, traversed
             * when we need to allocate or free an object.
             */
            struct Region *next{nullptr};

            /**
             * @brief Allocation bitmap
             *
             * A set bit indicates an available slot; a clear bit indicates allocated.
             */
            uint64_t bitmap[kBitmapEntries];

            /**
             * @brief Security cookie
             *
             * Contains a known value that's checked to ensure that the structures didn't get
             * overwritten.
             */
            uint64_t magic;

            /**
             * @brief Initialize region metadata
             */
            RegionMetadata() {
                // how many usable entries have we got?
                constexpr static const auto kBitmapBits = sizeof(bitmap) * 8;
                constexpr const auto payloadSize = RegionSize - offsetof(Region, storage);
                constexpr const auto numItems = payloadSize / sizeof(T);
                static_assert(numItems <= kBitmapBits, "region size too large for bitmap");

                // fill the bitmask (TODO: optimize)
                memset(this->bitmap, 0, sizeof(bitmap));

                for(size_t i = 0; i < numItems; i++) {
                    this->bitmap[i / 64] |= (1ULL << (i % 64));
                }


                // set up the security cookie
                this->magic = kMagic ^ reinterpret_cast<uint64_t>(this);
            }
        };

        /**
         * @brief Allocation region
         *
         * The zone allocator acquires external memory in units of regions, which are fixed size
         * structures that span a particular number of bytes.
         */
        struct Region {
            /**
             * @brief Region metadata
             */
            RegionMetadata meta;

            /**
             * @brief Object storage
             *
             * The remainder of the region is reserved for storage of objects.
             */
            alignas(T) uint8_t storage[];



            /**
             * @brief Alocate memory for a new region
             */
            void *operator new(size_t) noexcept {
                return Vm::VAlloc(RegionSize);
            }
            /**
             * @brief Release memory for a region
             *
             * The memory is returned back to the virtual allocator.
             */
            void operator delete(void *ptr) {
                Vm::VFree(ptr, RegionSize);
            }

            /**
             * @brief Allocate a new object
             */
            T *alloc() {
                uint64_t *readPtr = this->meta.bitmap;

                for(size_t off = 0; off < (RegionMetadata::kBitmapEntries * 64); off += 64) {
                    // read this chunk; skip if zero (all pages allocated)
                    auto val = *readPtr;
                    if(!val) {
                        readPtr++;
                        continue;
                    }

                    // mark as allocated
                    const auto bit = __builtin_ffsll(val);
                    REQUIRE(bit, "wtf");

                    val &= ~(1ULL << (bit - 1));
                    *readPtr = val;

                    // get its address
                    const auto idx = off + (bit - 1);
                    auto addr = this->addressFor(idx);

                    // XXX: zero memory
                    memset(addr, 0, sizeof(T));
                    return addr;
                }

                // failed to allocate
                return nullptr;
            }

            /**
             * @brief Release a previously allocated object
             *
             * @param ptr Pointer to an object to release
             *
             * @remark The specified object _must_ have previously been allocated from this pool.
             */
            void free(void *ptr) {
                // validate pointer is inside the region's extents
                if(!this->contains(ptr)) {
                    PANIC("attempt to free %p from foreign region (this=%p)", ptr, this);
                }

                // mark it as available
                const auto idx = this->indexFor(ptr);
                this->meta.bitmap[idx / 64] |= (1ULL << (idx % 64));
            }

            /**
             * @brief Test if the given object was allocated in this region
             *
             * This checks if the provided pointer falls inside of the memory region reserved for
             * allocation of objects.
             */
            inline bool contains(void *ptr) {
                const auto addr = reinterpret_cast<uintptr_t>(ptr);
                const auto endAddr = reinterpret_cast<uintptr_t>(this) + RegionSize;

                if(addr < reinterpret_cast<uintptr_t>(this) || (addr + sizeof(T)) > endAddr) {
                    return false;
                }
                return true;
            }

            /**
             * @brief Get the address to the nth memory storage slot
             */
            inline T *addressFor(const size_t idx) {
                return reinterpret_cast<T *>(this->storage) + idx;
            }

            /**
             * @brief Get the bitmap index for the address of a storage slot
             */
            inline size_t indexFor(const void *ptr) {
                return (reinterpret_cast<uintptr_t>(ptr) -
                        reinterpret_cast<uintptr_t>(this->storage)) / sizeof(T);
            }

            /**
             * @brief Is the region fully loaded?
             */
            constexpr inline bool isFull() const {
                for(size_t i = 0; i < RegionMetadata::kBitmapEntries; i++) {
                    if(this->meta.bitmap[i]) {
                        return false;
                    }
                }

                // all bits are clear
                return true;
            }
        };
        static_assert(sizeof(Region) <= RegionSize, "region size over limit (wtf)");

    public:
        /**
         * @brief Allocate memory for a new object
         *
         * @return Memory for a new object or `nullptr` if no memory available
         */
        T *alloc() {
            // TODO: acquire lock

            // find a free region and allocate from it
            auto nextFree = this->findRegionWithSpace();
            if(nextFree) {
                // TODO: handle this call failing and retry another region?
                return nextFree->alloc();
            }

            // otherwise, allocate a new region and thread it in
            auto newRegion = new Region;

            if(!this->start) {
                this->start = newRegion;
            }

            if(this->last) {
                this->last->meta.next = newRegion;
            } else {
                this->last = newRegion;
            }

            this->last = newRegion;
            this->freeRegion = newRegion;

            // allocate from that region
            return newRegion->alloc();
        }

        /**
         * @brief Release a previously allocated object
         *
         * Return the memory from a previously allocated object back to the appropriate region.
         */
        void free(void *ptr) {
            // TODO: acquire lock

            // iterate over all regions
            auto region = this->start;

            while(region) {
                if(!region->contains(ptr)) {
                    continue;
                }

                region->free(ptr);
                return;
            }

            // if we get here, the object belongs to nobody (wtf?)
            PANIC("object %p not in zone %p(%s)", ptr, this, ZoneName);
        }

    private:
        /**
         * @brief Allocate a new region
         *
         * Get memory for a new region, insert it into the region list, and return a pointer to
         * the newly allocated region.
         */
        Region *newRegion() {
            auto region = new Region;
            return region;
        }

        /**
         * @brief Find the first region with vacancy to allocate new objects
         *
         * Iterate through the list of all regions, starting with the one deemed most likely to
         * have free memory.
         */
        Region *findRegionWithSpace() {
            // bail if no entries
            if(!this->start) {
                return nullptr;
            }

            // TODO: support starting search at free ptr
            auto region = this->start;

            while(region) {
                if(!region->isFull()) {
                    return region;
                }
                region = region->meta.next;
            }

            // they're all full :(
            return nullptr;
        }

    private:
        /**
         * @brief Pointer to the first region
         *
         * This may be `nullptr` if no allocations have been serviced yet. It's used as the start
         * of a linked list of all regions.
         */
        Region *start{nullptr};

        /**
         * @brief Pointer to the last region
         *
         * Points to the last region in the list of regions.
         */
        Region *last{nullptr};

        /**
         * @brief First region with free objects
         *
         * Points to the region that most recently had an object freed back to it. This is used to
         * speed up searching for free objects.
         */
        Region *freeRegion{nullptr};
};


/**
 * @brief Enlightens a type with zone allocation
 *
 * Any class that has _public_ inheritance from this will be provided with operator overloading
 * on operators `new` and `delete`, which will in turn allocate the object from a dedicated zone
 * allocator.
 *
 * @tparam T Name of the type to allocate
 *
 * @TODO There are likely thread safety issues
 */
template<class T, const char *ZoneName, size_t RegionSize = (4096 * 4)>
class WithZoneAllocation {
    protected:
        /// Type of zone allocator we're using
        using AllocatorType = ZoneAllocator<T, ZoneName, RegionSize>;

    public:
        /**
         * @brief Initialize object in place
         *
         * This just returns the provided memory; it exists to support "placement new" uses.
         *
         * @param size Size of the map object
         * @param ptr Pointer to memory address
         *
         * @return Address to construct the object at
         */
        void *operator new(size_t size, void *ptr) noexcept {
            return ptr;
        }

        /**
         * @brief Allocate memory for object
         *
         * This will get memory for an uninitialized map object from our associated zone allocator.
         */
        void *operator new(size_t size) noexcept {
            // TODO: do we do anything with size?
            return gAllocator->alloc();
        }

        /**
         * @brief Return memory to zone allocator
         */
        void operator delete(void *ptr) {
            gAllocator->free(ptr);
        }

        /**
         * @brief Initialize the zone allocator
         */
        static void InitZone() {
            alignas(AllocatorType) static uint8_t gStorage[sizeof(AllocatorType)];
            gAllocator = new(gStorage) AllocatorType;
        }

    protected:
        /**
         * @brief Associated zone allocator
         *
         * This is the zone allocator from which all objects of this type are allocated. This
         * allocator itself is set up by the `InitZone` call.
         */
        static AllocatorType *gAllocator;
};

template<class T, const char *ZoneName, size_t RegionSize>
ZoneAllocator<T, ZoneName, RegionSize> *WithZoneAllocation<T, ZoneName, RegionSize>::
    gAllocator{nullptr};
}

#endif
