#include "retarget.h"

#include "animator.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <functional>

namespace nle
{

namespace
{

/// Rotation of a matrix that may also scale, as the armature roots of most
/// exported rigs do.
glm::quat rotation_of(const glm::mat4& matrix)
{
    glm::mat3 basis(matrix);

    for(int column = 0; column < 3; ++column)
    {
        const float length = glm::length(basis[column]);

        if(length > 0.0f)
        {
            basis[column] /= length;
        }
    }

    return glm::normalize(glm::quat_cast(basis));
}

/**
 * @brief A rig as it stands at bind time, and which of its nodes are bones.
 *
 * A joint's bind transform is its inverse bind matrix inverted -- that is
 * the pose the mesh was skinned in, and not necessarily the pose its nodes
 * were left in. Everything else stands where its rest transform puts it.
 */
struct bind_pose
{
    std::vector<glm::mat4> world;
    std::vector<glm::mat4> rest_world;
    std::vector<bool> joint;

    /// Nearest ancestor that is a joint, or -1.
    std::vector<int> joint_parent;
    std::vector<std::vector<int>> joint_children;
};

bind_pose pose_of(const skeleton& rig)
{
    const auto& nodes = rig.nodes();

    bind_pose pose;
    pose.world.assign(nodes.size(), glm::mat4(1.0f));
    pose.joint.assign(nodes.size(), false);
    pose.joint_parent.assign(nodes.size(), -1);
    pose.joint_children.assign(nodes.size(), {});

    std::vector<int> stack(rig.roots().begin(), rig.roots().end());

    while(!stack.empty())
    {
        const int index = stack.back();
        stack.pop_back();

        const auto& node = nodes[static_cast<size_t>(index)];
        const glm::mat4 parent = node.parent >= 0 ? pose.world[static_cast<size_t>(node.parent)]
                                                  : glm::mat4(1.0f);

        pose.world[static_cast<size_t>(index)] = parent * node.local_matrix();

        for(int child : node.children)
        {
            stack.push_back(child);
        }
    }

    pose.rest_world = pose.world;

    for(const auto& skin : rig.skins())
    {
        for(size_t j = 0; j < skin.joints.size(); ++j)
        {
            const int node = skin.joints[j];

            if(node < 0 || static_cast<size_t>(node) >= nodes.size())
            {
                continue;
            }

            pose.joint[static_cast<size_t>(node)] = true;

            if(j < skin.inverse_bind_matrices.size())
            {
                pose.world[static_cast<size_t>(node)] = glm::inverse(skin.inverse_bind_matrices[j]);
            }
        }
    }

    for(size_t i = 0; i < nodes.size(); ++i)
    {
        if(!pose.joint[i])
        {
            continue;
        }

        int up = nodes[i].parent;

        while(up >= 0 && !pose.joint[static_cast<size_t>(up)])
        {
            up = nodes[static_cast<size_t>(up)].parent;
        }

        pose.joint_parent[i] = up;

        if(up >= 0)
        {
            pose.joint_children[static_cast<size_t>(up)].push_back(static_cast<int>(i));
        }
    }

    return pose;
}

glm::vec3 position_in(const bind_pose& pose, int node)
{
    return glm::vec3(pose.world[static_cast<size_t>(node)][3]);
}

/// The parts of a person, each a chain of joints from the body outwards.
struct humanoid
{
    int hips = -1;

    std::vector<int> spine;
    std::vector<int> neck;
    std::vector<int> left_leg;
    std::vector<int> right_leg;
    std::vector<int> left_arm;
    std::vector<int> right_arm;
};

humanoid find_humanoid(const bind_pose& pose)
{
    humanoid body;

    std::function<int(int)> descendants = [&](int node) {
        int count = 0;

        for(int child : pose.joint_children[static_cast<size_t>(node)])
        {
            count += 1 + descendants(child);
        }

        return count;
    };

    // The joint everything else hangs from. Where there are several roots
    // -- a stray helper bone beside the armature -- the one with the most
    // under it is the body.
    int most = -1;

    for(size_t i = 0; i < pose.joint.size(); ++i)
    {
        if(pose.joint[i] && pose.joint_parent[i] < 0)
        {
            const int under = descendants(static_cast<int>(i));

            if(under > most)
            {
                most = under;
                body.hips = static_cast<int>(i);
            }
        }
    }

    if(body.hips < 0)
    {
        return body;
    }

    /// Out along the heaviest branch, which is the limb rather than the
    /// helper bone beside it.
    auto chain_from = [&](int start, size_t longest) {
        std::vector<int> chain;
        int at = start;

        while(at >= 0 && chain.size() < longest)
        {
            chain.push_back(at);

            const auto& children = pose.joint_children[static_cast<size_t>(at)];
            int next = -1;
            int heaviest = -1;

            for(int child : children)
            {
                const int weight = descendants(child);

                if(weight > heaviest)
                {
                    heaviest = weight;
                    next = child;
                }
            }

            at = next;
        }

        return chain;
    };

    /// How high a branch reaches on average, which is what tells a leg from
    /// a spine without trusting anybody's names.
    std::function<void(int, float&, int&)> heights = [&](int node, float& sum, int& count) {
        sum += position_in(pose, node).y;
        ++count;

        for(int child : pose.joint_children[static_cast<size_t>(node)])
        {
            heights(child, sum, count);
        }
    };

    auto reach = [&](int node) {
        float sum = 0.0f;
        int count = 0;
        heights(node, sum, count);
        return count > 0 ? sum / static_cast<float>(count) : 0.0f;
    };

    std::vector<int> branches = pose.joint_children[static_cast<size_t>(body.hips)];

    if(branches.size() < 3)
    {
        return humanoid();
    }

    // Highest first: the spine, then the two legs.
    std::sort(branches.begin(), branches.end(),
              [&](int a, int b) { return reach(a) > reach(b); });

    const int spine_root = branches[0];

    int left_leg = branches[branches.size() - 1];
    int right_leg = branches[branches.size() - 2];

    // The models face +Z, so their left is +X.
    if(position_in(pose, left_leg).x < position_in(pose, right_leg).x)
    {
        std::swap(left_leg, right_leg);
    }

    body.left_leg = chain_from(left_leg, 4);
    body.right_leg = chain_from(right_leg, 4);

    // Up the spine until it branches into the arms.
    int at = spine_root;

    while(at >= 0)
    {
        body.spine.push_back(at);

        const auto& children = pose.joint_children[static_cast<size_t>(at)];

        if(children.size() >= 3)
        {
            break;
        }

        if(children.size() == 2)
        {
            // Two arms and no neck, rather than a spine with a helper.
            const float a = position_in(pose, children[0]).x - position_in(pose, at).x;
            const float b = position_in(pose, children[1]).x - position_in(pose, at).x;

            if(a * b < 0.0f)
            {
                break;
            }
        }

        if(children.empty() || body.spine.size() > 8)
        {
            return humanoid();
        }

        int next = -1;
        int heaviest = -1;

        for(int child : children)
        {
            const int weight = descendants(child);

            if(weight > heaviest)
            {
                heaviest = weight;
                next = child;
            }
        }

        at = next;
    }

    const int chest = body.spine.back();

    std::vector<int> above = pose.joint_children[static_cast<size_t>(chest)];

    if(above.size() < 2)
    {
        return humanoid();
    }

    std::sort(above.begin(), above.end(), [&](int a, int b) {
        return position_in(pose, a).x > position_in(pose, b).x;
    });

    body.left_arm = chain_from(above.front(), 4);
    body.right_arm = chain_from(above.back(), 4);

    if(above.size() >= 3)
    {
        // Whatever is left nearest the middle is the neck.
        const float middle = position_in(pose, chest).x;
        int neck = -1;
        float nearest = 1e30f;

        for(size_t i = 1; i + 1 < above.size(); ++i)
        {
            const float off = std::abs(position_in(pose, above[i]).x - middle);

            if(off < nearest)
            {
                nearest = off;
                neck = above[i];
            }
        }

        body.neck = chain_from(neck, 2);
    }

    return body;
}

/**
 * @brief How big a body is, to measure its limbs against: from its hips to
 *        the top of its neck. Every rig has those, where not every rig has
 *        feet -- one in a robe may have been rigged without.
 */
float size_of(const bind_pose& pose, const humanoid& body)
{
    const int top = !body.neck.empty() ? body.neck.back() : !body.spine.empty() ? body.spine.back() : -1;

    if(body.hips < 0 || top < 0)
    {
        return 1.0f;
    }

    const float size = glm::length(position_in(pose, top) - position_in(pose, body.hips));
    return size > 1e-4f ? size : 1.0f;
}

/**
 * @brief Each bone of a chain's stretch of it: from its own joint to the
 *        next, measured down the limb from its root against the body's
 *        size, so two bodies' limbs can be laid side by side. Nothing says
 *        where the last bone ends; it is given half the one before.
 */
std::vector<std::pair<float, float>> spans_of(const bind_pose& pose, const std::vector<int>& chain, float size)
{
    std::vector<float> along(chain.size(), 0.0f);

    for(size_t i = 1; i < chain.size(); ++i)
    {
        along[i] = along[i - 1] + glm::length(position_in(pose, chain[i]) - position_in(pose, chain[i - 1])) / size;
    }

    std::vector<std::pair<float, float>> spans;

    for(size_t i = 0; i < chain.size(); ++i)
    {
        const float last_length = i > 0 ? (along[i] - along[i - 1]) * 0.5f : 0.1f;
        const float end = i + 1 < chain.size() ? along[i + 1] : along[i] + std::max(last_length, 0.02f);

        spans.emplace_back(along[i], end);
    }

    return spans;
}

/**
 * @brief Pairs two chains: bone for bone when they are as long as each
 *        other, and otherwise each by the stretch of the limb it covers.
 *
 * An auto-rigger that leaves a bone out of an arm -- no upper arm, so the
 * shoulder bone reaches from the collar to the elbow -- makes a chain one
 * shorter. Paired from the root, its every bone would take the motion of
 * the one above it, and the arm would bend at the wrong joints. Paired by
 * stretch, its long shoulder takes the upper arm's motion, the forearm the
 * forearm's, and only the collarbone's is left over; a leg with no knee
 * takes the thigh's, and a leg that stops at the knee keeps its knee.
 */
void pair_chains(const std::vector<int>& source, const std::vector<int>& target, std::vector<int>& bones,
                 const bind_pose& from, float from_size, const bind_pose& to, float to_size)
{
    if(source.size() == target.size())
    {
        for(size_t i = 0; i < source.size(); ++i)
        {
            bones[static_cast<size_t>(target[i])] = source[i];
        }

        return;
    }

    if(source.empty() || target.empty())
    {
        return;
    }

    const auto theirs = spans_of(from, source, from_size);
    const auto mine = spans_of(to, target, to_size);

    // In order down the limb, each source bone used once: whichever of the
    // ones still free shares most of this bone's stretch, or failing that
    // starts nearest it.
    size_t next_free = 0;

    for(size_t i = 0; i < target.size() && next_free < source.size(); ++i)
    {
        // Leaving one for each bone still to come, where there are enough.
        const size_t still_to_come = target.size() - 1 - i;
        const size_t last_usable = source.size() > still_to_come
                                       ? std::max(next_free, source.size() - 1 - still_to_come)
                                       : source.size() - 1;

        size_t best = next_free;
        float most = 0.0f;
        float nearest = 1e30f;

        for(size_t j = next_free; j <= last_usable; ++j)
        {
            const float shared = std::min(mine[i].second, theirs[j].second)
                               - std::max(mine[i].first, theirs[j].first);
            const float apart = std::abs(mine[i].first - theirs[j].first);

            if(shared > most || (most <= 0.0f && shared <= 0.0f && apart < nearest))
            {
                most = std::max(most, shared);
                nearest = apart;
                best = j;
            }
        }

        bones[static_cast<size_t>(target[i])] = source[best];
        next_free = best + 1;
    }
}

/**
 * @brief How each bone is turned to stand as its counterpart does.
 *
 * Only along the limbs. Where an auto-rigger put the vertebrae is its own
 * guess and says nothing about posture, so aligning them would lean one body
 * over to match another's guess -- but an arm hanging at thirty degrees
 * rather than twenty is posture, and is what this corrects.
 */
std::vector<glm::quat> limb_alignment(const bind_pose& from, const bind_pose& to,
                                      const humanoid& body, const std::vector<int>& bones)
{
    std::vector<glm::quat> align(to.world.size(), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    // The source's shoulders, the stubs from the chest to each arm.
    const humanoid source = find_humanoid(from);
    const auto a_shoulder = [&](int counterpart) {
        return (!source.left_arm.empty() && counterpart == source.left_arm.front())
            || (!source.right_arm.empty() && counterpart == source.right_arm.front());
    };

    for(const auto* limb : { &body.left_arm, &body.right_arm, &body.left_leg, &body.right_leg })
    {
        // Shoulders and hip sockets excepted, for the same reason as the
        // spine: which way the stub between them points is the rigger's.
        // A first arm bone that answers to an upper arm is no stub, though:
        // a rig with no upper arm reaches its shoulder from the collar to
        // the elbow, and how that hangs is posture.
        const bool arm = limb == &body.left_arm || limb == &body.right_arm;
        const size_t first = arm && !limb->empty()
                                     && (bones[static_cast<size_t>(limb->front())] < 0
                                         || a_shoulder(bones[static_cast<size_t>(limb->front())]))
                                 ? 1 : 0;

        for(size_t i = first; i < limb->size(); ++i)
        {
            const int bone = (*limb)[i];
            const int counterpart = bones[static_cast<size_t>(bone)];

            const bool has_next = i + 1 < limb->size()
                               && bones[static_cast<size_t>((*limb)[i + 1])] >= 0;

            if(counterpart < 0 || !has_next)
            {
                // The end of the limb turns with the bone before it.
                if(i > first)
                {
                    align[static_cast<size_t>(bone)] = align[static_cast<size_t>((*limb)[i - 1])];
                }

                continue;
            }

            const int next = (*limb)[i + 1];

            const glm::vec3 mine = position_in(to, next) - position_in(to, bone);
            const glm::vec3 theirs = position_in(from, bones[static_cast<size_t>(next)])
                                   - position_in(from, counterpart);

            if(glm::length(mine) > 1e-6f && glm::length(theirs) > 1e-6f)
            {
                align[static_cast<size_t>(bone)] = glm::rotation(glm::normalize(mine),
                                                                 glm::normalize(theirs));
            }
        }
    }

    return align;
}

} // namespace

std::vector<int> match_humanoid_bones(const skeleton& source, const skeleton& target)
{
    std::vector<int> bones(target.nodes().size(), -1);

    const bind_pose source_pose = pose_of(source);
    const bind_pose target_pose = pose_of(target);
    const humanoid from = find_humanoid(source_pose);
    const humanoid to = find_humanoid(target_pose);

    if(from.hips < 0 || to.hips < 0 || from.spine.empty() || to.spine.empty())
    {
        return bones;
    }

    bones[static_cast<size_t>(to.hips)] = from.hips;

    // The chest to the chest, whatever lies between it and the hips: that
    // is where both arms hang from, and an arm hanging off the wrong
    // vertebra swings from the wrong place.
    const size_t shared = std::min(from.spine.size(), to.spine.size());

    for(size_t i = 0; i + 1 < shared; ++i)
    {
        bones[static_cast<size_t>(to.spine[i])] = from.spine[i];
    }

    bones[static_cast<size_t>(to.spine.back())] = from.spine.back();

    const float source_size = size_of(source_pose, from);
    const float target_size = size_of(target_pose, to);

    for(const auto& [theirs, mine] : { std::pair(&from.neck, &to.neck),
                                       std::pair(&from.left_leg, &to.left_leg),
                                       std::pair(&from.right_leg, &to.right_leg),
                                       std::pair(&from.left_arm, &to.left_arm),
                                       std::pair(&from.right_arm, &to.right_arm) })
    {
        pair_chains(*theirs, *mine, bones, source_pose, source_size, target_pose, target_size);
    }

    return bones;
}

ref<animation_clip> retarget_clip(const animation_clip& clip,
                                  const skeleton& source, const skeleton& target,
                                  const std::vector<int>& bones)
{
    const auto& target_nodes = target.nodes();

    if(bones.size() != target_nodes.size()
       || std::none_of(bones.begin(), bones.end(), [](int b) { return b >= 0; }))
    {
        return nullptr;
    }

    const bind_pose from = pose_of(source);
    const bind_pose to = pose_of(target);
    const humanoid body = find_humanoid(to);

    const std::vector<glm::quat> align = limb_alignment(from, to, body, bones);

    // ---- how tall each stands, so the hips bob by the right amount -------
    float ratio = 1.0f;
    int source_hips = -1;

    if(body.hips >= 0 && bones[static_cast<size_t>(body.hips)] >= 0)
    {
        source_hips = bones[static_cast<size_t>(body.hips)];

        const float theirs = position_in(from, source_hips).y;
        const float mine = position_in(to, body.hips).y;

        if(std::abs(theirs) > 1e-6f)
        {
            ratio = mine / theirs;
        }
    }

    // ---- when to sample ---------------------------------------------------
    std::vector<float> times;

    for(const auto& sampler : clip.samplers)
    {
        times.insert(times.end(), sampler.input.begin(), sampler.input.end());
    }

    std::sort(times.begin(), times.end());
    times.erase(std::unique(times.begin(), times.end(),
                            [](float a, float b) { return std::abs(a - b) < 1e-4f; }),
                times.end());

    if(times.empty())
    {
        return nullptr;
    }

    // ---- which target nodes get a track ------------------------------------
    std::vector<int> tracked;

    for(size_t i = 0; i < target_nodes.size(); ++i)
    {
        if(to.joint[i])
        {
            tracked.push_back(static_cast<int>(i));
        }
    }

    auto result = make_ref<animation_clip>(clip.name());

    std::vector<size_t> rotation_sampler(target_nodes.size(), 0);

    for(int node : tracked)
    {
        rotation_sampler[static_cast<size_t>(node)] = result->samplers.size();

        animation_sampler sampler;
        sampler.input = times;
        result->samplers.push_back(std::move(sampler));

        animation_channel channel;
        channel.node = node;
        channel.path = animation_path::rotation;
        channel.sampler = static_cast<int>(rotation_sampler[static_cast<size_t>(node)]);
        result->channels.push_back(channel);
    }

    size_t hips_sampler = 0;

    if(source_hips >= 0)
    {
        hips_sampler = result->samplers.size();

        animation_sampler sampler;
        sampler.input = times;
        result->samplers.push_back(std::move(sampler));

        animation_channel channel;
        channel.node = body.hips;
        channel.path = animation_path::translation;
        channel.sampler = static_cast<int>(hips_sampler);
        result->channels.push_back(channel);
    }

    // ---- the source, played ------------------------------------------------
    // Through an animator of its own, so the source pose is worked out exactly
    // as it would be if it were being drawn.
    auto donor = make_ref<animation_clip>(clip);
    animator player(make_ref<skeleton>(source), { donor });
    player.play(static_cast<size_t>(0), false);
    player.pause();

    // Parents before children, so a parent's world rotation is ready when
    // its child asks.
    std::vector<int> order;
    {
        std::vector<int> stack(target.roots().rbegin(), target.roots().rend());

        while(!stack.empty())
        {
            const int index = stack.back();
            stack.pop_back();
            order.push_back(index);

            const auto& children = target_nodes[static_cast<size_t>(index)].children;

            for(auto it = children.rbegin(); it != children.rend(); ++it)
            {
                stack.push_back(*it);
            }
        }
    }

    std::vector<glm::quat> world(target_nodes.size());
    std::vector<glm::quat> previous(target_nodes.size(), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    for(size_t k = 0; k < times.size(); ++k)
    {
        player.seek(times[k]);

        for(int index : order)
        {
            const auto& node = target_nodes[static_cast<size_t>(index)];
            const glm::quat parent = node.parent >= 0 ? world[static_cast<size_t>(node.parent)]
                                                      : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

            const int counterpart = bones[static_cast<size_t>(index)];

            if(counterpart >= 0)
            {
                // Turned in the world by as much as the counterpart has turned
                // from its own bind pose, starting from this bone's bind pose
                // brought round to point the way the counterpart's does.
                const glm::quat turned = rotation_of(player.node_world_matrix(counterpart))
                                       * glm::inverse(rotation_of(from.world[static_cast<size_t>(counterpart)]));

                world[static_cast<size_t>(index)] = glm::normalize(
                    turned * align[static_cast<size_t>(index)]
                    * rotation_of(to.world[static_cast<size_t>(index)]));
            }
            else
            {
                world[static_cast<size_t>(index)] = glm::normalize(parent * node.rotation);
            }

            if(!to.joint[static_cast<size_t>(index)])
            {
                continue;
            }

            glm::quat local = glm::normalize(glm::inverse(parent) * world[static_cast<size_t>(index)]);

            // Kept on the same side of the sphere as the frame before, or
            // the interpolation between them takes the long way round.
            if(k > 0 && glm::dot(local, previous[static_cast<size_t>(index)]) < 0.0f)
            {
                local = -local;
            }

            previous[static_cast<size_t>(index)] = local;

            result->samplers[rotation_sampler[static_cast<size_t>(index)]].output.push_back(
                glm::vec4(local.x, local.y, local.z, local.w));
        }

        if(source_hips >= 0)
        {
            const glm::vec3 moved = glm::vec3(player.node_world_matrix(source_hips)[3])
                                  - position_in(from, source_hips);

            const glm::vec3 wanted = position_in(to, body.hips) + moved * ratio;

            const int parent = target_nodes[static_cast<size_t>(body.hips)].parent;
            const glm::mat4 parent_world = parent >= 0 ? to.rest_world[static_cast<size_t>(parent)]
                                                       : glm::mat4(1.0f);

            const glm::vec3 local = glm::vec3(glm::inverse(parent_world) * glm::vec4(wanted, 1.0f));

            result->samplers[hips_sampler].output.push_back(glm::vec4(local, 0.0f));
        }
    }

    result->recalculate_duration();
    return result;
}

glm::quat retarget_offset(const skeleton& source, const skeleton& target,
                          const std::vector<int>& bones, int node)
{
    const glm::quat none(1.0f, 0.0f, 0.0f, 0.0f);

    if(node < 0 || static_cast<size_t>(node) >= bones.size() || bones[static_cast<size_t>(node)] < 0)
    {
        return none;
    }

    const bind_pose from = pose_of(source);
    const bind_pose to = pose_of(target);
    const std::vector<glm::quat> align = limb_alignment(from, to, find_humanoid(to), bones);

    const int counterpart = bones[static_cast<size_t>(node)];

    // target_world = turned * align * target_bind, and
    // turned = source_world * inverse(source_bind), so what is left between
    // the two worlds is this -- and it is the same on every frame.
    return glm::normalize(glm::inverse(rotation_of(from.world[static_cast<size_t>(counterpart)]))
                          * align[static_cast<size_t>(node)]
                          * rotation_of(to.world[static_cast<size_t>(node)]));
}

} // namespace nle
