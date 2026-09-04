#include "ui/UiSfx.hpp"



#include "media/SwitchAudout.hpp"

#include "ui/ThemeManager.hpp"

#include "util/AudioWav.hpp"



#include <borealis.hpp>



#ifdef __SWITCH__

#include <algorithm>
#include <cstring>
#include <vector>



#include "util/Logger.hpp"

#endif



namespace sf::ui {



#ifdef __SWITCH__



namespace {



struct CachedSound {

    std::vector<int16_t> pcm;

};



CachedSound gNav;

CachedSound gConfirm;

CachedSound gToggle;

CachedSound gScrapeComplete;

bool gNavEnabled = true;

float gNavVolume = 1.0f;



bool loadWavInto(const std::string& path, CachedSound& out)

{

    std::vector<int16_t> samples;

    int channels = 0;

    if (!audio::loadWavFile(path, &samples, &channels))

        return false;



    const size_t frameCount = samples.size() / static_cast<size_t>(channels);

    out.pcm.resize(frameCount);

    if (channels == 1) {

        std::memcpy(out.pcm.data(), samples.data(), samples.size() * sizeof(int16_t));

    } else {

        for (size_t i = 0; i < frameCount; ++i) {

            const int mixed = static_cast<int>(samples[i * 2]) + static_cast<int>(samples[i * 2 + 1]);

            out.pcm[i] = static_cast<int16_t>(mixed / 2);

        }

    }

    return true;

}



bool loadSfx(const char* romfsPath, const std::string& themeBase, CachedSound& out)

{

    if (!themeBase.empty()) {

        if (std::string themed = ThemeManager::instance().sfxPath(themeBase); !themed.empty()) {

            if (loadWavInto(themed, out)) {

                SF_LOG_I("UI", "SFX %s from theme: %s", themeBase.c_str(), themed.c_str());

                return true;

            }

            SF_LOG_W("UI", "Theme SFX unreadable: %s", themed.c_str());

        }

    }

    return loadWavInto(romfsPath, out);

}



void playCached(const CachedSound& sound)

{

    if (!audio::SwitchAudout::ready() || !gNavEnabled || gNavVolume <= 0.001f || sound.pcm.empty())

        return;

    audio::SwitchAudout::playSfx(sound.pcm.data(), sound.pcm.size(), gNavVolume);

}



} // namespace



#endif



void initUiSfx()

{

#ifdef __SWITCH__

    audio::SwitchAudout::init();

    reloadThemeSfx();

#else

    auto* player = brls::Application::getAudioPlayer();

    if (!player)

        return;

    player->load(brls::SOUND_FOCUS_CHANGE);

    player->load(brls::SOUND_CLICK);

    player->load(brls::SOUND_FOCUS_SIDEBAR);

#endif

}



void reloadThemeSfx()

{

#ifdef __SWITCH__

    const bool loaded =

        loadSfx("romfs:/audio/nav.wav", "nav", gNav)

        && loadSfx("romfs:/audio/confirm.wav", "confirm", gConfirm)

        && loadSfx("romfs:/audio/toggle.wav", "toggle", gToggle)

        && loadSfx("romfs:/audio/scrape_complete.wav", "scrape_complete", gScrapeComplete);

    SF_LOG_I("UI", "UiSfx ready=%d", loaded && audio::SwitchAudout::ready() ? 1 : 0);

#else

    (void)0;

#endif

}



bool isAudoutReady()

{

#ifdef __SWITCH__

    return audio::SwitchAudout::ready();

#else

    return brls::Application::getAudioPlayer() != nullptr;

#endif

}



void setNavSoundEnabled(bool enabled)

{

#ifdef __SWITCH__

    gNavEnabled = enabled;

#else

    (void)enabled;

#endif

}



bool navSoundEnabled()

{

#ifdef __SWITCH__

    return gNavEnabled;

#else

    return true;

#endif

}



void setNavSoundVolume(float volume)

{

#ifdef __SWITCH__

    gNavVolume = std::clamp(volume, 0.f, 1.f);

#else

    (void)volume;

#endif

}



float navSoundVolume()

{

#ifdef __SWITCH__

    return gNavVolume;

#else

    return 1.f;

#endif

}



void playNavSfx()

{

#ifdef __SWITCH__

    playCached(gNav);

#else

    if (navSoundEnabled())

        brls::Application::getAudioPlayer()->play(brls::SOUND_FOCUS_CHANGE, navSoundVolume());

#endif

}



void playConfirmSfx()

{

#ifdef __SWITCH__

    playCached(gConfirm);

#else

    if (navSoundEnabled())

        brls::Application::getAudioPlayer()->play(brls::SOUND_CLICK, navSoundVolume());

#endif

}



void playToggleSfx()

{

#ifdef __SWITCH__

    playCached(gToggle);

#else

    if (navSoundEnabled())

        brls::Application::getAudioPlayer()->play(brls::SOUND_FOCUS_SIDEBAR, navSoundVolume());

#endif

}



void playScrapeCompleteSfx()

{

#ifdef __SWITCH__

    playCached(gScrapeComplete);

#else

    if (navSoundEnabled())

        brls::Application::getAudioPlayer()->play(brls::SOUND_CLICK, navSoundVolume());

#endif

}



} // namespace sf::ui

