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

struct audio_clip
{
    Mix_Chunk* chunk = nullptr;
};

namespace
{
    /// Forty-four one hundred, two channels, and a buffer big enough not to
    /// stutter while a frame takes longer than it meant to.
    constexpr int SAMPLE_RATE = 44100;
    constexpr int CHANNELS = 2;
    constexpr int BUFFER_SAMPLES = 2048;

    /// Sounds at once. A fight is a great many short ones overlapping, and
    /// one that finds nowhere to play is simply not heard.
    constexpr int SOUND_CHANNELS = 32;

    /// A handle is which channel, and which time round on it.
    constexpr uint32_t CHANNEL_BITS = 8;
    constexpr uint32_t CHANNEL_MASK = (1u << CHANNEL_BITS) - 1u;

    /// Loud on the near side and quieter on the far one, but never silent
    /// on either: a sound straight ahead is at full strength in both ears.
    void place(int channel, float pan)
    {
        const float p = std::clamp(pan, -1.0f, 1.0f);
        const auto left = static_cast<Uint8>(255.0f * std::min(1.0f, 1.0f - p));
        const auto right = static_cast<Uint8>(255.0f * std::min(1.0f, 1.0f + p));

        Mix_SetPanning(channel, left, right);
    }
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

    const int channels = Mix_AllocateChannels(SOUND_CHANNELS);
    m_channel_plays.assign(static_cast<size_t>(std::max(channels, 0)), 0u);
    m_channel_volumes.assign(static_cast<size_t>(std::max(channels, 0)), 1.0f);

    utils::print("audio: ready");
}

audio::~audio()
{
    if(!m_ready)
    {
        return;
    }

    Mix_HaltMusic();
    Mix_HaltChannel(-1);

    for(auto& [path, clip] : m_clips)
    {
        if(clip != nullptr)
        {
            if(clip->chunk != nullptr)
            {
                Mix_FreeChunk(clip->chunk);
            }

            delete clip;
        }
    }

    m_clips.clear();

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

audio_clip* audio::clip_for(const std::string& path)
{
    auto found = m_clips.find(path);

    if(found != m_clips.end())
    {
        return found->second;
    }

    // Decoded whole as it loads, so the bytes need not outlive it.
    const auto bytes = files::read(path);
    Mix_Chunk* loaded = nullptr;

    if(bytes)
    {
        loaded = Mix_LoadWAV_RW(SDL_RWFromConstMem(bytes->data(), static_cast<int>(bytes->size())), 1);
    }

    if(loaded == nullptr)
    {
        utils::prerror("audio: could not load", path, ":", bytes ? Mix_GetError() : "no such file");

        // Once: a missing sound is heard about once, not on every blow.
        m_clips[path] = nullptr;
        return nullptr;
    }

    auto* clip = new audio_clip();
    clip->chunk = loaded;
    m_clips[path] = clip;

    return clip;
}

audio::sound_handle audio::play_sound(const std::string& path, float volume, float pan, bool loop)
{
    if(!m_ready || m_channel_plays.empty())
    {
        return 0;
    }

    audio_clip* clip = clip_for(path);

    if(clip == nullptr)
    {
        return 0;
    }

    // A free channel chosen first, so its loudness and place are set before
    // anything is heard on it rather than a buffer after.
    const int channel = Mix_GroupAvailable(-1);

    if(channel < 0 || static_cast<size_t>(channel) >= m_channel_plays.size())
    {
        return 0;
    }

    const auto at = static_cast<size_t>(channel);

    m_channel_volumes[at] = std::clamp(volume, 0.0f, 1.0f);
    Mix_Volume(channel, static_cast<int>(m_channel_volumes[at] * m_sound_volume * MIX_MAX_VOLUME));
    place(channel, pan);

    if(Mix_PlayChannel(channel, clip->chunk, loop ? -1 : 0) < 0)
    {
        return 0;
    }

    const uint32_t round = ++m_channel_plays[at];

    return (round << CHANNEL_BITS) | static_cast<uint32_t>(channel + 1);
}

namespace
{
    /// The channel @p sound is on if it is still the one playing there, or
    /// less than nought.
    int channel_of(audio::sound_handle sound, const std::vector<uint32_t>& plays)
    {
        const int channel = static_cast<int>(sound & CHANNEL_MASK) - 1;

        if(sound == 0 || channel < 0 || static_cast<size_t>(channel) >= plays.size()
           || plays[static_cast<size_t>(channel)] != (sound >> CHANNEL_BITS) || !Mix_Playing(channel))
        {
            return -1;
        }

        return channel;
    }
}

void audio::move_sound(sound_handle sound, float volume, float pan)
{
    if(!m_ready)
    {
        return;
    }

    const int channel = channel_of(sound, m_channel_plays);

    if(channel < 0)
    {
        return;
    }

    m_channel_volumes[static_cast<size_t>(channel)] = std::clamp(volume, 0.0f, 1.0f);
    Mix_Volume(channel, static_cast<int>(m_channel_volumes[static_cast<size_t>(channel)] * m_sound_volume
                                         * MIX_MAX_VOLUME));
    place(channel, pan);
}

void audio::stop_sound(sound_handle sound, float fade_seconds)
{
    if(!m_ready)
    {
        return;
    }

    const int channel = channel_of(sound, m_channel_plays);

    if(channel < 0)
    {
        return;
    }

    if(fade_seconds > 0.0f)
    {
        Mix_FadeOutChannel(channel, static_cast<int>(fade_seconds * 1000.0f));
    }
    else
    {
        Mix_HaltChannel(channel);
    }
}

void audio::set_sound_volume(float volume)
{
    m_sound_volume = std::clamp(volume, 0.0f, 1.0f);

    if(!m_ready)
    {
        return;
    }

    for(size_t channel = 0; channel < m_channel_volumes.size(); ++channel)
    {
        Mix_Volume(static_cast<int>(channel),
                   static_cast<int>(m_channel_volumes[channel] * m_sound_volume * MIX_MAX_VOLUME));
    }
}

float audio::sound_volume() const
{
    return m_sound_volume;
}

#else // NLE_HAS_AUDIO

// No mixer on this machine. Everything is accepted and nothing is heard,
// which is a better outcome than a game that will not start.

struct audio_track
{
};

struct audio_clip
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

audio_clip* audio::clip_for(const std::string&) { return nullptr; }
audio::sound_handle audio::play_sound(const std::string&, float, float, bool) { return 0; }
void audio::move_sound(sound_handle, float, float) {}
void audio::stop_sound(sound_handle, float) {}
void audio::set_sound_volume(float volume) { m_sound_volume = volume; }
float audio::sound_volume() const { return m_sound_volume; }

#endif // NLE_HAS_AUDIO

bool audio::ready() const
{
    return m_ready;
}

} // namespace nle
