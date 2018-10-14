#pragma once
#include <string>
#include <switch.h>

namespace NXFramework
{

bool ExtractXCI( const std::string&  filename,
                const bool          saveNSP              = false);
bool ConvertXCI( const std::string&  filename);
bool InstallXCI( const std::string&  filename,
                const FsStorageId   destStorageId        = FsStorageId_SdCard,
                const bool          ignoreReqFirmVersion = true,
                const bool          deleteXCI            = false);

}
