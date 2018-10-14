#pragma once

#include <common.h>
#include "gui.h"
#include <string>

using namespace NXFramework;

class DLGInstall: public GUIComponent
{
public:
    enum DlgState
    {
        DLG_CONFIRMATION,
        DLG_PROGRESS,
        DLG_ERROR,
        DLG_DONE,
        DLG_IDLE
    };
    enum DlgMode
    {
        DLG_INSTALL,
        DLG_INSTALL_DELETE,
        DLG_INSTALL_EXTRACTED,
        DLG_EXTRACT,
        DLG_CONVERT,
    };

    DLGInstall(const GUI* gui);
    void Initialize() override;
    void Shutdown() override;
    void Render(const double timer) override;
    void Update(const double timer, const u64 kDown) override;
    void SetFilename(std::string filedir, std::string filename) { this->filedir = filedir; this->filename = filename; }
    void SetEnabled(bool enabled) override { GUIComponent::SetEnabled(enabled); }

    void        SetState(DlgState dlgState) { this->dlgState = dlgState; }
    DlgState    GetState()                  { return dlgState; }
    void        SetMode (DlgMode dlgMode)   { this->dlgMode = dlgMode; }
    DlgMode     GetMode ()                  { return dlgMode; }

private:
    void        CleanUp();
    DlgState    dlgState        = DLG_CONFIRMATION;
    DlgMode     dlgMode         = DLG_INSTALL;
    FsStorageId destStorageId   = FsStorageId_SdCard;
    bool        enoughSpace     = true;
    std::string filedir;
    std::string filename;
};
