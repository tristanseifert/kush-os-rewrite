#ifndef AHCIDRV_CONTROLLER_H
#define AHCIDRV_CONTROLLER_H

#include <array>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <thread>
#include <memory>
#include <mutex>

#include <libpci/UserClient.h>

struct AhciHbaRegisters;

class Port;

/**
 * Encapsulates the main behavior for an AHCI controller.
 *
 * This class is primarily responsible for overall controller initialization, interrupt management,
 * and the setup of the individual ports. Each port functions almost independently with its own
 * memory resources.
 */
class Controller {
    friend class Port;

    /// Maximum number of ports an AHCI controller supports
    constexpr static const size_t kMaxPorts{32};

    public:
        /// Create an AHCI controller attached to the given PCI device.
        Controller(const std::shared_ptr<libpci::Device> &);
        virtual ~Controller();

        /// Probes all attached ports.
        void probe();

        /// Whether the controller is 64 bit addressing capable
        constexpr bool is64BitCapable() const {
            return this->supports64Bit;
        }
        /// Maximum number of commands that may be pending at a given time
        constexpr size_t getQueueDepth() const {
            return this->numCommandSlots;
        }

    private:
        void performBiosHandoff();
        void reset();

        void irqHandlerMain();
        void handleAhciIrq();

    private:
        static const uintptr_t kAbarMappingRange[2];

        /// Whether various controller initialization parameters are logged
        constexpr static const bool kLogInit{false};
        /// Whether the process of cleaning up is logged
        constexpr static const bool kLogCleanup{false};

        /// Notification bit indicating the AHCI controller triggered an interrupt
        constexpr static const uintptr_t kAhciIrqBit{(1 << 0)};
        /// Notification bit indicating that the driver is shutting down and IRQ handler shall exit
        constexpr static const uintptr_t kDeviceWillStopBit{(1 << 1)};

        /// PCI device behind which the controller is operated
        std::shared_ptr<libpci::Device> dev;

        /// VM handle of the ABAR region for the device
        uintptr_t abarVmHandle{0};
        /// Base address of the AHCI HBA registers
        volatile AhciHbaRegisters *abar{nullptr};

        /// Number of command slots which may be used at once
        uint8_t numCommandSlots{0};
        /// Bitmap of which ports are valid and implemented on the HBA
        uint32_t validPorts{0};

        /// Whether the HBA supports 64-bit addressing
        bool supports64Bit{false};
        /// Whether the HBA supports SATA native command queuing
        bool supportsNCQ{false};
        /// Whether the HBA supports the SATA Notification register
        bool supportsSataNotifications{false};
        /// Whether staggered spinup is supported
        bool supportsStaggeredSpinup{false};

        /// Maximum supported SATA generation (1 = 1.5Gbps, 2 = 3Gbps, 3 = 6Gbps)
        size_t sataGen{0};

        /// Each implemented port has an allocated port object
        std::array<std::shared_ptr<Port>, kMaxPorts> ports;

        /// Interrupt handler thread
        std::unique_ptr<std::thread> irqHandlerThread;
        /// Thread handle of the IRQ handler thread
        uintptr_t irqHandlerThreadHandle{0};
        /// Run the interrupt handler as long as this flag is set
        std::atomic_bool irqHandlerRun{true};
        /// Indicates the interrupt handler is ready
        std::atomic_bool irqHandlerReady{false};
        /// Handle for the IRQ handler object
        uintptr_t irqHandlerHandle{0};
};

#endif
