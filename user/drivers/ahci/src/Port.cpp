#include "Port.h"
#include "Controller.h"
#include "AhciRegs.h"
#include "PortStructs.h"

#include "Log.h"

#include <algorithm>
#include <cstring>

#include <driver/ScatterGatherBuffer.h>

#include <unistd.h>
#include <sys/syscalls.h>

/// Region of virtual memory space for AHCI port command list/receive FIS blobs
uintptr_t Port::kPrivateMappingRange[2] = {
    // start
    0x10018000000,
    // end
    0x10019000000,
};
/**
 * Creates a new AHCI port object for the given controller. This allocates all memory, initializes
 * it and configures the port and spins up any attached devices, as well as identifying them.
 */
Port::Port(Controller *controller, const uint8_t _port) : port(_port), parent(controller) {
    int err;
    auto &regs = controller->abar->ports[_port];

    // allocate the command list and received FIS
    const size_t pageSz = sysconf(_SC_PAGESIZE);
    size_t allocSize = kCommandTableOffset;
    allocSize += (0x80 + (kCommandTableNumPrds * sizeof(PortCommandTablePrd))) * controller->getQueueDepth();

    allocSize = ((allocSize + pageSz - 1) / pageSz) * pageSz;

    err = AllocVirtualAnonRegion(allocSize,
            (VM_REGION_RW | VM_REGION_WRITETHRU | VM_REGION_MMIO | VM_REGION_LOCKED),
            &this->privRegionVmHandle);
    if(err) {
        throw std::system_error(errno, std::generic_category(), "AllocVirtualAnonRegion");
    }

    uintptr_t base{0};
    err = MapVirtualRegionRange(this->privRegionVmHandle, kPrivateMappingRange, allocSize, 0, &base);
    kPrivateMappingRange[0] += allocSize; // XXX: this seems like it shouldn't be necessary...
    if(err) {
        throw std::system_error(err, std::generic_category(), "MapVirtualRegion");
    }

    if(kLogInit) Trace("Mapped port %lu FIS/command list at $%p ($%p'h)", _port, base,
            this->privRegionVmHandle);

    memset(reinterpret_cast<void *>(base), 0, pageSz);

    // get its physical address
    uintptr_t physAddr{0};

    err = VirtualToPhysicalAddr(&base, 1, &physAddr);
    if(err) {
        throw std::system_error(err, std::generic_category(), "VirtualToPhysicalAddr");
    }

    // program the command list base register
    const auto cmdListPhys = physAddr + kCmdListOffset;
    this->cmdList = reinterpret_cast<volatile PortCommandList *>(base + kCmdListOffset);

    regs.cmdListBaseLow = cmdListPhys & 0xFFFFFFFF;
    if(controller->supports64Bit) {
        regs.cmdListBaseHigh = cmdListPhys >> 32;
    } else {
        if(cmdListPhys >> 32) {
            throw std::runtime_error("Command list base above 4G, but controller doesn't support 64-bit");
        }
    }

    this->initCommandTables(base);

    // program the FIS base register
    const auto rxFisPhys = physAddr + kReceivedFisOffset;
    this->receivedFis = reinterpret_cast<volatile PortReceivedFIS *>(base + kReceivedFisOffset);

    regs.fisBaseLow = rxFisPhys & 0xFFFFFFFF;
    if(controller->supports64Bit) {
        regs.fisBaseHigh = rxFisPhys >> 32;
    } else {
        if(rxFisPhys >> 32) {
            throw std::runtime_error("FIS base above 4G, but controller doesn't support 64-bit");
        }
    }

    if(kLogInit) Trace("Received FIS at $%p (%p), command list $%p (%p)", this->receivedFis,
            rxFisPhys, this->cmdList, cmdListPhys);

    // enable port interrupts
    regs.irqEnable = regs.irqEnable | AhciPortIrqs::DeviceToHostReg;

    // enable FIS reception and command processing and identify device
    this->startCommandProcessing();
    this->identDevice();
}

/**
 * Initializes all command tables for this port. We'll store the virtual address in the command
 * tables array, and then program the command list's physical address field for the corresponding
 * command table.
 */
void Port::initCommandTables(const uintptr_t vmBase) {
    int err;
    const size_t commandTableSize{0x80 + (0x10 * kCommandTableNumPrds)};

    for(size_t i = 0; i < this->parent->getQueueDepth(); i++) {
        const size_t offset{kCommandTableOffset + (i * commandTableSize)};
        const auto address{vmBase + offset};
        if(address & 0b1111111) {
            throw std::runtime_error("Failed to maintain 128 byte alignment for command tables");
        }

        this->cmdTables[i] = reinterpret_cast<volatile PortCommandTable *>(address);

        auto start = reinterpret_cast<volatile std::byte *>(this->cmdTables[i]);
        std::fill(start, start+commandTableSize, std::byte{0});

        // get its physical address and program into the command headers
        uintptr_t physAddr{0};

        err = VirtualToPhysicalAddr(&address, 1, &physAddr);
        if(err) {
            throw std::system_error(err, std::generic_category(), "VirtualToPhysicalAddr");
        }

        auto &hdr = this->cmdList->commands[i];
        hdr.cmdTableBaseLow = address & 0xFFFFFFFF;

        if(this->parent->is64BitCapable()) {
            hdr.cmdTableBaseHigh = address >> 32;
        }
    }
}

/**
 * Cleans up resources belonging to the port.
 */
Port::~Port() {
    this->stopCommandProcessing();

    // shut down the port resources

    // unmap the VM object and delete it
    UnmapVirtualRegion(this->privRegionVmHandle);
    DeallocVirtualRegion(this->privRegionVmHandle);
}



/**
 * Starts command processing and FIS reception.
 */
void Port::startCommandProcessing() {
    auto &regs = this->parent->abar->ports[this->port];

    // wait for any current command processing to complete
    while(regs.command & AhciPortCommand::CommandEngineRunning) {};
    // enable FIS reception and command sending
    regs.command = regs.command | AhciPortCommand::ReceiveFIS | AhciPortCommand::SendCommand;
}

/**
 * Stops command processing an FIS reception.
 */
void Port::stopCommandProcessing() {
    auto &regs = this->parent->abar->ports[this->port];

    // disable command processing and FIS reception
    regs.command = regs.command & ~AhciPortCommand::SendCommand;
    regs.command = regs.command & ~AhciPortCommand::ReceiveFIS;

    // wait for FIS receive and command processing to finish
    while(1) {
        if(regs.command & AhciPortCommand::CommandEngineRunning) continue;
        if(regs.command & AhciPortCommand::ReceiveFIS) continue;

        break;
    }
}



/**
 * Identifies the attached device. We'll look at the port signature to see what kind of device is
 * connected, and send either the ATA IDENTIFY DEVICE or the ATAPI command.
 */
void Port::identDevice() {
    auto &regs = this->parent->abar->ports[this->port];

    switch(regs.signature) {
        case AhciDeviceSignature::SATA: {
            Success("SATA device at port %u", this->port);

            auto buf = libdriver::ScatterGatherBuffer::Alloc(512);
            this->submitAtaCommand(0xEC, buf);
            break;
        }
        case AhciDeviceSignature::SATAPI:
            Success("SATAPI device at port %u", this->port);
            break;

        case AhciDeviceSignature::PortMultiplier:
            Warn("%s on port %u is not supported", "Port multiplier", this->port);
            break;
        case AhciDeviceSignature::EnclosureManagement:
            Warn("%s on port %u is not supported", "Enclosure management", this->port);
            break;

        // no device or is unsupported type
        default:
            break;
    }
}



/**
 * Handles an IRQ for this port.
 */
void Port::handleIrq() {
    auto &regs = this->parent->abar->ports[this->port];
    const auto is = regs.irqStatus;

    if(kLogIrq) Trace("Port %u irq: %08x", this->port, is);

    // acknowledge interrupts
    regs.irqStatus = is;
}



/**
 * Submits an ATA command to the device on this port.
 *
 * @note You should only invoke this when the device is confirmed to be an ATA device once the
 * signature is checked. ATAPI devices may not respond well to many ATA commands.
 *
 * @param cmd Command byte to write to the device
 * @param result Buffer large enough to store the full response of this request
 */
std::future<void> Port::submitAtaCommand(const uint8_t cmd,
        const std::shared_ptr<libdriver::ScatterGatherBuffer> &result) {
    // find a command slot
    const auto slotIdx = this->allocCommandSlot();
    auto table = this->cmdTables[slotIdx];

    // build the command (register, host to device) and copy to the table
    RegHostToDevFIS fis;
    fis.command = cmd;
    fis.c = 1; // write to command register

    memcpy((void *) &table->commandFIS, &fis, sizeof(fis)); // yikes

    // set up the result buffer descriptors
    const auto &extents = result->getExtents();
    if(extents.size() > kCommandTableNumPrds) {
        throw std::runtime_error("Scatter/gather buffer has too many extents!");
    }

    for(size_t i = 0; i < extents.size(); i++) {
        const auto &extent = extents[i];
        auto &prd = table->descriptors[i];

        prd.numBytes = extent.getSize();
        const auto phys = extent.getPhysAddress();

        prd.physAddrLow = phys & 0xFFFFFFFF;
        if(this->parent->is64BitCapable()) {
            prd.physAddrHigh = phys >> 32;
        }

        // if it's the last extent, we want an IRQ on completion
        prd.irqOnCompletion = (i == (extents.size() - 1)) ? 1 : 0;
    }

    // update the command list entry
    auto &cmdListEntry = this->cmdList->commands[slotIdx];
    cmdListEntry.commandFisLen = sizeof(fis) / 4;
    cmdListEntry.atapi = 0;
    cmdListEntry.write = 0;
    cmdListEntry.prefetchable = 0;

    cmdListEntry.clearBusy = 0;

    cmdListEntry.reset = 0;
    cmdListEntry.bist = 0;

    cmdListEntry.prdEntries = extents.size();

    // lastly, submit the command (so it begins executing)
    std::promise<void> p;
    auto fut = p.get_future();

    CommandInfo i(std::move(p));
    i.buffers.emplace_back(result);

    this->submitCommand(slotIdx, std::move(i));

    return fut;
}

/**
 * Returns the index of a command slot that is ready for use. It is marked as allocated.
 *
 * If there are currently no command slots available, we'll block on a condition variable that's
 * signaled any time a command completes and an old slot can be reused.
 */
size_t Port::allocCommandSlot() {
    /*
     * Find an empty slot, i.e. a bit in `busyCommands` that is clear. Since there's no guarantee
     * that the AHCI controller supports all 32 command slots, we can't use the fast intrinsics
     * and instead have to loop the entire array.
     */
    for(size_t i = 0; i < this->parent->getQueueDepth(); i++) {
        const uint32_t bit{(1U << i)};
        if(!(this->busyCommands & bit)) {
            this->busyCommands |= bit;
            return i;
        }
    }

    // TODO: block on some condition variable and retry
    throw std::runtime_error("failed to find available command");
}

/**
 * Submits the given command. This will insert it into the outstanding commands map, then notify
 * the HBA that this command is ready to execute.
 */
void Port::submitCommand(const uint8_t slot, CommandInfo info) {
    auto &regs = this->parent->abar->ports[this->port];

    // record keeping
    this->outstandingCommands |= (1 << slot);
    this->inFlightCommands[slot].emplace(std::move(info));

    // start command
    Trace("CmdIssue %u", slot);
    regs.cmdIssue = regs.cmdIssue | (1 << slot);
}


