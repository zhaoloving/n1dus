#include "app.h"
#include "common.h"
#include <Tinfoil/include/nx/ipc/tin_ipc.h>


namespace NXFramework
{
App* GlobalApp = nullptr;

void startNXLink(int argc, char **argv)
{
    socketInitializeDefault();
    LOG("%d arguments\n", argc);
    for (int i=0; i<argc; ++i)
    {
        LOG("argv[%d] = %s\n", i, argv[i]);
    }
    LOG("nxlink host is %s\n", inet_ntoa(__nxlink_host));
    nxlinkStdio();
    LOG("printf output now goes to nxlink server\n");
}

void stopNXLink()
{
    socketExit();
}

App::App()
{
    GlobalApp = this;
}

App::~App()
{

}

void App::TimerUpdate()
{
    static u64 currentTime  = 0;
    static u64 lastTime     = 0;

    lastTime    = currentTime;
    currentTime = SDL_GetPerformanceCounter();
    deltaTime   = (double)((currentTime - lastTime) * 1000 / SDL_GetPerformanceFrequency());
}

u32 App::Run(int argc, char **argv)
{
    try
    {
        #ifdef DEBUG
        startNXLink(argc, argv);
        #endif // DEBUG

        Initialize_Internal();
        while(appletMainLoop() && isAlive)
        {
            TimerUpdate();
            Update_Internal();
            Render_Internal();
        }

        #ifdef DEBUG
        stopNXLink();
        #endif

    }
    catch(...)
    {
        LOG("App::Run Exception!\n");
        return -1;
    }
    Shutdown_Internal();
    return 0;
}

void App::Exit()
{
}

void App::Initialize_Internal()
{
    if (R_FAILED(ncmInitialize()))    LOG("ncmInitialize failed!\n");
    if (R_FAILED(nsInitialize()))     LOG("nsInitialize failed!\n");
    if (R_FAILED(nsextInitialize()))  LOG("nsextInitialize failed!\n");
    if (R_FAILED(esInitialize()))     LOG("esInitialize failed!\n");
    if (R_FAILED(nifmInitialize()))   LOG("nifmInitialize failed!\n");
    if (R_FAILED(psmInitialize()))    LOG("psmInitialize failed!\n");
    if (R_FAILED(setInitialize()))    LOG("setInitialize failed!\n");
    if (R_FAILED(plInitialize()))     LOG("plInitialize failed!\n");
    if (R_FAILED(romfsInit()))        LOG("romfsInit failed!\n");
    if (R_FAILED(timeInitialize()))   LOG("timeInitialize failed!\n");

    // SDL
	SDL_Init(SDL_INIT_EVERYTHING);
    SDL::Initialize(1280, 720);

	// IMG
	IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);

	// TTF
	TTF_Init();

    Initialize();
}

void App::Shutdown_Internal()
{
    Shutdown();

    // TTF
    TTF_Quit();

	// IMG
	IMG_Quit();

	// SDL
	SDL::Shutdown();
	SDL_Quit();

	timeExit();
	romfsExit();
    plExit();
    setExit();
    psmExit();
    nifmExit();
    esExit();
    nsextExit();
    nsExit();
    ncmExit();
}

void App::Render_Internal()
{
    SDL::BeginFrame();
    Render(deltaTime);
    SDL::EndFrame();
}

void App::Update_Internal()
{
    hidScanInput();
    u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO);
    #ifdef DEBUG
    DebugController(kDown);
    #endif
    // a bit hacky, but have to prevent hard quitting during install
    // (workerThread.running) to prevent exFat corruption
    // we could join threads but it would stall the UI update,
    // which is not in a thread right now (not sure SDL is thread safe, investigate)
    if ((kDown & KEY_PLUS) && !workerThread.running) isAlive = false;
    Update(deltaTime, kDown);
}

 void App::DebugController(const u64 kDown)
 {
    return;
    if (kDown & KEY_A)              LOG("A Pressed\n");
    if (kDown & KEY_B)              LOG("B Pressed\n");
    if (kDown & KEY_X)              LOG("X Pressed\n");
    if (kDown & KEY_Y)              LOG("Y Pressed\n");
    if (kDown & KEY_LSTICK)         LOG("Left Stick Pressed\n");
    if (kDown & KEY_RSTICK)         LOG("Right Stick Pressed\n");
    if (kDown & KEY_L)              LOG("L Pressed\n");
    if (kDown & KEY_R)              LOG("R Pressed\n");
    if (kDown & KEY_ZL)             LOG("ZL Pressed\n");
    if (kDown & KEY_ZR)             LOG("ZR Pressed\n");
    if (kDown & KEY_PLUS)           LOG("Plus Pressed\n");
    if (kDown & KEY_MINUS)          LOG("Minus Right Pressed\n");
    if (kDown & KEY_DDOWN)          LOG("D-Pad Down Pressed\n");
    if (kDown & KEY_DUP)            LOG("D-Pad Up Pressed\n");
    if (kDown & KEY_DLEFT)          LOG("D-Pad Left Pressed\n");
    if (kDown & KEY_DRIGHT)         LOG("D-Pad Right Pressed\n");
    if (kDown & KEY_LSTICK_LEFT)    LOG("Left Stick Left Pressed\n");
    if (kDown & KEY_LSTICK_UP)      LOG("Left Stick Up Pressed\n");
    if (kDown & KEY_LSTICK_RIGHT)   LOG("Left Stick Right Pressed\n");
    if (kDown & KEY_LSTICK_DOWN)    LOG("Left Stick Down Pressed\n");
    if (kDown & KEY_RSTICK_LEFT)    LOG("Right Stick Left Pressed\n");
    if (kDown & KEY_RSTICK_UP)      LOG("Right Stick Up Pressed\n");
    if (kDown & KEY_RSTICK_RIGHT)   LOG("Right Stick Right Pressed\n");
    if (kDown & KEY_RSTICK_DOWN)    LOG("Right Stick Down Pressed\n");
    if (kDown & KEY_SL)             LOG("SL Pressed\n");
    if (kDown & KEY_SR)             LOG("SR Pressed\n");
    if (kDown & KEY_UP)             LOG("D-Pad Up or Sticks Up Pressed\n");
    if (kDown & KEY_DOWN)           LOG("D-Pad Down or Sticks Down Pressed\n");
    if (kDown & KEY_LEFT)           LOG("D-Pad Left or Sticks Left Pressed\n");
    if (kDown & KEY_RIGHT)          LOG("D-Pad Right or Sticks Right Pressed\n");
 }

}
