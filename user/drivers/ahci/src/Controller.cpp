#include "Controller.h"
#include "Port.h"
#include "AhciRegs.h"

#include "Log.h"

#include <sys/syscalls.h>
#include <libpci/UserClient.h>

#include <stdexcept>
#include <system_error>

/// Region of virtual memory space for mapping AHCI ABAR regions
const uintptr_t Controller::kAbarMappingRange[2] = {
    // start
    0x10000000000,
    // end
    0x10001000000,
};
/**
 * Initializes an AHCI controller attached to the given PCI device. We'll start off by mapping the
 * register areas, then perform the actual AHCI controller initialization.
 */
Controller::Controller(const std::shared_ptr<libpci::Device> &_dev) : dev(_dev) {
    int err;

    // find ABAR (this is always BAR5)
    size_t abarSize{0};
    for(const auto &a : _dev->getAddressResources()) {
        if(a.bar != libpci::Device::BaseAddress::BAR5) continue;

        // create physical region
        abarSize = a.length;
        err = AllocVirtualPhysRegion(a.base, abarSize,
                (VM_REGION_RW | VM_REGION_MMIO | VM_REGION_WRITETHRU), &this->abarVmHandle);
        if(err) {
            throw std::system_error(errno, std::generic_category(), "AllocVirtualPhysRegion");
        }
    }

    if(!this->abarVmHandle) {
        throw std::runtime_error("Failed to find ABAR");
    }

    // map ABAR
    uintptr_t base{0};
    err = MapVirtualRegionRange(this->abarVmHandle, kAbarMappingRange, abarSize, 0, &base);
    if(err) {
        throw std::system_error(err, std::generic_category(), "MapVirtualRegion");
    }

    this->abar = reinterpret_cast<volatile AhciHbaRegisters *>(base);

    // TODO: verify version?
    if(kLogInit) Trace("AHCI version for %s: %08x", _dev->getPath().c_str(), this->abar->version);

    // grab ownership from BIOS/system firmware if needed and reset the HBA
    if(this->abar->hostCapsExt & AhciHostCaps2::BiosHandoffSupported) {
        this->performBiosHandoff();
    }

    this->reset();

    const auto hostCaps = this->abar->hostCaps;
    this->supports64Bit = hostCaps & AhciHostCaps::Supports64Bit;
    this->supportsNCQ = hostCaps & AhciHostCaps::SataNCQ;
    this->supportsSataNotifications = hostCaps & AhciHostCaps::SNotification;
    this->supportsStaggeredSpinup = hostCaps & AhciHostCaps::StaggeredSpinup;

    this->sataGen = (hostCaps & (AhciHostCaps::HbaMaxSpeedMask)) >> AhciHostCaps::HbaMaxSpeedOffset;

    // set up the interrupt handler and wait for it to allocate a vector; then register for MSI
    if(!this->dev->supportsMsi()) {
        throw std::runtime_error("AHCI controller requires MSI support");
    }

    this->irqHandlerThread = std::make_unique<std::thread>(&Controller::irqHandlerMain, this);

    while(!this->irqHandlerReady) {
        // this is kind of nasty but whatever
        ThreadUsleep(1000 * 33);
    }

    // put the controller into AHCI mode and read some info
    this->abar->ghc = this->abar->ghc | AhciGhc::AhciEnable;
    this->validPorts = this->abar->portsImplemented;

    this->numCommandSlots = ((this->abar->hostCaps & AhciHostCaps::NumCommandSlotsMask)
        >> AhciHostCaps::NumCommandSlotsOffset) + 1;
    if(kLogInit) Trace("Have %lu command slots", this->numCommandSlots);

    // configure each of the implemented ports
    for(size_t i = 0; i < kMaxPorts; i++) {
        if(!(this->abar->portsImplemented & (1 << i))) continue;
        auto port = std::make_shared<Port>(this, i);

        this->ports[i] = std::move(port);
    }

    // enable interrupts
    this->abar->ghc = this->abar->ghc | AhciGhc::IrqEnable;

    if(this->abar->ghc & AhciGhc::MsiSingleMessage) {
        Warn("AHCI HBA %s is using single MSI mode!", this->dev->getPath().c_str());
    }
}

/**
 * Performs the BIOS/OS handoff procedure. It is described in detail in section 10.6 of the AHCI
 * specification (version 1.3).
 *
 * XXX: This is currently untested
 */
void Controller::performBiosHandoff() {
    // 1. set OS ownership flag
    this->abar->bohc = this->abar->bohc | AhciBohc::OsOwnershipFlag;
    // 3. wait for BIOS ownership flag to be cleared
    while(this->abar->bohc & AhciBohc::BiosOwnershipFlag) {}
    // 4. give HBA 25msec to set the busy flag
    ThreadUsleep(25 * 1000);
    const bool needsCleanup = this->abar->bohc & AhciBohc::BiosBusy;

    // 5. if the busy flag was set, wait at least 2 seconds for commands to complete
    if(needsCleanup) {
        ThreadUsleep(2 * 1000 * 1000);
    }
}

/**
 * Resets the HBA.
 *
 * TODO: We need to add a 1 second timeout; if the HBA doesn't reset in that timeframe, we should
 * give up and assume it's fucked.
 */
void Controller::reset() {
    this->abar->ghc = this->abar->ghc | AhciGhc::Reset;
    while(this->abar->ghc & AhciGhc::Reset) {}
}



/**
 * Cleans up the resources allocated by the AHCI controller.
 */
Controller::~Controller() {
    // shut down IRQ handler
    this->irqHandlerRun = false;
    NotificationSend(this->irqHandlerThreadHandle, kDeviceWillStopBit);
    this->irqHandlerThread->join();

    // lastly, remove the ABAR mapping
    if(this->abarVmHandle) {
        UnmapVirtualRegion(this->abarVmHandle);
        DeallocVirtualRegion(this->abarVmHandle);
    }
}



/**
 * Main loop for the interrupt handler
 */
void Controller::irqHandlerMain() {
    int err;

    // set up the IRQ handler object
    err = ThreadGetHandle(&this->irqHandlerThreadHandle);
    if(err) Abort("Failed to get irq handler thread handle: %d", err);

    ThreadSetName(0, "AHCI irq handler");

    err = IrqHandlerInstallLocal(0, kAhciIrqBit, &this->irqHandlerHandle);
    if(err) Abort("%s failed: %d", "IrqHandlerInstallLocal", err);

    err = IrqHandlerGetInfo(this->irqHandlerHandle, SYS_IRQ_INFO_VECTOR);
    if(err < 0) Abort("%s failed: %d", "IrqHandlerGetInfo", err);
    const uintptr_t vector = err;

    // configure the PCI device to use MSI
    // TODO: figure out CPU APIC id
    this->dev->enableMsi(0, vector, 1);

    // we're ready to process events
    this->irqHandlerReady = true;
    if(kLogInit) Trace("IRQ handler set up (vector %lu)", vector);

    // await interrupts or events (via notifications)
    while(this->irqHandlerRun) {
        const uintptr_t bits = NotificationReceive(0, UINTPTR_MAX);

        if(bits & kAhciIrqBit) {
            this->handleAhciIrq();
        }
    }

    // clean up
    if(kLogCleanup) Trace("Cleaning up IRQ handler");
    dev->disableMsi();

    // XXX: there is not currently a way for us to release the allocated MSI vector...
    IrqHandlerRemove(this->irqHandlerHandle);
}

/**
 * Handles an AHCI interrupt.
 */
void Controller::handleAhciIrq() {
    // figure out which ports have an interrupt pending
    const auto is = this->abar->irqStatus;

    for(size_t i = 0; i < kMaxPorts; i++) {
        const uint32_t bit{1U << i};
        if(is & bit) {
            this->ports[i]->handleIrq();
        }
    }

    // clear interrupshiones
    this->abar->irqStatus = is;
}
