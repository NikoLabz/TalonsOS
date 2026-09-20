#include "block_device.h"

#include <stdint.h>

namespace
{
    constexpr uint16_t ATA_DATA       = 0x1F0;
    constexpr uint16_t ATA_ERROR      = 0x1F1;
    constexpr uint16_t ATA_SECTOR_CNT = 0x1F2;
    constexpr uint16_t ATA_LBA_LOW    = 0x1F3;
    constexpr uint16_t ATA_LBA_MID    = 0x1F4;
    constexpr uint16_t ATA_LBA_HIGH   = 0x1F5;
    constexpr uint16_t ATA_DRIVE      = 0x1F6;
    constexpr uint16_t ATA_STATUS     = 0x1F7;
    constexpr uint16_t ATA_COMMAND    = 0x1F7;

    constexpr uint8_t ATA_CMD_READ_PIO  = 0x20;
    constexpr uint8_t ATA_CMD_WRITE_PIO = 0x30;
    constexpr uint8_t ATA_CMD_CACHE_FLUSH = 0xE7;

    constexpr uint8_t ATA_STATUS_BSY = 0x80;
    constexpr uint8_t ATA_STATUS_DRQ = 0x08;
    constexpr uint8_t ATA_STATUS_ERR = 0x01;

    static inline void outb(
        uint16_t port,
        uint8_t value
    )
    {
        asm volatile (
            "outb %0, %1"
            :
            : "a"(value), "Nd"(port)
        );
    }

    static inline uint8_t inb(
        uint16_t port
    )
    {
        uint8_t value;

        asm volatile (
            "inb %1, %0"
            : "=a"(value)
            : "Nd"(port)
        );

        return value;
    }

    static inline void outw(
        uint16_t port,
        uint16_t value
    )
    {
        asm volatile (
            "outw %0, %1"
            :
            : "a"(value), "Nd"(port)
        );
    }

    bool ata_wait_ready()
    {
        for (uint32_t i = 0;
             i < 1000000;
             ++i)
        {
            const uint8_t status =
                inb(ATA_STATUS);

            if (status & ATA_STATUS_ERR)
            {
                return false;
            }

            if (!(status & ATA_STATUS_BSY) &&
                (status & ATA_STATUS_DRQ))
            {
                return true;
            }
        }

        return false;
    }

    bool ata_wait_not_busy()
    {
        for (uint32_t i = 0;
             i < 1000000;
             ++i)
        {
            const uint8_t status =
                inb(ATA_STATUS);

            if (status & ATA_STATUS_ERR)
            {
                return false;
            }

            if (!(status & ATA_STATUS_BSY))
            {
                return true;
            }
        }

        return false;
    }

    bool ata_read_sector(
        uint32_t lba,
        void* buffer
    )
    {
        if (buffer == nullptr)
        {
            return false;
        }

        if (!ata_wait_not_busy())
        {
            return false;
        }

        outb(ATA_DRIVE,
             static_cast<uint8_t>(
                 0xE0 | ((lba >> 24) & 0x0F)
             ));

        outb(ATA_SECTOR_CNT, 1);
        outb(ATA_LBA_LOW,
             static_cast<uint8_t>(lba & 0xFF));
        outb(ATA_LBA_MID,
             static_cast<uint8_t>((lba >> 8) & 0xFF));
        outb(ATA_LBA_HIGH,
             static_cast<uint8_t>((lba >> 16) & 0xFF));

        outb(
            ATA_COMMAND,
            ATA_CMD_READ_PIO
        );

        if (!ata_wait_ready())
        {
            return false;
        }

        uint16_t* destination =
            reinterpret_cast<uint16_t*>(buffer);

        for (int i = 0; i < 256; ++i)
        {
            uint16_t value;

            asm volatile (
                "inw %1, %0"
                : "=a"(value)
                : "Nd"(ATA_DATA)
            );

            destination[i] = value;
        }

        return true;
    }

    bool ata_write_sector(
        uint32_t lba,
        const void* buffer
    )
    {
        if (buffer == nullptr)
        {
            return false;
        }

        if (!ata_wait_not_busy())
        {
            return false;
        }

        outb(
            ATA_DRIVE,
            static_cast<uint8_t>(
                0xE0 | ((lba >> 24) & 0x0F)
            )
        );

        outb(ATA_SECTOR_CNT, 1);

        outb(
            ATA_LBA_LOW,
            static_cast<uint8_t>(lba & 0xFF)
        );

        outb(
            ATA_LBA_MID,
            static_cast<uint8_t>(
                (lba >> 8) & 0xFF
            )
        );

        outb(
            ATA_LBA_HIGH,
            static_cast<uint8_t>(
                (lba >> 16) & 0xFF
            )
        );

        outb(
            ATA_COMMAND,
            ATA_CMD_WRITE_PIO
        );

        if (!ata_wait_ready())
        {
            return false;
        }

        const uint16_t* source =
            reinterpret_cast<const uint16_t*>(buffer);

        for (int i = 0; i < 256; ++i)
        {
            outw(
                ATA_DATA,
                source[i]
            );
        }

        outb(
            ATA_STATUS,
            ATA_CMD_CACHE_FLUSH
        );

        return ata_wait_not_busy();
    }
}


// ============================================================
// KernelBlockDevice
// ============================================================

bool KernelBlockDevice::readBlock(
    uint64_t block,
    void* buffer
)
{
    if (buffer == nullptr)
    {
        return false;
    }

    /*
     * TalonsFS uses filesystem-relative block numbers.
     *
     * Filesystem block 0 is physical disk block FILESYSTEM_START (128).
     */
    if (block >= getBlockCount())
    {
        return false;
    }

    const uint64_t physicalBlock =
        FILESYSTEM_START + block;

    if (physicalBlock >= TOTAL_DISK_BLOCKS)
    {
        return false;
    }

    return ata_read_sector(
        static_cast<uint32_t>(physicalBlock),
        buffer
    );
}


bool KernelBlockDevice::writeBlock(
    uint64_t block,
    const void* buffer
)
{
    if (buffer == nullptr)
    {
        return false;
    }

    /*
     * TalonsFS uses filesystem-relative block numbers.
     *
     * Filesystem block 0 is physical disk block FILESYSTEM_START (128).
     */
    if (block >= getBlockCount())
    {
        return false;
    }

    const uint64_t physicalBlock =
        FILESYSTEM_START + block;

    if (physicalBlock >= TOTAL_DISK_BLOCKS)
    {
        return false;
    }

    return ata_write_sector(
        static_cast<uint32_t>(physicalBlock),
        buffer
    );
}


uint64_t KernelBlockDevice::getBlockCount() const
{
    /*
     * The first 17 physical blocks belong to the
     * boot/disk area and are not part of TalonsFS.
     */
    return TOTAL_DISK_BLOCKS -
           FILESYSTEM_START;
}


uint32_t KernelBlockDevice::getBlockSize() const
{
    return 512;
}
