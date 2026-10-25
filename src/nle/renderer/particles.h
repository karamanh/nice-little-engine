/**
 * @file particles.h
 * @brief A great many small bright things, simulated entirely on the GPU.
 *
 * Stateless, which is the whole design: a particle's position, size and
 * colour are a pure function of its index and the clock. Nothing is stored
 * between frames, nothing is read back, and the CPU's entire contribution is
 * one uniform and one instanced draw call. Ten emitters of two hundred
 * particles each cost ten draw calls and no per-particle work at all.
 *
 * The cost of statelessness is that particles cannot collide, cannot be
 * spawned in bursts and cannot outlive their emitter. For fire on the end of
 * a staff, none of that is a loss: it is a loop that has always been burning
 * and always will be.
 *
 * The geometry is one quad, turned to face the camera in the vertex shader.
 * Drawn additively with depth writes off, so overlapping particles brighten
 * each other and nothing is hidden behind the ones in front of it.
 */

#pragma once

#include "render_object_3d.h"

#include <glm/glm.hpp>

namespace nle
{

/// What a puff of particles behaves like.
enum class particle_style
{
    /// Rises, narrows and fades. Fire, and anything that burns.
    flame,

    /// Drifts outward and downward, slowly, and stays soft. Frost, mist.
    cloud,

    /// Darts out and dies almost at once. Sparks, lightning.
    spark,

    /**
     * @brief Comes down from above, across the whole circle. Snow, ash.
     *
     * The one style that starts somewhere other than the emitter: particles
     * are born high and spread over the area, and fall. How high is the
     * radius and a half, so a wider fall starts higher and takes about as
     * long to arrive -- set the speed and the lifetime to match, or it will
     * either vanish in mid-air or pile through the floor.
     */
    fall,

    /**
     * @brief Thrown out from the middle, turning as it goes. An explosion.
     *
     * Everything starts at the centre and reaches the radius by the end of
     * its life, so the whole circle is swept rather than filled: what you
     * see is a front going outward, not a cloud sitting there. The turn is
     * what stops it looking like a ring of dots moving in straight lines.
     */
    swirl,

    /**
     * @brief Falls hard and straight in a narrow column. A bolt.
     *
     * Where a fall is spread over the whole circle, this is spread over
     * almost none of it: a handful of columns' worth of particles dropping
     * fast down one line. The emitter is placed where the strike should
     * land and the column arrives at it.
     */
    bolt,

    /**
     * @brief Lightning: a jagged arc that strikes, flickers out, and strikes
     *        again somewhere else after a while.
     *
     * Every particle sits on one crooked line from a radius and a half up
     * down to the emitter, and the line is a new shape every strike. Not
     * every lifetime strikes: speed is reused as the share that do, nought
     * to one, so the gaps between are uneven the way real crackling is --
     * with no timer anywhere outside the shader.
     */
    arc,

    /**
     * @brief Round and round in a flat ring, evenly spaced, twinkling, and
     *        drawn as little four-pointed stars. Stars round a dazed head.
     *
     * Nothing is born or dies: the ring is always full. Radius is the
     * ring's, and speed is how many turns a second, in radians.
     */
    orbit,

    /**
     * @brief Round and up at once: a funnel of wind. A whirlwind.
     *
     * Each particle circles the emitter's axis as it climbs, its circle
     * widening as it rises, so the whole reads as a spinning column that
     * opens at the top. Radius is the funnel's at its widest, and about how
     * tall it stands; speed is how fast it turns, in radians a second.
     */
    vortex,

    /**
     * @brief Born all round a short cylinder and turning slowly about it:
     *        mist hanging about something. Frost on a staff.
     *
     * The cylinder stands along the emitter's own up, so an emitter turned
     * to lie along a staff wraps the staff. Each fades in and out rather
     * than starting bright, so nothing reads as going off. Radius is the
     * cylinder's, and its height about twice that; speed is how fast it
     * turns, in radians a second.
     */
    mist
};

/**
 * @brief One emitter: a cloud of particles around a point.
 *
 * Placed like anything else, including by matrix, so it can be hung off a
 * bone. Its own transform is where the particles come from; they are
 * simulated in its space and so follow it without any of them being moved.
 */
class particle_emitter : public render_object_3d
{
public:
    particle_emitter();
    ~particle_emitter() override;

    /// How many at once. Cheap -- this is one number in one draw call -- but
    /// not free, since each one is still a quad's worth of overdraw.
    void set_count(size_t count);
    size_t count() const;

    void set_style(particle_style style);
    particle_style style() const;

    /// What colour they start and end. Ending darker and redder is what makes
    /// a flame look like one.
    void set_colours(const glm::vec3& born, const glm::vec3& dying);

    /// How long one lives, in seconds, and how far it gets.
    void set_lifetime(float seconds);
    void set_speed(float units_per_second);

    /// How big one is at birth, in world units.
    void set_size(float size);
    float size() const;

    /// How far from the middle they are born.
    void set_radius(float radius);

    /**
     * @brief Spreads where they are born along a line rather than a point.
     *
     * For a thing that glows along its length -- a sword's blade -- rather
     * than at one end of it. @p span is a world-space direction times how far
     * it reaches; zero puts every particle at the emitter, which is where
     * they were before this existed.
     *
     * Only where they are *born* moves. How they then travel is unchanged, so
     * a flame spread along a blade still rises rather than running down it.
     */
    void set_span(const glm::vec3& span);
    const glm::vec3& span() const;

    /// Overall brightness. Everything else being equal, this is the dial to
    /// turn when a thing should read as more enchanted.
    void set_intensity(float intensity);
    float intensity() const;

    /**
     * @brief Fewer of them the further off they are, and none past a point.
     *
     * For something small that many people may be carrying at once -- the
     * frost on a staff -- where a crowd would otherwise be a crowd's worth of
     * particles however small it is on the screen. Off by default: a spell's
     * effect is the whole point of looking across a field.
     */
    void set_thin_with_distance(bool thin);

    /**
     * @brief Laid over what is behind them rather than added to it.
     *
     * Particles add light, which is right for fire and frost and sparks and
     * means a dark one cannot be seen at all. Dark ones -- storm cloud,
     * smoke -- are drawn this way instead: their colour, as solid as the
     * intensity says, nought to one, over whatever is there.
     */
    void set_dark(bool dark);

    void render(render_command_buffer& command_buffer, const render_context& context) override;

    /// Light casts no shadow -- and drawn into the shadow pass, a fire aged
    /// twice a frame.
    bool casts_shadow() const override { return false; }

    /// Drawn after everything solid. Particles write no depth, so among the
    /// solid things they were drawn in whatever order the scene held them,
    /// and a body drawn after a staff's flame painted over it whether it
    /// stood in front of the flame or behind it.
    bool see_through() override { return true; }

private:
    /// Builds the one quad and the shader, once for the whole process.
    static void ensure_resources();

    size_t m_count = 48;
    particle_style m_style = particle_style::flame;

    glm::vec3 m_born = glm::vec3(1.0f, 0.75f, 0.25f);
    glm::vec3 m_dying = glm::vec3(0.9f, 0.15f, 0.05f);

    float m_lifetime = 1.1f;
    float m_speed = 1.0f;
    float m_size = 0.18f;
    float m_radius = 0.1f;

    /// Where they are born, spread along this from the emitter. Zero for a
    /// point, which is the ordinary case.
    glm::vec3 m_span = glm::vec3(0.0f);
    float m_intensity = 1.0f;
    bool m_thin_with_distance = false;
    bool m_dark = false;

    /// Counts up, and is what the whole simulation is a function of.
    float m_clock = 0.0f;
};

} // namespace nle
