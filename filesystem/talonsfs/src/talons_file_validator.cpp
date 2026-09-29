#include "../include/talons_file_validator.h"

namespace TalonsFile
{
    ValidationError validateHeader(
        const Header* header)
    {
        if (header == nullptr)
        {
            return ValidationError::NullHeader;
        }

        if (header->magic[0] != MAGIC[0] ||
            header->magic[1] != MAGIC[1] ||
            header->magic[2] != MAGIC[2] ||
            header->magic[3] != MAGIC[3])
        {
            return ValidationError::InvalidMagic;
        }

	if (header->version == 0)
	{
    	    return ValidationError::UnsupportedVersion;
	}

        if (header->headerSize < HEADER_SIZE)
        {
            return ValidationError::InvalidHeaderSize;
        }

        if (header->fileType == TYPE_INVALID)
        {
            return ValidationError::InvalidFileType;
        }

        if (header->flags & FLAG_RESERVED_MASK)
        {
            return ValidationError::InvalidFlags;
        }

        return ValidationError::None;
    }
}
