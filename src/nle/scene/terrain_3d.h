/**
 * @file terrain_3d.h
 * @author Hasan Karaman (hk@hasankaraman.dev)
 * @brief Ground: a grid mesh you can stand on, pick with the mouse and decorate.
 * @version 0.1
 * @date 2026-09-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include "../core/ray.h"
#include "../mesh/mesh_instance_3d.h"

#include <functional>
#include <vector>

namespace nle
{

class model;

/**
 * @brief One stop of the terrain's colour ramp.
 *
 * Between two stops the colour is interpolated, so a handful of these is
 * enough to get sand into grass into rock into snow.
 */
struct terrain_layer
{
    /// Height, in the terrain's local space, at which this colour is reached.
    float height = 0.0f;

    glm::vec3 color = glm::vec3(1.0f);
};

/**
 * @brief Fractal noise settings for a generated heightfield.
 *
 * The defaults give gentle, rolling ground rather than mountains, on the
 * assumption that something has to walk over it.
 */
struct terrain_noise
{
    int seed = 1337;

    /// Feature size, per world unit. Smaller is broader.
    float frequency = 0.02f;

    int octaves = 4;
    float lacunarity = 2.0f;
    float gain = 0.5f;

    /// Peak height above the base. The field spans roughly [-amplitude, amplitude].
    float amplitude = 6.0f;

    /// A level circle at the centre, so there is somewhere to spawn and build.
    /// Zero disables it.
    float flat_radius = 0.0f;

    /// Distance over which that circle blends back into the hills.
    float flat_falloff = 12.0f;

    /// Pulls the outside edge of the patch down to the base height, so the map
    /// ends in a rim instead of a cliff. Zero disables it.
    float edge_falloff = 0.0f;
};

/**
 * @brief Something that can be painted onto the ground: a road, a path, sand.
 *
 * A colour, and optionally a texture to go with it. The weights were always
 * exactly what a splat map holds, so adding pictures turned out to be a
 * change of how they are drawn rather than of what is stored -- every level
 * painted before there were any textures still paints the same.
 *
 * The texture is multiplied by the colour, so a layer with no texture is a
 * flat colour exactly as it was, and a layer with one can be tinted without
 * needing a second copy of the file.
 */
struct terrain_paint_layer
{
    std::string name;
    glm::vec3 color = glm::vec3(0.5f);

    /// Path to a picture, or empty for a flat colour. Loaded by whoever
    /// sets the layers; the terrain is handed the texture itself.
    std::string texture;

    /// How many times it repeats across the whole terrain. Ground textures
    /// are small and tile, so this is usually a good deal more than one.
    float tiling = 24.0f;
};

/// What a brush stroke does to the ground under it.
enum class sculpt_mode
{
    /// Pulls the surface up.
    raise,

    /// Pushes it down.
    lower,

    /// Averages each sample with its neighbours, wearing edges off.
    smooth,

    /// Pulls everything towards a given height, which is how a building gets
    /// somewhere level to stand.
    flatten
};

/**
 * @brief Rules for sprinkling props across the surface.
 *
 * Placements that fail the filters are retried rather than nudged, so a strict
 * filter thins the scatter out instead of bunching props against its edge.
 */
struct terrain_scatter
{
    int seed = 1337;

    /// How many props to aim for. Fewer are placed if the filters are tight.
    int count = 100;

    /// Steepest ground a prop will stand on, in degrees.
    float max_slope = 90.0f;

    /// Height band, in world space, the prop is allowed in.
    float min_height = -1e9f;
    float max_height = 1e9f;

    /// Radius around the terrain's centre left clear.
    float clear_radius = 0.0f;

    /// Strip along the outside edge left clear, in world units.
    float margin = 0.0f;

    /**
     * @brief Uniform scale, drawn from [x, y].
     *
     * Multiplied into whatever scale the prop already has rather than replacing
     * it, so a factory can shape a prop -- flatten a box into a slab, say -- and
     * still have scatter vary its size.
     */
    glm::vec2 scale_range = glm::vec2(1.0f, 1.0f);

    /// Random rotation about Y.
    bool random_yaw = true;

    /**
     * @brief Leans the prop with the ground instead of standing it upright.
     *
     * Exact for ground that falls away along Z, and an approximation once yaw
     * and a sideways lean are combined -- the object's euler angles are applied
     * X, then Y, then Z, which is the wrong order to compose a tilt on top of a
     * yaw. Close enough for rocks and bushes on gentle ground; stand anything
     * tall upright instead.
     */
    bool align_to_normal = false;

    /// Placement attempts per prop before that prop is given up on.
    int attempts_per_prop = 24;
};

/**
 * @brief A square patch of ground, centred on the object's position.
 *
 * Flat by default. set_noise() generates rolling ground, or set_height_function()
 * supplies your own surface; the mesh, its normals, height_at() and picking all
 * follow from that one function, so what you see and what you pick cannot
 * disagree.
 *
 *     auto ground = make_ref<terrain_3d>(256.0f, 128);
 *     ground->set_noise({ .frequency = 0.012f, .amplitude = 9.0f, .flat_radius = 20.0f });
 *     ground->set_layers({ { 0.0f, sand }, { 2.0f, grass }, { 7.0f, rock } });
 *     scene->add_child(ground);
 *
 * Decoration goes through decorate() and scatter(), which place props on the
 * surface and hand them to the scene the terrain belongs to. They are put in
 * the scene rather than parented to the terrain because the renderer draws the
 * scene's own render objects and does not walk further down the tree -- a prop
 * added with terrain->add_child() would never be drawn. Order does not matter:
 * decorating a terrain that is not in a scene yet holds the props until it is.
 */
class terrain_3d : public mesh_instance_3d
{
public:
    /**
     * @param size       world units along each edge
     * @param resolution tiles along each edge; vertices are not shared between
     *                   tiles, so the checker pattern has hard edges
     * @param detail_distance see set_detail_distance; given here, nothing is
     *                   built until the ground is first drawn
     */
    terrain_3d(float size = 100.0f, int resolution = 50, float detail_distance = 0.0f);
    virtual ~terrain_3d();

    /**
     * @brief Sets the surface height, sampled in the terrain's local XZ space,
     *        and rebuilds the mesh.
     *
     * Pass nullptr to go back to flat, which also makes picking exact rather
     * than iterative.
     */
    void set_height_function(std::function<float(float, float)> height);

    // ---- sculpting ------------------------------------------------------

    /// Height samples along each edge. One more than the tile resolution,
    /// since a row of tiles has a sample at each end.
    int samples() const;

    /**
     * @brief The editable heightfield, row-major from -half to +half.
     *
     * Empty until something sculpts or bakes. While it is empty the surface
     * comes from the height function instead, which is what noise sets.
     */
    const std::vector<float>& heightmap() const;

    /// Replaces it wholesale. Wrong-sized input is refused.
    bool set_heightmap(std::vector<float> heights);

    /// Whether the surface comes from the heightfield rather than a function
    /// -- which is to say, whether it has been shaped by hand.
    bool sculpted() const;

    /**
     * @brief Samples the current surface into the heightfield.
     *
     * Generation and sculpting meet here: noise lays the ground out, this
     * freezes it, and from then on a brush has something to push around.
     * Called for you the first time anything sculpts.
     */
    void bake_heightmap();

    /// Throws the heightfield away, going back to whatever the function says.
    void clear_heightmap();

    /**
     * @brief Applies one brush stroke, centred on a world position.
     *
     * The brush falls off towards its edge, so overlapping strokes build up
     * smoothly rather than in discs. @p strength is in world units per second
     * for raise and lower, and a rate of approach for smooth and flatten.
     *
     * @param level the height flatten pulls towards. Ignored by the others.
     * @return whether anything moved, so a caller can skip rebuilding.
     */
    bool sculpt(const glm::vec3& center, float radius, float strength,
                sculpt_mode mode, float delta_time, float level = 0.0f);

    // ---- painting -------------------------------------------------------

    /**
     * @brief The colours this terrain can be painted with.
     *
     * Changing the set keeps whatever weights still have a layer to belong
     * to, so renaming or recolouring one does not wipe the painting.
     */
    void set_paint_layers(std::vector<terrain_paint_layer> layers);
    const std::vector<terrain_paint_layer>& paint_layers() const;

    /**
     * @brief The picture under everything, before anything is painted on.
     *
     * Null for ground shaded by colour alone, which is what every terrain
     * was before this existed and what one still is until given something.
     */
    void set_base_texture(ref<class texture> picture, float tiling = 24.0f);
    ref<class texture> base_texture() const;

    /**
     * @brief The picture for one paint layer.
     *
     * Kept beside the layers rather than inside them because a
     * terrain_paint_layer is data a level file holds, and a texture is a
     * thing on a graphics card. The file says which picture; this is the
     * picture.
     */
    void set_layer_texture(size_t layer, ref<class texture> picture);

    /// How many layers one splat map can carry: four, one to a channel.
    static constexpr size_t LAYERS_TO_A_SPLATMAP = 4;

    /// How many splat maps there are, and so how many pictures the ground
    /// can wear. Two is not a limit anybody chose -- it is what fits in the
    /// texture units left over once the mesh, the base and the shadow map
    /// have theirs -- and a third would cost only another sampler.
    static constexpr size_t SPLATMAPS = 2;

    /// The most layers that can have pictures.
    static constexpr size_t MOST_TEXTURED_LAYERS = LAYERS_TO_A_SPLATMAP * SPLATMAPS;

    /// One weight per sample per layer, 0 to 255, layer-major within a sample.
    const std::vector<uint8_t>& paintmap() const;
    bool set_paintmap(std::vector<uint8_t> weights);

    /// Whether anything has been painted.
    bool painted() const;

    /**
     * @brief Brushes @p layer onto the ground, or takes it off again.
     *
     * Layers are painted over one another in order, so a road laid after
     * grass covers it. Erasing thins the layer rather than revealing a
     * particular one underneath, which is what makes it behave like a brush.
     *
     * @return whether anything changed.
     */
    bool paint(const glm::vec3& center, float radius, int layer, float strength,
               float delta_time, bool erase = false);

    /// The strongest layer at a world position, or -1 where nothing is
    /// painted. What gameplay would ask to know it is standing on a road.
    int paint_at(float x, float z) const;

    /**
     * @brief Generates a fractal heightfield, replacing any height function.
     */
    void set_noise(const terrain_noise& noise);
    const terrain_noise& noise() const;

    /// Whether the current surface came from set_noise().
    bool generated() const;

    /**
     * @brief Draws the ground, with its own pictures if it has any.
     *
     * Everything a mesh instance does, and then the handful of uniforms and
     * samplers that turn the ground's half of the shader on. Nothing else
     * sets them, so nothing else pays for them.
     */
    void render(render_command_buffer& command_buffer, const render_context& context) override;

    void record_extra_uniforms(render_command_buffer& command_buffer,
                               const render_context& context) override;

    /**
     * @brief The painting as a sampler reads it: one texel per sample, one
     *        layer per channel, in the layers' own order.
     *
     * Built on demand, because a brush stroke touches the same ground many
     * times a second and only the last of those is ever seen. Null when no
     * layer has a picture, since then nothing would read it.
     */
    /**
     * @brief The painting as a sampler reads it.
     *
     * @p which selects the map: the first carries layers nought to three and
     * the second the four after that, a channel to a layer.
     */
    ref<class texture> splatmap(size_t which = 0);

    /**
     * @brief The ground seen from above, as RGBA pixels, @p side to a side.
     *
     * For a map. Every paint layer is mixed in by its colour, pictures
     * included, so a gravel road and a painted one both read as a road --
     * a map is made of colours, and telling one place from another is the
     * whole of what it is for. Shaded a little by how the ground faces, or a
     * hilly place comes out as one flat wash.
     *
     * Built on demand and not kept: it is wanted once, when a level is
     * entered, and holding it would be holding a picture of ground that can
     * be sculpted under it.
     */
    std::vector<uint8_t> overhead_image(int side) const;

    /// Alternating tile colours. Used only while no layers are set.
    void set_colors(const glm::vec3& first, const glm::vec3& second);

    /**
     * @brief Colours the ground by height, replacing the checkerboard.
     *
     * Heights are in the terrain's local space, so they line up with
     * terrain_noise::amplitude. The stops are sorted for you. Pass an empty
     * list to go back to the checkerboard.
     */
    void set_layers(std::vector<terrain_layer> layers);
    const std::vector<terrain_layer>& layers() const;

    /**
     * @brief Blends @p color in where the ground is steeper than @p angle.
     *
     * Exposed rock on hillsides, in other words. @p blend is the half-width of
     * the transition, in degrees.
     */
    void set_cliff(const glm::vec3& color, float angle = 38.0f, float blend = 12.0f);
    void clear_cliff();

    void rebuild();

    // ---- level of detail --------------------------------------------------

    /**
     * @brief Draws the ground in patches, each as fine as its distance asks.
     *
     * One mesh the size of the map is every triangle of the map, every
     * frame, wherever the camera is looking: half a million of them for a
     * town two kilometres across, nearly all behind the eye or too far off
     * to tell from a quarter as many. In patches, the ones off the screen
     * are not drawn at all, and the rest are drawn at full detail only out
     * to @p distance, at half out to twice that, a quarter out to four
     * times, and an eighth beyond.
     *
     * Each patch hangs a skirt from its edges, so where a fine patch meets
     * a coarse one the gap between their edges shows ground, not sky.
     *
     * Nought, the default, is one mesh as ever -- which is what sculpting
     * wants, since a brush stroke then rewrites a few hundred vertices in
     * place. Only ground that shares its vertices is drawn in patches: the
     * checkerboard is one mesh whatever is asked.
     */
    void set_detail_distance(float distance);
    float detail_distance() const;

    /// How many patches were drawn last frame, and of how many triangles:
    /// for seeing what the above is saving.
    size_t patches_drawn() const;
    size_t triangles_drawn() const;

    float size() const;
    int resolution() const;

    /// Height of the surface under a world-space XZ position.
    float height_at(float x, float z) const;

    /// Surface normal under a world-space XZ position.
    glm::vec3 normal_at(float x, float z) const;

    /// Angle between the surface and horizontal, in degrees.
    float slope_at(float x, float z) const;

    /// Whether a world-space XZ position is over the patch at all.
    bool contains(float x, float z) const;

    /// Copy of @p position moved vertically onto the surface.
    glm::vec3 place_on_surface(const glm::vec3& position) const;

    /**
     * @brief How far from the middle anybody may walk, or nought for no limit.
     *
     * A circle rather than the square the mesh is, because the ground worth
     * playing on is the flat middle and what is past it is scenery. A square
     * edge means the corners are three quarters as far away again as the
     * sides, and a player who walks into one has found the end of the world
     * at a different distance depending which way they set off.
     */
    void set_bounds_radius(float radius);
    float bounds_radius() const;

    /**
     * @brief @p position, brought inside the edge if it is outside it.
     *
     * Height is left alone: what this decides is where somebody may stand,
     * not how high the ground is there.
     */
    glm::vec3 inside_bounds(const glm::vec3& position) const;

    /**
     * @brief Sits @p prop on the ground at @p position and adds it to the scene.
     *
     * The prop's Y is taken from the surface; its X and Z are used as given.
     */
    void decorate(ref<render_object_3d> prop, const glm::vec3& position);

    /**
     * @brief Sprinkles instances of @p prop over the surface.
     * @return the props actually placed, which may be fewer than count.
     */
    std::vector<ref<render_object_3d>> scatter(ref<model> prop, const terrain_scatter& settings);

    /**
     * @brief As above, but you build each prop.
     *
     * @p factory is called with the index of the prop being placed and may
     * return nullptr to skip it, which is how you mix several kinds of prop
     * into one scatter.
     */
    std::vector<ref<render_object_3d>> scatter(const std::function<ref<render_object_3d>(int)>& factory,
                                               const terrain_scatter& settings);

    /// Everything decorate() and scatter() have placed.
    const std::vector<ref<render_object_3d>>& decorations() const;

    /// Removes them all from the scene.
    void clear_decorations();

    /**
     * @brief Where @p r first meets the ground.
     *
     * Flat terrain is solved directly. An uneven one is marched along the ray
     * until the ray drops below the surface, then bisected -- good enough for
     * picking a spot to aim a spell at, and it needs no acceleration structure.
     *
     * @return false if the ray never meets the patch.
     */
    bool raycast(const ray& r, glm::vec3& hit) const;

protected:
    /// Picks up props that were scattered before the terrain joined a scene.
    void set_scene(ref<render_object_3d> scene) override;

private:
    /// The weights as a picture the shader can read: one channel per
    /// textured layer. Rebuilt when the painting changes, and only when
    /// there is something textured to draw with it.
    void refresh_splatmap();

    float m_size;
    int m_resolution;

    glm::vec3 m_first_color = glm::vec3(0.42f, 0.44f, 0.38f);
    glm::vec3 m_second_color = glm::vec3(0.33f, 0.35f, 0.30f);

    std::vector<terrain_layer> m_layers;

    bool m_has_cliff = false;
    glm::vec3 m_cliff_color = glm::vec3(0.40f, 0.38f, 0.35f);
    float m_cliff_angle = 38.0f;
    float m_cliff_blend = 12.0f;

    std::function<float(float, float)> m_height;

    /// (samples * samples) heights, row-major. Empty means "use m_height".
    std::vector<float> m_heights;

    std::vector<terrain_paint_layer> m_paint_layers;

    /// (samples * samples * layers) weights, layer-major within a sample.
    std::vector<uint8_t> m_paint;

    /// The pictures. Separate from m_paint_layers, which is level data.
    ref<class texture> m_base_texture;
    ref<class texture> m_layer_textures[MOST_TEXTURED_LAYERS];

    /// The weights, uploaded. Null until something textured wants it.
    ref<class texture> m_splatmap[SPLATMAPS];

    float m_base_tiling = 24.0f;

    /// Nought means the whole square, which is what every level said before
    /// there was an edge.
    float m_bounds_radius = 0.0f;

    /// Set when the painting has moved on from what the splat map holds.
    bool m_splat_stale = true;

    /// Index of the first weight of a sample, or a size when given the count.
    size_t paint_index(int x, int z) const;

    /// Bilinear sample of the heightfield, in local space.
    float sample_heightmap(float x, float z) const;

    /**
     * @brief Whether the ground is anything other than the plane y = 0.
     *
     * Either a height function or a sculpted field counts. Asking about only
     * one of them is how a hand-made hill ends up drawn but not collided
     * with: the mesh comes from local_height(), which knows about both, while
     * anything testing m_height alone quietly takes the flat path.
     */
    bool has_surface() const;

    /**
     * @brief Builds the mesh as a shared-vertex grid.
     *
     * Which is what sculpting needs: a stroke then rebuilds (resolution + 1)^2
     * vertices rather than four per tile. At a resolution fine enough to sculpt
     * at, that is the difference between a brush that follows the mouse and one
     * that does not. The checkerboard still uses the old builder, since hard
     * tile edges are the one thing shared vertices cannot do.
     */
    ref<class mesh_3d> build_grid_mesh() const;

    /// Whether m_mesh is a grid, and so can be updated a patch at a time.
    bool m_mesh_is_grid = false;

    // ---- level of detail --------------------------------------------------

    /// Tiles along a patch's edge, and how many times it can be halved.
    static constexpr int PATCH_TILES = 32;
    static constexpr int DETAIL_LEVELS = 4;

    /// One square of the ground with its own vertices: the same ones at
    /// every level of detail, read every one, every other, every fourth...
    struct patch
    {
        unsigned int vertices = 0;

        /// One to a level, since which indices a vertex array draws with is
        /// part of the array.
        unsigned int arrays[DETAIL_LEVELS] = {};

        /// A ball round it, in the terrain's own space, for asking whether
        /// the camera can see it; and the box it fills, for how far off it is.
        glm::vec3 centre = glm::vec3(0.0f);
        float radius = 0.0f;
        glm::vec3 low = glm::vec3(0.0f);
        glm::vec3 high = glm::vec3(0.0f);
    };

    std::vector<patch> m_patches;

    /// The triangles of a patch at each level: the same for every patch,
    /// since each numbers its vertices the same way.
    unsigned int m_patch_indices[DETAIL_LEVELS] = {};
    size_t m_patch_index_counts[DETAIL_LEVELS] = {};

    ref<class material> m_patch_material;

    float m_detail_distance = 0.0f;

    /// Set when the ground has changed since the patches were built. They
    /// are built when next drawn: a level being loaded changes the ground
    /// several times over before anybody sees it.
    bool m_patches_stale = true;

    size_t m_patches_drawn = 0;
    size_t m_triangles_drawn = 0;

    /// Whether the ground is drawn in patches rather than as one mesh.
    bool in_patches() const;

    void build_patches();
    void free_patches();

    void render_patches(render_command_buffer& command_buffer, const render_context& context);

    /**
     * @brief Re-does the vertices in a rectangle of samples, in place.
     *
     * What makes a brush usable: a stroke touches a few hundred vertices, and
     * rebuilding the mesh to report that costs more than the stroke did. The
     * rectangle is widened by one sample on each side, because a vertex's
     * normal is taken from the heights either side of it and those change too.
     */
    void refresh_region(int min_x, int max_x, int min_z, int max_z);

    terrain_noise m_noise;
    bool m_generated = false;

    std::vector<ref<render_object_3d>> m_decorations;

    /// Local height, before the object's own transform.
    float local_height(float x, float z) const;

    /// Local normal, from central differences over the height function.
    glm::vec3 local_normal(float x, float z) const;

    /// Colour of the surface at a local position, from the layers and the cliff.
    glm::vec3 surface_color(int tile_x, int tile_z, float local_y, const glm::vec3& normal) const;

    /// The ground's colour before any paint: the checkerboard, or the height
    /// layers, with the cliff colour worked in.
    glm::vec3 ground_tone(int tile_x, int tile_z, float local_y, const glm::vec3& normal) const;

    /**
     * @brief The paint at a sample, mixed over @p over in the layers' order.
     *
     * @param skip_textured leaves out layers drawn as pictures, which is what
     *        the mesh wants -- their colour would show through the picture.
     *        A map wants them in: it is made of colours and has no picture
     *        for them to show through.
     */
    glm::vec3 painted_tone(int tile_x, int tile_z, const glm::vec3& over,
                           bool skip_textured) const;

    /// Adds a prop to the scene, or holds it until there is one.
    void attach(ref<render_object_3d> prop);

    ref<class mesh_3d> build_mesh() const;
};

} // namespace nle
