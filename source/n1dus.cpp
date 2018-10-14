#include <common.h>
#include "n1dus.h"

using namespace NXFramework;

namespace
{

}

void n1dus::Initialize()
{
    gui.Initialize();
}

void n1dus::Shutdown()
{
    gui.Shutdown();
}

void n1dus::Update(const double timer, const u64 kDown)
{
#ifdef DEBUG
    if (kDown & KEY_ZL)
        displayOpenFiles();
#endif
    gui.Update(timer, kDown);
}

void n1dus::Render(const double timer)
{
    SDL::ClearScreen(SDL::Renderer, BACKGROUND_COL);
    gui.Render(timer);
}

int main(int argc, char **argv)
{
    try
    {
        n1dus app;
        app.Run(argc, argv);
    }
    catch (std::exception& e)
    {
        LOG_DEBUG("An error occurred:\n%s", e.what());
        u64 kDown = 0;
        while (!kDown)
        {
            hidScanInput();
            kDown = hidKeysDown(CONTROLLER_P1_AUTO);
        }
    }
    catch (...)
    {
        LOG_DEBUG("An unknown error occurred:\n");
        u64 kDown = 0;
        while (!kDown)
        {
            hidScanInput();
            kDown = hidKeysDown(CONTROLLER_P1_AUTO);
        }
    }
#ifdef DEBUG
    if(openFileCount() != 0)
    {
        LOG("Quitting n1dus: some files are still open!");
        displayOpenFiles();
        return 0;
    }
#endif
    LOG("Quitting n1dus: all good!");
    return 0;
}
