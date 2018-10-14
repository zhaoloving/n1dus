#include "dlgInstall.h"
#include <nxFramework/SDL/SDLRender.h>
#include <nxFramework/SDL/SDLHelper.h>
#include <nxFramework/nspHelper.h>
#include <nxFramework/xciHelper.h>
#include <nxFramework/fileHelper.h>
#include <algorithm>
#include "gui.h"

using namespace NXFramework;

namespace
{
    struct ProcessThreadArgs
    {
        std::string filedir;
        std::string filename;
        FsStorageId destStorageId;
        bool        ignoreReqFirmVersion;
        bool*       running;
        bool        deleteSourceFile;
    };
    ProcessThreadArgs   processThreadArgs;
}

float progress        = 0.f;
int   progressState   = 0;      // 0..n various state depending on the context, -1 means error
float progressSpeed   = 0.f;

int InstallThread(void* in)
{
    ProcessThreadArgs* args = reinterpret_cast<ProcessThreadArgs*>(in);
    std::string filePath = args->filedir + args->filename;
    LOG("Installing %s...\n", filePath.c_str());
    if(strncasecmp(GetFileExt(args->filename.c_str()), "nsp", 3) == 0)
    {
        if(!InstallNSP(filePath, args->destStorageId, args->ignoreReqFirmVersion, false))
            progressState = -1;

        if(args->deleteSourceFile)
        {
            LOG("Deleting file %s...\n", filePath.c_str());
            if(remove(filePath.c_str()) != 0)
                printf("Error deleting file\n");
        }
    }
    else
    if(strncasecmp(GetFileExt(args->filename.c_str()), "xci", 3) == 0)
    {
        if(!InstallXCI(filePath, args->destStorageId, args->ignoreReqFirmVersion, args->deleteSourceFile))
            progressState = -1;
    }
    *(args->running) = false;
    return 0;
}

int InstallExtractedThread(void* in)
{
    ProcessThreadArgs* args = reinterpret_cast<ProcessThreadArgs*>(in);
    std::string filePath = args->filedir + args->filename;
    LOG("Installing folder %s...\n", filePath.c_str());
    if(!InstallExtracted(filePath, args->destStorageId, args->ignoreReqFirmVersion))
        progressState = -1;

    *(args->running) = false;
    return 0;
}

int ExtractThread(void* in)
{
    ProcessThreadArgs* args = reinterpret_cast<ProcessThreadArgs*>(in);
    std::string filePath = args->filedir + args->filename;
    LOG("\nExtracting %s...:)\n", filePath.c_str());
    if(strncasecmp(GetFileExt(args->filename.c_str()), "nsp", 3) == 0)
    {
        if(!ExtractNSP(filePath))
            progressState = -1;
    }
    else
    if(strncasecmp(GetFileExt(args->filename.c_str()), "xci", 3) == 0)
    {
        if(!ExtractXCI(filePath, false))
            progressState = -1;
    }
    *(args->running) = false;
    return 0;
}

int ConvertThread(void* in)
{
    ProcessThreadArgs* args = reinterpret_cast<ProcessThreadArgs*>(in);
    std::string filePath = args->filedir + args->filename;
    LOG("\n Converting %s...:)\n", filePath.c_str());
    if(strncasecmp(GetFileExt(args->filename.c_str()), "xci", 3) == 0)
    {
        if(!ConvertXCI(filePath))
            progressState = -1;
    }
    *(args->running) = false;
    return 0;
}


DLGInstall::DLGInstall(const GUI* gui)
: GUIComponent(gui)
{}

void DLGInstall::Initialize()
{

}
void DLGInstall::Shutdown()
{

}

void DLGInstall::Update(const double timer, const u64 kDown)
{
    if(dlgState == DLG_CONFIRMATION)
    {
        // SDCard or Nand selection
        if( dlgMode == DLG_INSTALL              ||
            dlgMode == DLG_INSTALL_DELETE       ||
            dlgMode == DLG_INSTALL_EXTRACTED)
        {
            if (kDown & KEY_DLEFT)
                destStorageId = FsStorageId_SdCard;
            else
            if (kDown & KEY_DRIGHT)
                destStorageId = FsStorageId_NandUser;
        }

        // Thread kick off
        if (kDown & KEY_A)
        {
            std::string filePath = filedir + filename;
            u64 freeSpace   = GetFreeSpace(destStorageId);
            u64 inputSize   = (dlgMode == DLG_INSTALL_EXTRACTED)?
                                GetDirSizeNonRecursive(filePath.c_str()):
                                GetFileSize(filePath.c_str());
            enoughSpace     = true;

            // Check if enoug free space
            if(dlgMode == DLG_CONVERT)
            {
                // we need twice the XCI size: 1x for temp files, 1x for NSP
                if(freeSpace < 2 * inputSize)
                {
#ifdef DEBUG
                    char freeStr[256];
                    char neededStr[256];
                    GetSizeString(freeStr, freeSpace);
                    GetSizeString(neededStr, 2 * inputSize);
                    LOG("Not enough space to convert %s... %s free %s needed\n", filePath.c_str(), freeStr, neededStr);
#endif
                    enoughSpace = false;
                }
            }
            else
            if(dlgMode == DLG_INSTALL           ||
               dlgMode == DLG_INSTALL_DELETE    ||
               dlgMode == DLG_INSTALL_EXTRACTED ||
               dlgMode == DLG_EXTRACT)
            {
                // we need the file size
                if(freeSpace < inputSize)
                {
#ifdef DEBUG
                    char freeStr[256];
                    char neededStr[256];
                    GetSizeString(freeStr, freeSpace);
                    GetSizeString(neededStr, inputSize);
                    LOG("Not enough space to install/extract %s... %s free %s needed\n", filePath.c_str(), freeStr, neededStr);
#endif
                    enoughSpace = false;
                }
            }
            else
            {
#ifdef DEBUG
                char freeStr[256];
                GetSizeString(freeStr, freeSpace);
                LOG("Enough space to process %s... %s free\n", filePath.c_str(), freeStr);
#endif
            }

            if(enoughSpace)
            {
                displayOpenFiles();
                if(dlgMode == DLG_INSTALL || dlgMode == DLG_INSTALL_DELETE)
                {
                    progress                                 = 0.f;
                    progressState                            = 0;
                    processThreadArgs.filedir                = filedir;
                    processThreadArgs.filename               = filename;
                    processThreadArgs.destStorageId          = destStorageId;
                    processThreadArgs.ignoreReqFirmVersion   = true;
                    processThreadArgs.running                = &workerThread.running;
                    processThreadArgs.deleteSourceFile       = (dlgMode == DLG_INSTALL_DELETE);
                    workerThread.running                     = true;
                    LOG("Installing %s to %s...\n", filePath.c_str(), destStorageId==FsStorageId_SdCard?"SD Card":"Nand");
                    thrd_create(&workerThread.thread, InstallThread, &processThreadArgs);
                    dlgState = DLG_PROGRESS;
                }
                else
                if(dlgMode == DLG_INSTALL_EXTRACTED)
                {
                    progress                                 = 0.f;
                    progressState                            = 0;
                    processThreadArgs.filedir                = filedir;
                    processThreadArgs.filename               = filename;
                    processThreadArgs.destStorageId          = destStorageId;
                    processThreadArgs.ignoreReqFirmVersion   = true;
                    processThreadArgs.deleteSourceFile       = false;
                    processThreadArgs.running                = &workerThread.running;
                    workerThread.running                     = true;
                    LOG("Installing folder %s to %s...\n", filePath.c_str(), destStorageId==FsStorageId_SdCard?"SD Card":"Nand");
                    thrd_create(&workerThread.thread, InstallExtractedThread, &processThreadArgs);
                    dlgState = DLG_PROGRESS;
                }
                else
                if(dlgMode == DLG_EXTRACT)
                {
                    progress                                 = 0.f;
                    progressState                            = 0;
                    processThreadArgs.filedir                = filedir;
                    processThreadArgs.filename               = filename;
                    processThreadArgs.deleteSourceFile       = false;
                    processThreadArgs.running                = &workerThread.running;
                    workerThread.running                     = true;
                    LOG("Extracting %s...\n", filePath.c_str());
                    thrd_create(&workerThread.thread, ExtractThread, &processThreadArgs);
                    dlgState = DLG_PROGRESS;
                }
                if(dlgMode == DLG_CONVERT)
                {
                    progress                                 = 0.f;
                    progressState                            = 0;
                    processThreadArgs.filedir                = filedir;
                    processThreadArgs.filename               = filename;
                    processThreadArgs.deleteSourceFile       = false;
                    processThreadArgs.running                = &workerThread.running;
                    workerThread.running                     = true;
                    LOG("Extracting %s...\n", filePath.c_str());
                    thrd_create(&workerThread.thread, ConvertThread, &processThreadArgs);
                    dlgState = DLG_PROGRESS;
                }
            }
        }
        else
        if (kDown & KEY_B)
        {
            // Quit this window
            SetEnabled(false);
            dlgState        = DLG_DONE;
            dlgMode         = DLG_INSTALL;
            progress        = 0.f;
            progressSpeed   = 0.f;
            progressState   = 0;
            enoughSpace     = true;
        }
    }
    else
    if(dlgState == DLG_PROGRESS)
    {
        if(progressState == -1)     // if error
            dlgState = DLG_ERROR;
        else
        if(!workerThread.running)   // we are done and no error
            CleanUp();
    }
    else
    if(dlgState == DLG_ERROR)
    {
        if (kDown & KEY_A)
            CleanUp();
    }
}

void DLGInstall::CleanUp()
{
    thrd_join(workerThread.thread, NULL);
    dlgState        = DLG_DONE;
    dlgMode         = DLG_INSTALL;
    progress        = 0.f;
    progressSpeed   = 0.f;
    progressState   = 0;
    enoughSpace     = true;
    displayOpenFiles();
}

void DLGInstall::Render(const double timer)
{
    const char* fileExt = GetFileExt(filename.c_str());
    bool isXCI = (strncasecmp(fileExt, "xci", 3) == 0);

    std::string message;
  	switch(dlgState)
	{
		case DLG_PROGRESS:
		    // Extract
            if(dlgMode == DLG_EXTRACT)
            {
                if(!isXCI || progressState == 0)
                    message = "Extracting NCAs...";
                else
                if(isXCI && progressState == 1)
                    message = "Patching NCAs...";
            }
            // Install
            else
            if(dlgMode == DLG_INSTALL           ||
               dlgMode == DLG_INSTALL_EXTRACTED ||
               dlgMode == DLG_INSTALL_DELETE)
            {
                if(!isXCI || progressState == 2)
                    message = std::string("Installing to ") +
                        (destStorageId==FsStorageId_SdCard?
                        std::string("SD Card"):std::string("Nand")) +
                        std::string("...");
                else
                if(isXCI && progressState == 0)
                    message = "Extracting NCAs...";
                else
                if(isXCI && progressState == 1)
                    message = "Patching NCAs...";
            }
            // Convert
            else
            {
                if(progressState == 0)
                    message = "Extracting NCAs...";
                else
                if(progressState == 1)
                    message = "Patching NCAs...";
                else
                    message = "Saving NSP...";
            }
        break;
		case DLG_DONE:
            message = "Done!";
        break;
        case DLG_ERROR:
            message = "Error processing files!";
        break;
		case DLG_CONFIRMATION:
        default:
            if(dlgMode == DLG_EXTRACT)
                message = "Extract";
            else
            if(dlgMode == DLG_INSTALL || dlgMode == DLG_INSTALL_EXTRACTED)
                message = "Install";
            else
            if(dlgMode == DLG_INSTALL_DELETE)
                message = "Install and Delete";
            else
                message = "Convert";
	}
    // Main frame
    SDL::DrawImageScale(SDL::Renderer, rootGui->TextureHandle(GUI::Properties_dialog_dark),
                        190, 247, 900, 225);

    // Message
    int message_height = 0;
    TTF_SizeText(rootGui->FontHandle(GUI::Roboto), message.c_str(), NULL, &message_height);
    if(enoughSpace)
    {
        SDL::DrawText(SDL::Renderer, rootGui->FontHandle(GUI::Roboto),
                      205, 255, (dlgState == DLG_ERROR)?RED:TITLE_COL, message.c_str());
    }
    else
    {
        std::string notEnoughSpace =
            std::string("Not enough free space on ")                                        +
            (destStorageId==FsStorageId_SdCard?std::string("SD Card"):std::string("Nand"))  +
            std::string("!");
        SDL::DrawText(SDL::Renderer, rootGui->FontHandle(GUI::Roboto),
                      205, 255, RED, notEnoughSpace.c_str(), 600);
    }

    // Filename
    SDL::DrawText(SDL::Renderer, rootGui->FontHandle(GUI::Roboto_small),
                  205, 255 + message_height, TITLE_COL, filename.c_str(), 600);

    // Cancel
    int options_cancel_width  = 0;
    int options_cancel_height = 0;
    TTF_SizeText(rootGui->FontHandle(GUI::Roboto), "(B) Cancel", &options_cancel_width, &options_cancel_height);
    SDL::DrawText(SDL::Renderer, rootGui->FontHandle(GUI::Roboto),
                  1060 - options_cancel_width,
                  467  - options_cancel_height, dlgState != DLG_CONFIRMATION?DARK_GREY:TITLE_COL, "(B) CANCEL");

    // OK
    if(enoughSpace)
    {
        int options_ok_width  = 0;
        TTF_SizeText(rootGui->FontHandle(GUI::Roboto), "(A) OK", &options_ok_width, NULL);
        SDL::DrawText(SDL::Renderer, rootGui->FontHandle(GUI::Roboto),
                      1060 - 50 - options_cancel_width - options_ok_width,
                      467       - options_cancel_height, dlgState == DLG_PROGRESS?DARK_GREY:TITLE_COL, "(A) OK");
    }

    // SDCard or Nand
    if(dlgState == DLG_CONFIRMATION)
    {
        if(dlgMode == DLG_INSTALL           ||
           dlgMode == DLG_INSTALL_DELETE    ||
           dlgMode == DLG_INSTALL_EXTRACTED)
        {
            int txt_height = 0;
	        int txt_width  = 0;
            TTF_SizeText(rootGui->FontHandle(GUI::Roboto), "SD Card", &txt_width, &txt_height);
            SDL::DrawText(SDL::Renderer, rootGui->FontHandle(GUI::Roboto),
                          190+350, 375-txt_height,
                          (destStorageId == FsStorageId_SdCard)  ?CYAN:DARK_GREY, "SD Card");

            TTF_SizeText(rootGui->FontHandle(GUI::Roboto), "Nand", &txt_width, &txt_height);
            SDL::DrawText(SDL::Renderer, rootGui->FontHandle(GUI::Roboto),
                          190+900-350-txt_width, 375-txt_height,
                          (destStorageId == FsStorageId_NandUser)?CYAN:DARK_GREY, "Nand");
        }
    }

    if(dlgState == DLG_PROGRESS)
    {
        int barSize           = 850;
        int borderSize        = 3;

        char percentStr[32];
        sprintf(percentStr, "%d%s - %.2f MB/s", (int)(progress * 100.0), "%", progressSpeed);
        int percent_witdh  = 0;
        int percent_height = 0;
        TTF_SizeText(rootGui->FontHandle(GUI::Roboto), percentStr, &percent_witdh, &percent_height);
        SDL::DrawText(SDL::Renderer, rootGui->FontHandle(GUI::Roboto), 215+barSize-percent_witdh, 375-borderSize-percent_height-5, TITLE_COL, percentStr);

        // Progress bar
        float progressBarSize = barSize * progress;
        SDL::DrawRect(SDL::Renderer, 215-borderSize , 375-borderSize, barSize+borderSize*2, 20+borderSize*2 , DARK_GREY);
        SDL::DrawRect(SDL::Renderer, 215            , 375           , (int)progressBarSize, 20              , CYAN);
    }
}
