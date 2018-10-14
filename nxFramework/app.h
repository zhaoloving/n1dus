#pragma once

#include <common.h>

namespace NXFramework
{

class App
{
public:
    App();
    virtual ~App();

    u32 Run(int argc, char **argv);

protected:
    virtual void Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void Update(const double deltaT, const u64 kDown) = 0;
    virtual void Render(const double deltaT) = 0;

    void Exit();
private:
    void Initialize_Internal();
    void Shutdown_Internal();
    void Update_Internal();
    void Render_Internal();

    void TimerUpdate();
    void DebugController(const u64 kDown);

    bool    isAlive     = true;
    double  deltaTime   = 0;
};

extern App* GlobalApp;

}
