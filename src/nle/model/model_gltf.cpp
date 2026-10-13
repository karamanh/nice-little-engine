#include "model_gltf.h"
#include "gltf_instance_3d.h"

#include "../animation/retarget.h"

#include "../core/files.h"
#include "../core/utils.h"

// tinygltf pulls in stb_image and nlohmann's json. The stb implementation comes
// from the system libstb we already link against, exactly as texture.cpp does,
// so only the declarations are wanted here. We never write glTF files.
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "../../vendor/tiny_gltf.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

namespace nle
{

namespace
{

/// Values of one accessor, widened to float, with its component count.
struct accessor_values
{
    std::vector<float> values;
    int components = 0;
    size_t count = 0;

    bool empty() const { return count == 0 || components == 0; }

    glm::vec4 at(size_t index, const glm::vec4& fallback = glm::vec4(0.0f)) const
    {
        if(index >= count)
        {
            return fallback;
        }

        glm::vec4 result = fallback;
        const size_t base = index * static_cast<size_t>(components);

        for(int c = 0; c < components && c < 4; ++c)
        {
            result[c] = values[base + static_cast<size_t>(c)];
        }

        return result;
    }
};

/// Reads one component, honouring the accessor's normalized flag.
float read_component(const unsigned char* pointer, int component_type, bool normalized)
{
    switch(component_type)
    {
        case TINYGLTF_COMPONENT_TYPE_FLOAT:
        {
            float value = 0.0f;
            std::memcpy(&value, pointer, sizeof(float));
            return value;
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        {
            const uint8_t value = *pointer;
            return normalized ? static_cast<float>(value) / 255.0f : static_cast<float>(value);
        }
        case TINYGLTF_COMPONENT_TYPE_BYTE:
        {
            int8_t value = 0;
            std::memcpy(&value, pointer, sizeof(int8_t));
            return normalized ? std::max(static_cast<float>(value) / 127.0f, -1.0f) : static_cast<float>(value);
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        {
            uint16_t value = 0;
            std::memcpy(&value, pointer, sizeof(uint16_t));
            return normalized ? static_cast<float>(value) / 65535.0f : static_cast<float>(value);
        }
        case TINYGLTF_COMPONENT_TYPE_SHORT:
        {
            int16_t value = 0;
            std::memcpy(&value, pointer, sizeof(int16_t));
            return normalized ? std::max(static_cast<float>(value) / 32767.0f, -1.0f) : static_cast<float>(value);
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        {
            uint32_t value = 0;
            std::memcpy(&value, pointer, sizeof(uint32_t));
            return static_cast<float>(value);
        }
        default:
            return 0.0f;
    }
}

accessor_values read_accessor(const tinygltf::Model& gltf, int accessor_index)
{
    accessor_values result;

    if(accessor_index < 0 || static_cast<size_t>(accessor_index) >= gltf.accessors.size())
    {
        return result;
    }

    const auto& accessor = gltf.accessors[static_cast<size_t>(accessor_index)];

    // A bufferView-less accessor is all zeroes (or sparse, which we skip).
    if(accessor.bufferView < 0 || static_cast<size_t>(accessor.bufferView) >= gltf.bufferViews.size())
    {
        return result;
    }

    const auto& view = gltf.bufferViews[static_cast<size_t>(accessor.bufferView)];
    if(view.buffer < 0 || static_cast<size_t>(view.buffer) >= gltf.buffers.size())
    {
        return result;
    }

    const auto& buffer = gltf.buffers[static_cast<size_t>(view.buffer)];

    const int components = tinygltf::GetNumComponentsInType(static_cast<uint32_t>(accessor.type));
    const int component_size = tinygltf::GetComponentSizeInBytes(static_cast<uint32_t>(accessor.componentType));

    if(components <= 0 || component_size <= 0)
    {
        return result;
    }

    // ByteStride() resolves the "tightly packed" case (a stride of 0) for us.
    const int stride = accessor.ByteStride(view);
    if(stride <= 0)
    {
        return result;
    }

    const size_t begin = view.byteOffset + accessor.byteOffset;
    const size_t needed = begin + static_cast<size_t>(stride) * (accessor.count - 1)
                        + static_cast<size_t>(components) * static_cast<size_t>(component_size);

    if(accessor.count == 0 || needed > buffer.data.size())
    {
        utils::prerror("model_gltf: accessor runs past the end of its buffer");
        return result;
    }

    result.components = components;
    result.count = accessor.count;
    result.values.resize(accessor.count * static_cast<size_t>(components));

    const unsigned char* base = buffer.data.data() + begin;

    for(size_t i = 0; i < accessor.count; ++i)
    {
        const unsigned char* element = base + static_cast<size_t>(stride) * i;

        for(int c = 0; c < components; ++c)
        {
            result.values[i * static_cast<size_t>(components) + static_cast<size_t>(c)] =
                read_component(element + static_cast<size_t>(c) * static_cast<size_t>(component_size),
                               accessor.componentType, accessor.normalized);
        }
    }

    return result;
}

std::vector<uint32_t> read_indices(const tinygltf::Model& gltf, int accessor_index, size_t vertex_count)
{
    std::vector<uint32_t> indices;

    const accessor_values values = read_accessor(gltf, accessor_index);

    if(values.empty())
    {
        // An indexless primitive draws its vertices in order.
        indices.resize(vertex_count);
        for(size_t i = 0; i < vertex_count; ++i)
        {
            indices[i] = static_cast<uint32_t>(i);
        }
        return indices;
    }

    indices.reserve(values.count);
    for(float value : values.values)
    {
        indices.push_back(static_cast<uint32_t>(value));
    }

    return indices;
}

glm::mat4 read_matrix(const std::vector<double>& values)
{
    // glTF stores matrices column-major, which is also glm's layout.
    glm::mat4 matrix(1.0f);

    if(values.size() != 16)
    {
        return matrix;
    }

    for(int column = 0; column < 4; ++column)
    {
        for(int row = 0; row < 4; ++row)
        {
            matrix[column][row] = static_cast<float>(values[static_cast<size_t>(column * 4 + row)]);
        }
    }

    return matrix;
}

animation_path parse_animation_path(const std::string& path, bool& supported)
{
    supported = true;

    if(path == "translation") return animation_path::translation;
    if(path == "rotation")    return animation_path::rotation;
    if(path == "scale")       return animation_path::scale;

    // "weights" drives morph targets, which we do not load.
    supported = false;
    return animation_path::translation;
}

interpolation_type parse_interpolation(const std::string& interpolation)
{
    if(interpolation == "STEP")        return interpolation_type::step;
    if(interpolation == "CUBICSPLINE") return interpolation_type::cubic_spline;
    return interpolation_type::linear;
}

/// Flat normals for a primitive that shipped without any.
void generate_normals(std::vector<vertex>& vertices, const std::vector<uint32_t>& indices)
{
    for(auto& v : vertices)
    {
        v.normal = glm::vec3(0.0f);
    }

    for(size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        const uint32_t ia = indices[i];
        const uint32_t ib = indices[i + 1];
        const uint32_t ic = indices[i + 2];

        if(ia >= vertices.size() || ib >= vertices.size() || ic >= vertices.size())
        {
            continue;
        }

        const glm::vec3 edge_1 = vertices[ib].position - vertices[ia].position;
        const glm::vec3 edge_2 = vertices[ic].position - vertices[ia].position;
        const glm::vec3 face_normal = glm::cross(edge_1, edge_2);

        vertices[ia].normal += face_normal;
        vertices[ib].normal += face_normal;
        vertices[ic].normal += face_normal;
    }

    for(auto& v : vertices)
    {
        if(glm::length(v.normal) > 0.0f)
        {
            v.normal = glm::normalize(v.normal);
        }
        else
        {
            v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }
}

} // namespace

model_gltf::model_gltf(const std::string& path)
{
    m_skeleton = make_ref<class skeleton>();
    m_multimesh_3d = make_ref<multimesh_3d>();

    load(path);
}

model_gltf::~model_gltf()
{
}

const std::vector<gltf_primitive>& model_gltf::primitives() const
{
    return m_primitives;
}

ref<class skeleton> model_gltf::skeleton() const
{
    return m_skeleton;
}

const std::vector<ref<animation_clip>>& model_gltf::animations() const
{
    return m_animations;
}

std::vector<std::string> model_gltf::animation_names() const
{
    std::vector<std::string> names;
    names.reserve(m_animations.size());

    for(const auto& clip : m_animations)
    {
        names.push_back(clip ? clip->name() : std::string());
    }

    return names;
}

size_t model_gltf::borrow_animations(const model_gltf& donor, bool replace)
{
    if(&donor == this || !donor.m_skeleton || !m_skeleton)
    {
        return 0;
    }

    const std::vector<int> bones = match_humanoid_bones(*donor.m_skeleton, *m_skeleton);

    if(std::none_of(bones.begin(), bones.end(), [](int b) { return b >= 0; }))
    {
        utils::prerror("model_gltf:", m_path, "and", donor.m_path,
                       "are not rigs this can match; no clips borrowed");
        return 0;
    }

    size_t borrowed = 0;

    for(const auto& clip : donor.m_animations)
    {
        if(!clip)
        {
            continue;
        }

        auto mine = std::find_if(m_animations.begin(), m_animations.end(),
                                 [&](const ref<animation_clip>& c) { return c && c->name() == clip->name(); });

        if(mine != m_animations.end() && !replace)
        {
            continue;
        }

        auto converted = retarget_clip(*clip, *donor.m_skeleton, *m_skeleton, bones);

        if(!converted)
        {
            continue;
        }

        if(mine != m_animations.end())
        {
            *mine = converted;
        }
        else
        {
            m_animations.push_back(converted);
        }

        ++borrowed;
    }

    return borrowed;
}

bool model_gltf::skinned() const
{
    for(const auto& skin : m_skeleton->skins())
    {
        if(!skin.joints.empty())
        {
            return true;
        }
    }

    return false;
}

std::string model_gltf::name() const
{
    return m_name;
}

const std::string& model_gltf::path() const
{
    return m_path;
}

ref<multimesh_instance_3d> model_gltf::create_instance()
{
    return create_gltf_instance();
}

ref<gltf_instance_3d> model_gltf::create_gltf_instance()
{
    return make_ref<gltf_instance_3d>(
        std::static_pointer_cast<model_gltf>(shared_from_this()));
}

void model_gltf::load(const std::string& path)
{
    m_path = path;

    tinygltf::Model gltf;
    tinygltf::TinyGLTF loader;
    std::string error;
    std::string warning;

    const bool binary = path.size() >= 4 && path.compare(path.size() - 4, 4, ".glb") == 0;

    // glTF images are decoded by stb, whose flip-on-load is one global
    // switch that anything else loading a picture may have left on. glTF
    // wants its rows exactly as stored, so it is said here rather than
    // trusted: a texture decoded upside down is a model whose every UV lands
    // on the wrong part of its atlas, and it only happened to models loaded
    // after some other picture had been.
    stbi_set_flip_vertically_on_load(false);

    // Read through nle::files, the file and whatever it names beside it (a
    // .gltf's buffers and pictures), so a game that ships its files changed
    // can hand them over as they were.
    tinygltf::FsCallbacks callbacks{};
    callbacks.FileExists = [](const std::string& name, void*) { return files::exists(name); };
    callbacks.ExpandFilePath = &tinygltf::ExpandFilePath;
    callbacks.WriteWholeFile = &tinygltf::WriteWholeFile;
    callbacks.ReadWholeFile = [](std::vector<unsigned char>* out, std::string* err,
                                 const std::string& name, void*) {
        const auto bytes = files::read(name);

        if(!bytes)
        {
            if(err != nullptr)
            {
                *err += "could not read " + name + "\n";
            }

            return false;
        }

        out->assign(bytes->begin(), bytes->end());
        return true;
    };

    // Left out on purpose: the size on disk is not the size of what is read.
    callbacks.GetFileSizeInBytes = nullptr;
    callbacks.user_data = nullptr;
    loader.SetFsCallbacks(callbacks);

    const auto bytes = files::read(path);

    if(!bytes)
    {
        throw std::runtime_error("model_gltf: failed to load '" + path + "': could not read it");
    }

    const std::string base_dir = std::filesystem::path(path).parent_path().string();

    const bool loaded = binary
        ? loader.LoadBinaryFromMemory(&gltf, &error, &warning,
                                      reinterpret_cast<const unsigned char*>(bytes->data()),
                                      static_cast<unsigned int>(bytes->size()), base_dir)
        : loader.LoadASCIIFromString(&gltf, &error, &warning, bytes->data(),
                                     static_cast<unsigned int>(bytes->size()), base_dir);

    if(!warning.empty())
    {
        utils::prdebug("model_gltf:", path, "warning:", warning);
    }

    if(!loaded)
    {
        throw std::runtime_error("model_gltf: failed to load '" + path + "': " +
                                 (error.empty() ? "unknown error" : error));
    }

    m_name = gltf.scenes.empty() || gltf.scenes[0].name.empty()
        ? path
        : gltf.scenes[0].name;

    // ---- textures -------------------------------------------------------
    // Decoded once per image and shared by every material that uses it.
    std::unordered_map<int, ref<class texture>> textures;

    auto texture_for = [&](int texture_index) -> ref<class texture> {
        if(texture_index < 0 || static_cast<size_t>(texture_index) >= gltf.textures.size())
        {
            return nullptr;
        }

        const int image_index = gltf.textures[static_cast<size_t>(texture_index)].source;
        if(image_index < 0 || static_cast<size_t>(image_index) >= gltf.images.size())
        {
            return nullptr;
        }

        auto cached = textures.find(image_index);
        if(cached != textures.end())
        {
            return cached->second;
        }

        const auto& image = gltf.images[static_cast<size_t>(image_index)];

        ref<class texture> result;
        if(!image.image.empty() && image.width > 0 && image.height > 0)
        {
            // Small textures are palettes: a few dozen texels across, each a
            // flat colour a whole face is painted from, and blending between
            // them paints the robe onto the face. Large ones are baked
            // atlases -- an auto-unwrapped model is hundreds of small islands
            // packed into one picture -- and sampled without mipmaps, a
            // character across a field picks one texel in a hundred from
            // whichever island happens to be nearest. That reads as a texture
            // mapped onto the wrong model.
            constexpr int LARGEST_PALETTE = 512;

            const texture_filter filtering = std::max(image.width, image.height) > LARGEST_PALETTE
                                                 ? texture_filter::smooth
                                                 : texture_filter::crisp;

            // glTF texture space has its origin at the top left, which is
            // where glTexImage2D puts the first row, so no flip is wanted.
            result = make_ref<class texture>(image.image.data(), image.width, image.height,
                                             image.component, false, filtering);
        }
        else
        {
            utils::prerror("model_gltf: image", image_index, "could not be decoded");
        }

        textures.emplace(image_index, result);
        return result;
    };

    // ---- materials ------------------------------------------------------
    auto material_for = [&](int material_index) -> std::pair<ref<class material>, ref<class texture>> {
        auto result = make_ref<class material>();

        if(material_index < 0 || static_cast<size_t>(material_index) >= gltf.materials.size())
        {
            result->set_ambient(glm::vec3(0.2f));
            result->set_diffuse(glm::vec3(0.8f));
            result->set_specular(glm::vec3(0.2f));
            result->set_shininess(16.0f);
            result->set_dissolve(1.0f);
            return { result, nullptr };
        }

        const auto& source = gltf.materials[static_cast<size_t>(material_index)];
        const auto& pbr = source.pbrMetallicRoughness;

        glm::vec4 base_color(1.0f);
        if(pbr.baseColorFactor.size() == 4)
        {
            for(int i = 0; i < 4; ++i)
            {
                base_color[i] = static_cast<float>(pbr.baseColorFactor[static_cast<size_t>(i)]);
            }
        }

        const float roughness = static_cast<float>(pbr.roughnessFactor);
        const float metallic = static_cast<float>(pbr.metallicFactor);

        // The engine shades with Phong, so the metallic/roughness inputs are
        // approximated rather than used properly: rougher means a wider, dimmer
        // highlight, and metals take their specular colour from the base colour.
        const float shininess = std::max(2.0f, (1.0f - roughness) * (1.0f - roughness) * 256.0f);
        const glm::vec3 specular_color = glm::mix(glm::vec3(1.0f), glm::vec3(base_color), metallic);

        result->set_id(source.name);
        result->set_diffuse(glm::vec3(base_color));
        result->set_ambient(glm::vec3(base_color) * 0.2f);
        result->set_specular(specular_color * (1.0f - roughness) * 0.5f);
        result->set_shininess(shininess);
        result->set_dissolve(base_color.a);

        return { result, texture_for(pbr.baseColorTexture.index) };
    };

    // ---- nodes ----------------------------------------------------------
    auto& nodes = m_skeleton->nodes();
    nodes.resize(gltf.nodes.size());

    for(size_t i = 0; i < gltf.nodes.size(); ++i)
    {
        const auto& source = gltf.nodes[i];
        auto& node = nodes[i];

        node.name = source.name;

        if(source.matrix.size() == 16)
        {
            // A node may give a baked matrix instead of TRS. Decompose it, so
            // animation channels still have something to write into.
            const glm::mat4 matrix = read_matrix(source.matrix);

            glm::vec3 skew;
            glm::vec4 perspective;
            if(!glm::decompose(matrix, node.scale, node.rotation, node.translation, skew, perspective))
            {
                utils::prdebug("model_gltf: node", i, "has a matrix that cannot be decomposed");
            }
        }
        else
        {
            if(source.translation.size() == 3)
            {
                node.translation = glm::vec3(static_cast<float>(source.translation[0]),
                                             static_cast<float>(source.translation[1]),
                                             static_cast<float>(source.translation[2]));
            }
            if(source.rotation.size() == 4)
            {
                // glTF stores quaternions xyzw; glm's constructor takes wxyz.
                node.rotation = glm::quat(static_cast<float>(source.rotation[3]),
                                          static_cast<float>(source.rotation[0]),
                                          static_cast<float>(source.rotation[1]),
                                          static_cast<float>(source.rotation[2]));
            }
            if(source.scale.size() == 3)
            {
                node.scale = glm::vec3(static_cast<float>(source.scale[0]),
                                       static_cast<float>(source.scale[1]),
                                       static_cast<float>(source.scale[2]));
            }
        }

        for(int child : source.children)
        {
            if(child >= 0 && static_cast<size_t>(child) < nodes.size())
            {
                node.children.push_back(child);
                nodes[static_cast<size_t>(child)].parent = static_cast<int>(i);
            }
        }
    }

    auto& roots = m_skeleton->roots();
    for(size_t i = 0; i < nodes.size(); ++i)
    {
        if(nodes[i].parent < 0)
        {
            roots.push_back(static_cast<int>(i));
        }
    }

    // ---- skins ----------------------------------------------------------
    auto& skins = m_skeleton->skins();
    skins.resize(gltf.skins.size());

    for(size_t i = 0; i < gltf.skins.size(); ++i)
    {
        const auto& source = gltf.skins[i];
        auto& skin = skins[i];

        skin.name = source.name;
        skin.joints = source.joints;

        if(skin.joints.size() > static_cast<size_t>(MAX_JOINTS))
        {
            utils::prerror("model_gltf: skin", source.name, "has", skin.joints.size(),
                           "joints; the shader supports", MAX_JOINTS, "- extra joints are dropped");
            skin.joints.resize(static_cast<size_t>(MAX_JOINTS));
        }

        const accessor_values inverse_bind = read_accessor(gltf, source.inverseBindMatrices);

        skin.inverse_bind_matrices.assign(skin.joints.size(), glm::mat4(1.0f));

        if(inverse_bind.components == 16)
        {
            for(size_t j = 0; j < skin.joints.size() && j < inverse_bind.count; ++j)
            {
                std::memcpy(glm::value_ptr(skin.inverse_bind_matrices[j]),
                            inverse_bind.values.data() + j * 16,
                            16 * sizeof(float));
            }
        }
    }

    // ---- meshes ---------------------------------------------------------
    // glTF meshes are referenced by nodes, so geometry is built per (node,
    // primitive) pair: that is what actually gets drawn, and it carries both
    // the node transform and the skin.
    for(size_t node_index = 0; node_index < gltf.nodes.size(); ++node_index)
    {
        const auto& source_node = gltf.nodes[node_index];

        if(source_node.mesh < 0 || static_cast<size_t>(source_node.mesh) >= gltf.meshes.size())
        {
            continue;
        }

        const auto& mesh = gltf.meshes[static_cast<size_t>(source_node.mesh)];

        for(const auto& primitive : mesh.primitives)
        {
            if(primitive.mode != TINYGLTF_MODE_TRIANGLES)
            {
                utils::prdebug("model_gltf: skipping non-triangle primitive in mesh", mesh.name);
                continue;
            }

            auto attribute_of = [&](const char* name) -> int {
                auto it = primitive.attributes.find(name);
                return it == primitive.attributes.end() ? -1 : it->second;
            };

            const accessor_values positions = read_accessor(gltf, attribute_of("POSITION"));
            if(positions.empty())
            {
                continue;
            }

            const accessor_values normals = read_accessor(gltf, attribute_of("NORMAL"));
            const accessor_values uvs = read_accessor(gltf, attribute_of("TEXCOORD_0"));
            const accessor_values colors = read_accessor(gltf, attribute_of("COLOR_0"));
            const accessor_values joints = read_accessor(gltf, attribute_of("JOINTS_0"));
            const accessor_values weights = read_accessor(gltf, attribute_of("WEIGHTS_0"));

            std::vector<vertex> vertices(positions.count);

            for(size_t i = 0; i < positions.count; ++i)
            {
                auto& v = vertices[i];

                v.position = glm::vec3(positions.at(i));
                v.normal = glm::vec3(normals.at(i));
                v.uv = glm::vec2(uvs.at(i));

                // White, not black: an untextured mesh without COLOR_0 should
                // show its material rather than disappear.
                v.color = glm::vec3(colors.at(i, glm::vec4(1.0f)));

                v.joints = joints.at(i);
                v.weights = weights.at(i);

                const float total = v.weights.x + v.weights.y + v.weights.z + v.weights.w;
                if(total > 0.0f)
                {
                    v.weights /= total;
                }
            }

            std::vector<uint32_t> indices = read_indices(gltf, primitive.indices, vertices.size());

            if(normals.empty())
            {
                generate_normals(vertices, indices);
            }

            auto [primitive_material, primitive_texture] = material_for(primitive.material);

            auto mesh_3d_instance = make_ref<mesh_3d>(vertices, indices, primitive_texture);
            mesh_3d_instance->set_material(primitive_material);

            m_multimesh_3d->meshes().push_back(mesh_3d_instance);

            gltf_primitive entry;
            entry.mesh = mesh_3d_instance;
            entry.node = static_cast<int>(node_index);
            entry.skin = source_node.skin >= 0 && static_cast<size_t>(source_node.skin) < skins.size()
                ? source_node.skin
                : -1;

            m_primitives.push_back(entry);
        }
    }

    // ---- animations -----------------------------------------------------
    for(const auto& source : gltf.animations)
    {
        auto clip = make_ref<animation_clip>(
            source.name.empty() ? "animation_" + std::to_string(m_animations.size()) : source.name);

        clip->samplers.reserve(source.samplers.size());

        for(const auto& source_sampler : source.samplers)
        {
            animation_sampler sampler;
            sampler.interpolation = parse_interpolation(source_sampler.interpolation);

            const accessor_values input = read_accessor(gltf, source_sampler.input);
            const accessor_values output = read_accessor(gltf, source_sampler.output);

            sampler.input = input.values;

            sampler.output.reserve(output.count);
            for(size_t i = 0; i < output.count; ++i)
            {
                sampler.output.push_back(output.at(i));
            }

            clip->samplers.push_back(std::move(sampler));
        }

        for(const auto& source_channel : source.channels)
        {
            bool supported = false;
            const animation_path path = parse_animation_path(source_channel.target_path, supported);

            if(!supported)
            {
                utils::prdebug("model_gltf: unsupported animation path", source_channel.target_path);
                continue;
            }

            animation_channel channel;
            channel.node = source_channel.target_node;
            channel.path = path;
            channel.sampler = source_channel.sampler;

            clip->channels.push_back(channel);
        }

        clip->recalculate_duration();
        m_animations.push_back(clip);
    }

    utils::prdebug("model_gltf: loaded", path, "-", m_primitives.size(), "primitives,",
                   skins.size(), "skins,", m_animations.size(), "animations");
}

} // namespace nle
