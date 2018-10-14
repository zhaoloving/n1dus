#pragma once

#include <common.h>

using namespace NXFramework;

class GUI;
class GUIComponent
{
public:
    GUIComponent(const GUI* gui): rootGui(gui) { assert(gui != nullptr); }
    virtual ~GUIComponent() {}
    virtual void Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void Render(const double timer) = 0;
    virtual void Update(const double timer, const u64 kDown) = 0;
    virtual bool IsEnabled()                 { return isEnabled; }
    virtual void SetEnabled(bool enabled)    { isEnabled = enabled; }
protected:
    bool isEnabled = true;
    const GUI* rootGui;
};

class GUIFileBrowser;
class GUI
{
public:
    enum Texture
    {
        Battery_20,
        Battery_20_charging,
        Battery_30,
        Battery_30_charging,
        Battery_50,
        Battery_50_charging,
        Battery_60,
        Battery_60_charging,
        Battery_80,
        Battery_80_charging,
        Battery_90,
        Battery_90_charging,
        Battery_full,
        Battery_full_charging,
        Battery_low,
        Battery_unknown,
        Icon_dir,
        Icon_file,
        Properties_dialog_dark,
        Properties_dialog,
        Tex_count
    };
    enum Font
    {
        Roboto_large = 0,
        Roboto,
        Roboto_small,
        Roboto_osk,
        Font_count
    };

    GUI();
    ~GUI();
    void Initialize();
    void Shutdown();
    void Render(const double timer);
    void Update(const double timer, const u64 kDown);
    SDL_Texture* TextureHandle(const GUI::Texture id) const { return textures[id]; }
    TTF_Font*    FontHandle(const GUI::Font id)       const { return fonts[id];    }

private:
    // Status Bar
    void StatusBarGetBatteryStatus(int x, int y);
    void StatusBarDisplayTime(void);

    // Resources
    void LoadTextures();
    void FreeTextures();
    void LoadFonts();
    void FreeFonts();

    SDL_Texture* textures[Texture::Tex_count];
    TTF_Font*    fonts[Font::Font_count];

    // Components
    GUIFileBrowser* guiFileBrowser;
};
