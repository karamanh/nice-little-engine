/**
 * @file audio.h
 * @brief Sound out of the machine, and one piece of music at a time.
 *
 * A game wants a theme that starts when you walk into a town and is gone by
 * the time you are fighting something. That is two questions -- what should
 * be sounding, and how loudly -- and only the second one is continuous, so
 * this keeps them apart: ask for a track and it takes care of handing over
 * from whatever was playing, and set the volume every frame if you like.
 *
 * Why one track: the mixer underneath plays a single music stream, so a true
 * crossfade is not on offer. What happens instead is a fall to silence and a
 * rise back out of it, which is what a change of place sounds like anyway.
 *
 * Volume is this class's alone. The mixer's own fade helpers are not used,
 * because they fight with anything that sets the volume while they run, and
 * the whole point here is that the game sets the volume while they run.
 *
 * Short sounds -- a blow, a bowstring, a crackle -- play over the music on
 * channels of their own, each at its own loudness and place between the two
 * ears. Where a sound is in the world is the game's to work out: this only
 * hears how loud and how far left.
 *
 * Built without a device, or built on a machine with no mixer at all, every
 * call is a no-op and ready() says so. Silence is a perfectly good outcome;
 * refusing to start is not.
 */

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace nle
{

/// Opaque so that no header of a game's has to see the mixer's.
struct audio_track;
struct audio_clip;

class audio
{
public:
    audio();
    ~audio();

    audio(const audio&) = delete;
    audio& operator=(const audio&) = delete;

    /// Whether there is a device. False means every call below does nothing.
    bool ready() const;

    /**
     * @brief Asks for @p path to be the music.
     *
     * Asking for what is already playing does nothing, so this is safe to
     * call every frame -- and is meant to be, since what should be playing
     * is usually a question about where somebody is standing.
     *
     * @param fade_seconds how long the handover takes, each way.
     */
    void play_music(const std::string& path, float fade_seconds = 2.0f);

    /// Fades out and stays out.
    void stop_music(float fade_seconds = 2.0f);

    /// What is playing or on its way in, which is what was last asked for.
    const std::string& music() const;

    /// Whether anything is actually sounding, fade included.
    bool music_playing() const;

    /**
     * @brief How loud, from 0 to 1, applied at once.
     *
     * Smoothing belongs to the caller: only the caller knows whether the
     * volume is dropping because a fight started or because somebody moved
     * a slider, and those should not sound the same.
     */
    void set_music_volume(float volume);
    float music_volume() const;

    /// Drives the handover. Call once a frame.
    void update(float delta_time);

    /// A sound playing, to change or stop later. Nought is none.
    using sound_handle = uint32_t;

    /**
     * @brief Plays @p path once, or over and over with @p loop, at
     *        @p volume from 0 to 1 and @p pan from -1 (all left) to 1.
     *
     * Loaded the first time and kept, decoded: a sound is heard again and
     * again and is a few kilobytes. WAV and OGG. When every channel is
     * already sounding it is simply not heard -- in that much noise, nobody
     * will miss it.
     *
     * @return what to stop it with, or nought if it is not playing.
     */
    sound_handle play_sound(const std::string& path, float volume = 1.0f, float pan = 0.0f, bool loop = false);

    /// Moves a sound already playing: how loud, and how far left or right.
    void move_sound(sound_handle sound, float volume, float pan);

    /// Fades @p sound out over @p fade_seconds and stops it. A sound that
    /// has already ended, or whose channel has moved on, is left alone.
    void stop_sound(sound_handle sound, float fade_seconds = 0.08f);

    /// How loud every sound is, from 0 to 1, on top of each one's own.
    void set_sound_volume(float volume);
    float sound_volume() const;

private:
    void apply_volume();

    /// Loaded once and kept: a track is a few megabytes of mp3 and walking
    /// in and out of a town should not decode it again each time.
    audio_track* track_for(const std::string& path);

    bool m_ready = false;

    /// What was asked for, and what is actually sounding. They differ for as
    /// long as a handover takes.
    std::string m_wanted;
    std::string m_playing;

    /// The handover envelope, 0 to 1, and how fast it moves.
    float m_envelope = 0.0f;
    float m_envelope_target = 0.0f;
    float m_fade_rate = 0.5f;

    float m_volume = 1.0f;

    std::map<std::string, audio_track*> m_tracks;

    audio_clip* clip_for(const std::string& path);

    /// Loaded once each, or nullptr for one that would not load.
    std::map<std::string, audio_clip*> m_clips;

    float m_sound_volume = 1.0f;

    /// How many times each channel has been started, so an old handle to a
    /// channel now playing something else stops nothing.
    std::vector<uint32_t> m_channel_plays;

    /// Each channel's own loudness before the master, to apply it again
    /// when the master moves.
    std::vector<float> m_channel_volumes;
};

} // namespace nle
