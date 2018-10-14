#include "guiFileBrowser.h"
#include <nxFramework/SDL/SDLRender.h>
#include <nxFramework/SDL/SDLHelper.h>
#include <nxFramework/nspHelper.h>
#include <nxFramework/xciHelper.h>
#include <algorithm>
#include "gui.h"

#define START_PATH ROOT_PATH
#define FILES_PER_PAGE 15

using namespace NXFramework;

GUIFileBrowser::GUIFileBrowser(const GUI* gui)
: GUIComponent(gui)
, dlgInstall(gui)
{
    strcpy(curDir, START_PATH);
    dlgInstall.SetEnabled(false);
}
void GUIFileBrowser::Initialize()
{
    extFilters.push_back("nsp");
    extFilters.push_back("xci");
    extFilters.push_back("nca");
    extFilters.push_back("cnmt");
    extFilters.push_back("xml");
    extFilters.push_back("cert");
    extFilters.push_back("tik");
    extFilters.push_back("00");
    extFilters.clear(); // debug
    PopulateFiles(curDir, dirEntries, extFilters);
}
void GUIFileBrowser::Shutdown()
{

}
void GUIFileBrowser::Render(const double timer)
{
    int topCoord = 90;
	int title_height = 0;
	TTF_SizeText(rootGui->FontHandle(GUI::Roboto_large), curDir, NULL, &title_height);
	SDL::DrawText(SDL::Renderer, rootGui->FontHandle(GUI::Roboto), 12, 18 + ((topCoord - title_height)/2), WHITE, curDir);

	int printed = 0;
	for(int i = 0 ; i < (int)dirEntries.size() ; ++i)
	{
		if (printed == FILES_PER_PAGE) // Limit the files per page
			break;

		if(i >= cursor.top)
		{
			if (i == cursor.current)
				SDL::DrawRect(SDL::Renderer, 0, topCoord + (37 * printed), 1280, 36, SELECTOR_COL);

            SDL::DrawImageScale(SDL::Renderer,
                                (dirEntries[i].isDir)?
                                rootGui->TextureHandle(GUI::Icon_dir):
                                rootGui->TextureHandle(GUI::Icon_file),
                                20, topCoord + (37 * printed), 36, 36);

			// Date // TODO: Date looks wrong, is it a libnx bug?
            int widthDate = 0;
#if 0
            char dateString[256];
            GetFileModifiedTime(std::string(std::string(curDir) + (char*)dirEntries[i].name).c_str(), dateString);
			TTF_SizeText(rootGui->FontHandle(GUI::Roboto_small), dateString, &widthDate, NULL);
			SDL::DrawText(SDL::Renderer, rootGui->FontHandle(GUI::Roboto_small), 1260 - widthDate, 145 + (37 * printed), TEXT_MIN_COL, dateString);
#endif
			// Folder / File size
            if (!dirEntries[i].isDir)
			{
                int width = 0;
				char size[16];
                GetSizeString(size, dirEntries[i].size);
				TTF_SizeText(rootGui->FontHandle(GUI::Roboto_small), size, &width, NULL);
				SDL::DrawText(SDL::Renderer, rootGui->FontHandle(GUI::Roboto_small), 1260 - width - widthDate, topCoord + (37 * printed), TEXT_MIN_COL, size);
			}
			else
            {
                int width = 0;
				TTF_SizeText(rootGui->FontHandle(GUI::Roboto_small), "Folder", &width, NULL);
				SDL::DrawText(SDL::Renderer, rootGui->FontHandle(GUI::Roboto_small), 1260 - width - widthDate, topCoord + 7 + (37 * printed), TEXT_MIN_COL, "Folder");
            }

   			char buf[64];
			strncpy(buf, (char*)dirEntries[i].name, sizeof(buf));
			buf[sizeof(buf) - 1] = '\0';
			int height = 0;
			TTF_SizeText(rootGui->FontHandle(GUI::Roboto), buf, NULL, &height);
			SDL::DrawText(SDL::Renderer, rootGui->FontHandle(GUI::Roboto), 65, topCoord + ((37 - height)/2) + (37 * printed), WHITE,
                 (strncmp((char*)dirEntries[i].name, "..", 2) == 0)?"Parent folder":buf);

			printed++; // Increase printed counter
		}
	}
    // Hint Bar
    SDL::DrawRect(SDL::Renderer, 0, 680, 1280, 40, MENU_BAR_COL);
    DirEntry& entry = dirEntries[cursor.current];
    const char* fileExt = GetFileExt((char*)entry.name);
    if(entry.isDir)
    {
        std::string keyB = "(B) Go up";
        std::string keyA = "(A) Explore";
        std::string keyX = "(X) Install extracted NSP/XCI";

        int widthKeyB  = 0;
        int heightKeyB = 0;
        TTF_SizeText(rootGui->FontHandle(GUI::Roboto_small), keyB.c_str(), &widthKeyB, &heightKeyB);
        SDL::DrawText(SDL::Renderer, rootGui->FontHandle(GUI::Roboto),
                      12,
                      708 - heightKeyB,
                      (strcmp(curDir, ROOT_PATH) != 0)?WHITE:DARK_GREY, keyB.c_str());

        int widthKeyA  = 0;
        int heightKeyA = 0;
        TTF_SizeText(rootGui->FontHandle(GUI::Roboto_small), keyA.c_str(), &widthKeyA, &heightKeyA);
        SDL::DrawText(SDL::Renderer, rootGui->FontHandle(GUI::Roboto),
                      12 + 45 * 1 + widthKeyB,
                      708 - heightKeyA,
                      WHITE, keyA.c_str());

        int widthKeyX  = 0;
        int heightKeyX = 0;
        TTF_SizeText(rootGui->FontHandle(GUI::Roboto_small), keyX.c_str(), &widthKeyX, &heightKeyX);
        SDL::DrawText(SDL::Renderer, rootGui->FontHandle(GUI::Roboto),
                      12 + 45 * 2 + widthKeyA + widthKeyB,
                      708 - heightKeyX,
                      WHITE, keyX.c_str());
    }
    else
    if((strncasecmp(fileExt, "nsp", 3) == 0) || (strncasecmp(fileExt, "xci", 3) == 0))
    {
        std::string keyB        = "(B) Go up";
        std::string keyA        = "(A) Install";
        std::string keyX        = "(X) Install & Delete";
        std::string keyY        = "(Y) Extract";
        std::string keyMinus    = "(-) Convert to NSP";

        int widthKeyB  = 0;
        int heightKeyB = 0;
        TTF_SizeText(rootGui->FontHandle(GUI::Roboto_small), keyB.c_str(), &widthKeyB, &heightKeyB);
        SDL::DrawText(SDL::Renderer, rootGui->FontHandle(GUI::Roboto),
                      12,
                      708 - heightKeyB,
                      (strcmp(curDir, ROOT_PATH) != 0)?WHITE:DARK_GREY, keyB.c_str());

        int widthKeyA  = 0;
        int heightKeyA = 0;
        TTF_SizeText(rootGui->FontHandle(GUI::Roboto_small), keyA.c_str(), &widthKeyA, &heightKeyA);
        SDL::DrawText(SDL::Renderer, rootGui->FontHandle(GUI::Roboto),
                      12 + 45 * 1 + widthKeyB,
                      708 - heightKeyA,
                      WHITE, keyA.c_str());

        int widthKeyX  = 0;
        int heightKeyX = 0;
        TTF_SizeText(rootGui->FontHandle(GUI::Roboto_small), keyX.c_str(), &widthKeyX, &heightKeyX);
        SDL::DrawText(SDL::Renderer, rootGui->FontHandle(GUI::Roboto),
                      12 + 45 * 2 + widthKeyA + widthKeyB,
                      708 - heightKeyX,
                      WHITE, keyX.c_str());

        int widthKeyY  = 0;
        int heightKeyY = 0;
        TTF_SizeText(rootGui->FontHandle(GUI::Roboto_small), keyY.c_str(), &widthKeyY, &heightKeyY);
        SDL::DrawText(SDL::Renderer, rootGui->FontHandle(GUI::Roboto),
                      12 + 45 * 3 + widthKeyA + widthKeyB + widthKeyX,
                      708 - heightKeyY,
                      WHITE, keyY.c_str());

        if(strncasecmp(fileExt, "xci", 3) == 0)
        {
            int widthKeyMinus  = 0;
            int heightKeyMinus = 0;
            TTF_SizeText(rootGui->FontHandle(GUI::Roboto_small), keyMinus.c_str(), &widthKeyMinus, &heightKeyMinus);
            SDL::DrawText(SDL::Renderer, rootGui->FontHandle(GUI::Roboto),
                          12 + 45 * 4 + widthKeyA + widthKeyB + widthKeyX + widthKeyY,
                          708 - heightKeyMinus,
                          WHITE, keyMinus.c_str());
        }
    }
    {
        std::string keyPlus = "(+) Quit";
        int widthKeyPlus  = 0;
        int heightKeyPlus = 0;
        TTF_SizeText(rootGui->FontHandle(GUI::Roboto_small), keyPlus.c_str(), &widthKeyPlus, &heightKeyPlus);
        SDL::DrawText(SDL::Renderer, rootGui->FontHandle(GUI::Roboto),
                      1230 - widthKeyPlus, (708 - heightKeyPlus), WHITE, keyPlus.c_str());
    }

    if(dlgInstall.IsEnabled())
        dlgInstall.Render(timer);
}

void GUIFileBrowser::Update(const double timer, const u64 kDown)
{
    if(dlgInstall.GetState() == DLGInstall::DLG_DONE)
    {
        dlgInstall.SetState(DLGInstall::DLG_IDLE);
        dlgInstall.SetEnabled(false);
        LOG("DLG done, hiding...\n");

        // Refresh folder view
        PopulateFiles(curDir, dirEntries, extFilters);
        cursor.current = std::min(cursor.current, static_cast<int>(dirEntries.size()) - 1);
    }

    if(dlgInstall.IsEnabled())
    {
        dlgInstall.Update(timer, kDown);
    }
    else
    if(dirEntries.size() > 0)
    {
        DirEntry& entry = dirEntries[cursor.current];
        const char* fileExt = GetFileExt((char*)entry.name);

        if (kDown & KEY_DUP)
        {
            cursor.current = std::max(cursor.current - 1, 0);
            if(cursor.current < cursor.top)
                cursor.top--;
        }
        else
        if (kDown & KEY_DDOWN)
        {
            cursor.current = std::min(cursor.current + 1, static_cast<int>(dirEntries.size()) - 1);
            if(cursor.current > cursor.top + FILES_PER_PAGE - 1)
                cursor.top++;
        }
        else
   		if (hidKeysHeld(CONTROLLER_P1_AUTO) & KEY_LSTICK_UP)
		{
            cursor.current = std::max(cursor.current - 1, 0);
            if(cursor.current < cursor.top)
                cursor.top--;
			wait(2);
		}
		else
        if (hidKeysHeld(CONTROLLER_P1_AUTO) & KEY_LSTICK_DOWN)
		{
            cursor.current = std::min(cursor.current + 1, static_cast<int>(dirEntries.size()) - 1);
            if(cursor.current > cursor.top + FILES_PER_PAGE - 1)
                cursor.top++;
   			wait(2);
		}

   		if (kDown & KEY_DLEFT)
        {
            cursor.current = std::max(cursor.current - FILES_PER_PAGE + 1, 0);
            if(cursor.current < cursor.top)
                cursor.top = cursor.current;
  			wait(5);
        }
		else
        if (kDown & KEY_DRIGHT)
        {
            cursor.current = std::min(cursor.current + FILES_PER_PAGE - 1, static_cast<int>(dirEntries.size()) - 1);
            if(cursor.current > cursor.top + FILES_PER_PAGE - 1)
                cursor.top = std::max(cursor.current - FILES_PER_PAGE + 1, 0);
            wait(5);
        }
        else
        if (kDown & KEY_A)
		{
		    if(entry.isDir)
            {
                // Explore
                Navigate(curDir, dirEntries[cursor.current], false);
                PopulateFiles(curDir, dirEntries, extFilters);

                // Backup cursor
                cursorStack.push_back(cursor);
                cursor.current  = 0;
                cursor.top      = 0;
            }
            else
            {
                // Install NSP or XCI
                if( (strncasecmp(fileExt, "xci", 3) == 0) ||
                    (strncasecmp(fileExt, "nsp", 3) == 0))
                {
                    dlgInstall.SetMode(DLGInstall::DLG_INSTALL);
                    dlgInstall.SetState(DLGInstall::DLG_CONFIRMATION);
                    dlgInstall.SetFilename(std::string(curDir), std::string((char*)entry.name));
                    dlgInstall.SetEnabled(true);
                }
            }
		}
		else
        if (kDown & KEY_X)
        {
		    if(entry.isDir)
            {
                // Install Extracted
                dlgInstall.SetMode(DLGInstall::DLG_INSTALL_EXTRACTED);
                dlgInstall.SetState(DLGInstall::DLG_CONFIRMATION);
                dlgInstall.SetFilename(std::string(curDir), std::string((char*)entry.name));
                dlgInstall.SetEnabled(true);
            }
            else
            {
                // Install & Delete NCP or XCI
                if( (strncasecmp(fileExt, "xci", 3) == 0) ||
                    (strncasecmp(fileExt, "nsp", 3) == 0))
                {
                    dlgInstall.SetMode(DLGInstall::DLG_INSTALL_DELETE);
                    dlgInstall.SetState(DLGInstall::DLG_CONFIRMATION);
                    dlgInstall.SetFilename(std::string(curDir), std::string((char*)entry.name));
                    dlgInstall.SetEnabled(true);
                }
            }
        }
		else
        if (kDown & KEY_Y)
        {
		    if(!entry.isDir)
            {
                // Extract NSP or XCI
                if( (strncasecmp(fileExt, "xci", 3) == 0) ||
                    (strncasecmp(fileExt, "nsp", 3) == 0))
                {
                    dlgInstall.SetMode(DLGInstall::DLG_EXTRACT);
                    dlgInstall.SetState(DLGInstall::DLG_CONFIRMATION);
                    dlgInstall.SetFilename(std::string(curDir), std::string((char*)entry.name));
                    dlgInstall.SetEnabled(true);
                }
            }
        }
        else
        if(kDown & KEY_MINUS)
        {
            if(!entry.isDir)
            {
                // Convert XCI to NSP
                if(strncasecmp(fileExt, "xci", 3) == 0)
                {
                    dlgInstall.SetMode(DLGInstall::DLG_CONVERT);
                    dlgInstall.SetState(DLGInstall::DLG_CONFIRMATION);
                    dlgInstall.SetFilename(std::string(curDir), std::string((char*)entry.name));
                    dlgInstall.SetEnabled(true);
                }
            }
        }
		else
		if ((strcmp(curDir, ROOT_PATH) != 0) && (kDown & KEY_B))
		{
			Navigate(curDir, dirEntries[cursor.current], true);
            PopulateFiles(curDir, dirEntries, extFilters);

            // Restore cursor
            cursor = cursorStack.back();
            cursorStack.pop_back();
		}
    }
}
