#include "audio.h"

#include "../core/files.h"
#include "../core/utils.h"

#include <algorithm>
#include <cmath>

#ifdef NLE_HAS_AUDIO
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#endif

namespace nle
{

#ifdef NLE_HAS_AUDIO

struct audio_track
{
    Mix_Music* music = nullptr;

    /// The file, read through nle::files. Music is decoded as it plays, so
    /// the bytes have to outlive it.
    std::string bytes;
};

namespace
{
    /// Forty-four one hundred, two channels, and a buffer big enough not to
    /// stutter while a frame takes longer than it meant to.
    constexpr int SAMPLE_RATE = 44100;
    constexpr int CHANNELS = 2;
    constexpr int BUFFER_SAMPLES = 2048;
}

audio::audio()
{
    if(SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
    {
        utils::prerror("audio: no audio subsystem:", SDL_GetError());
        return;
    }

    // Only the formats actually used. Asking for all of them makes the
    // startup fail on a machine missing a decoder for something never played.
    const int wanted = MIX_INIT_MP3 | MIX_INIT_OGG;

    if((Mix_Init(wanted) & MIX_INIT_MP3) == 0)
    {
        utils::prerror("audio: no mp3 decoder:", Mix_GetError());
    }

    if(Mix_OpenAudio(SAMPLE_RATE, MIX_DEFAULT_FORMAT, CHANNELS, BUFFER_SAMPLES) != 0)
    {
        utils::prerror("audio: could not open a device:", Mix_GetError());
        Mix_Quit();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return;
    }

    m_ready = true;

    utils::print("audio: ready");
}

audio::~audio()
{
    if(!m_ready)
    {
        return;
    }

    Mix_HaltMusic();

    for(auto& [path, track] : m_tracks)
    {
        if(track != nullptr)
        {
            if(track->music != nullptr)
            {
                Mix_FreeMusic(track->music);
            }

            delete track;
        }
    }

    m_tracks.clear();

    Mix_CloseAudio();
    Mix_Quit();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

audio_track* audio::track_for(const std::string& path)
{
    auto found = m_tracks.find(path);

    if(found != m_tracks.end())
    {
        return found->second;
    }

    auto bytes = files::read(path);
    auto* track = new audio_track();
    Mix_Music* loaded = nullptr;

    if(bytes)
    {
        track->bytes = std::move(*bytes);
        loaded = Mix_LoadMUS_RW(SDL_RWFromConstMem(track->bytes.data(), static_cast<int>(track->bytes.size())), 1);
    }

    if(loaded == nullptr)
    {
        utils::prerror("audio: could not load", path, ":", bytes ? Mix_GetError() : "no such file");
        delete track;

        // Remembered as a failure too, so a missing file is reported once
        // rather than on every frame that asks for it.
        m_tracks[path] = nullptr;
        return nullptr;
    }

    track->music = loaded;

    m_tracks[path] = track;

    return track;
}

void audio::apply_volume()
{
    const float level = std::clamp(m_volume, 0.0f, 1.0f)
                      * std::clamp(m_envelope, 0.0f, 1.0f);

    Mix_VolumeMusic(static_cast<int>(level * MIX_MAX_VOLUME));
}

void audio::play_music(const std::string& path, float fade_seconds)
{
    if(!m_ready || path == m_wanted)
    {
        return;
    }

    m_wanted = path;
    m_fade_rate = fade_seconds > 0.01f ? 1.0f / fade_seconds : 100.0f;

    // Nothing sounding: start straight away and rise out of silence.
    if(!Mix_PlayingMusic())
    {
        m_envelope = 0.0f;
        m_playing.clear();
    }

    m_envelope_target = 0.0f;
}

void audio::stop_music(float fade_seconds)
{
    if(!m_ready)
    {
        return;
    }

    m_wanted.clear();
    m_fade_rate = fade_seconds > 0.01f ? 1.0f / fade_seconds : 100.0f;
    m_envelope_target = 0.0f;
}

const std::string& audio::music() const
{
    return m_wanted;
}

bool audio::music_playing() const
{
    return m_ready && Mix_PlayingMusic() != 0;
}

void audio::set_music_volume(float volume)
{
    m_volume = std::clamp(volume, 0.0f, 1.0f);

    if(m_ready)
    {
        apply_volume();
    }
}

float audio::music_volume() const
{
    return m_volume;
}

void audio::update(float delta_time)
{
    if(!m_ready)
    {
        return;
    }

    // The envelope moves towards where it is going, and nothing else here
    // decides volume -- see the note in the header about not using the
    // mixer's own fades.
    const float step = m_fade_rate * delta_time;

    if(m_envelope < m_envelope_target)
    {
        m_envelope = std::min(m_envelope_target, m_envelope + step);
    }
    else if(m_envelope > m_envelope_target)
    {
        m_envelope = std::max(m_envelope_target, m_envelope - step);
    }

    // Faded out, and what is wanted is not what is on. Swap now, while it is
    // silent, which is the only moment the swap cannot be heard.
    if(m_envelope <= 0.0f && m_playing != m_wanted)
    {
        if(Mix_PlayingMusic())
        {
            Mix_HaltMusic();
        }

        m_playing = m_wanted;

        if(!m_playing.empty())
        {
            if(audio_track* track = track_for(m_playing))
            {
                // Silent before it starts, so the first buffer does not
                // arrive at full volume ahead of the rise.
                Mix_VolumeMusic(0);

                if(Mix_PlayMusic(track->music, -1) == 0)
                {
                    m_envelope_target = 1.0f;
                }
                else
                {
                    utils::prerror("audio: could not play", m_playing, ":", Mix_GetError());
                    m_playing.clear();
                    m_wanted.clear();
                }
            }
            else
            {
                // It does not exist. Forget it was asked for, so this is not
                // retried for ever.
                m_playing.clear();
                m_wanted.clear();
            }
        }
    }

    apply_volume();
}

#else // NLE_HAS_AUDIO

// No mixer on this machine. Everything is accepted and nothing is heard,
// which is a better outcome than a game that will not start.

struct audio_track
{
};

audio::audio()
{
    utils::print("audio: built without a mixer; there will be silence");
}

audio::~audio() = default;

audio_track* audio::track_for(const std::string&) { return nullptr; }
void audio::apply_volume() {}

void audio::play_music(const std::string& path, float) { m_wanted = path; }
void audio::stop_music(float) { m_wanted.clear(); }
const std::string& audio::music() const { return m_wanted; }
bool audio::music_playing() const { return false; }
void audio::set_music_volume(float volume) { m_volume = volume; }
float audio::music_volume() const { return m_volume; }
void audio::update(float) {}

#endif // NLE_HAS_AUDIO

bool audio::ready() const
{
    return m_ready;
}

} // namespace nle
