#pragma once

#include <common.h>
#include <fileHelper.h>
#include "gui.h"
#include "dlgInstall.h"

using namespace NXFramework;

class GUIFileBrowser: public GUIComponent
{
public:
    GUIFileBrowser(const GUI* gui);
    void Initialize() override;
    void Shutdown() override;
    void Render(const double timer) override;
    void Update(const double timer, const u64 kDown) override;
private:
    struct Cursor
    {
        int current = 0;
        int top    = 0;
    };
    char                    curDir[512];
    Cursor                  cursor;
    std::vector<DirEntry>   dirEntries;
    std::vector<std::string>extFilters;
    std::vector<Cursor>     cursorStack;
    DLGInstall              dlgInstall;
};
