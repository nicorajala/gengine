#include "Engine/physics/physics.hpp"
#include "Engine/objects/object.hpp" // needed to access and write owner->position
#include <algorithm>
#include <iostream>

using namespace Physics;
using namespace NMATH;

World::World()
    : gravity(Vec3d(0.0, -9.81, 0.0)),
      restitution(0.2),
      positionalCorrection(0.8)
{
}

World::~World() {
    for (auto b : bodies_) delete b;
    bodies_.clear();
}

RigidBody* World::createBody(Object* owner, bool isStatic, double mass, double radius, const Vec3d& position, int colliderType, double height) {
    // reuse existing if present (and update properties)
    for (auto b : bodies_) {
        if (b->owner == owner && owner != nullptr) {
            b->isStatic = isStatic;
            b->mass = (mass <= 0.0) ? 1.0 : mass;
            b->radius = radius;
            b->height = height;
            b->colliderType = colliderType;
            b->enabled = owner ? owner->collisionEnabled : true;
            // ensure body position matches owner
            if (owner) b->position = owner->position;
            if (colliderType == COLLIDER_PLANE) b->planeY = position.y;
            return b;
        }
    }

    RigidBody* b = new RigidBody(owner);
    b->isStatic = isStatic;
    b->mass = (mass <= 0.0) ? 1.0 : mass;
    b->radius = radius;
    b->height = height;
    b->colliderType = colliderType;
    b->enabled = owner ? owner->collisionEnabled : true;

    // If an owner is supplied, initialize the body position from owner; otherwise use provided position
    if (owner) b->position = owner->position;
    else b->position = position;

    if (colliderType == COLLIDER_PLANE) {
        b->planeY = position.y;
    } else {
        b->planeY = 0.0;
    }

    b->vel = Vec3d(0.0);
    b->force = Vec3d(0.0);

    bodies_.push_back(b);
    return b;
}

void World::removeBody(Object* owner) {
    for (size_t i = 0; i < bodies_.size(); ++i) {
        if (bodies_[i]->owner == owner) {
            delete bodies_[i];
            bodies_.erase(bodies_.begin() + i);
            return;
        }
    }
}

static void applyImpulse(RigidBody* a, RigidBody* b, const Vec3d& normal) {
    // relative velocity along normal
    Vec3d rv = b->vel - a->vel;
    double velAlongNormal = rv.dot(normal);
    if (velAlongNormal > 0) return; // separating

    double invMassA = a->isStatic ? 0.0 : 1.0 / a->mass;
    double invMassB = b->isStatic ? 0.0 : 1.0 / b->mass;

    double e = 0.2;

    double j = -(1.0 + e) * velAlongNormal;
    double denom = invMassA + invMassB;
    if (denom <= 0.0) return;
    j /= denom;

    Vec3d impulse = normal * j;
    if (!a->isStatic) a->vel = a->vel - impulse * invMassA;
    if (!b->isStatic) b->vel = b->vel + impulse * invMassB;
}

static inline Vec3d getBodyPos(RigidBody* b) {
    return b->owner ? b->owner->position : b->position;
}

static inline void applyPositionDelta(RigidBody* b, const Vec3d& delta) {
    if (b->owner) b->owner->position = b->owner->position + delta;
    else b->position = b->position + delta;
}

void World::step(float dt) {
    if (dt <= 0.0f) return;

    // Integrate forces -> velocities (semi-implicit Euler)
    for (auto b : bodies_) {
        if (!b->enabled || b->isStatic) continue;
        // apply gravity
        b->force += gravity * b->mass;
        Vec3d accel = b->force * (1.0 / b->mass);
        b->vel = b->vel + accel * dt;
        // clear forces after integration
        b->force = Vec3d(0.0);
    }

    // Broad-phase naive N^2 checks and resolve collisions (sphere/cylinder)
    size_t n = bodies_.size();
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            RigidBody* A = bodies_[i];
            RigidBody* B = bodies_[j];
            if (!A->enabled || !B->enabled) continue;
            // If either owner has collision disabled, skip
            if (A->owner && !A->owner->collisionEnabled) continue;
            if (B->owner && !B->owner->collisionEnabled) continue;

            // Skip any pair where either collider is a PLANE: plane handled in dedicated pass below
            if (A->colliderType == COLLIDER_PLANE || B->colliderType == COLLIDER_PLANE) continue;

            Vec3d posA = getBodyPos(A);
            Vec3d posB = getBodyPos(B);
            Vec3d d = posB - posA;

            // horizontal distance on XZ plane
            Vec3d dh(d.x, 0.0, d.z);
            double horiz = dh.length();
            double rsum = A->radius + B->radius;

            // vertical intervals
            double aTop, aBottom, bTop, bBottom;
            if (A->colliderType == COLLIDER_CYLINDER) {
                aTop = posA.y + A->height * 0.5;
                aBottom = posA.y - A->height * 0.5;
            } else {
                aTop = posA.y + A->radius;
                aBottom = posA.y - A->radius;
            }
            if (B->colliderType == COLLIDER_CYLINDER) {
                bTop = posB.y + B->height * 0.5;
                bBottom = posB.y - B->height * 0.5;
            } else {
                bTop = posB.y + B->radius;
                bBottom = posB.y - B->radius;
            }

            bool vertOverlap = !(aBottom > bTop || bBottom > aTop);

            // collision test: require horiz overlap AND vertical overlap
            if (horiz < rsum && vertOverlap) {
                // horizontal penetration
                double penetration = rsum - horiz;
                Vec3d normal;
                if (horiz > 1e-6) normal = dh / horiz;
                else {
                    // fallback normal (arbitrary)
                    normal = Vec3d(0.0, 1.0, 0.0);
                }

                // Positional correction (safe): avoid dividing by tiny masses and clamp per-body movement
                double eps = 1e-9;
                double invMassA = A->isStatic ? 0.0 : 1.0 / std::max(A->mass, eps);
                double invMassB = B->isStatic ? 0.0 : 1.0 / std::max(B->mass, eps);
                double invSum = invMassA + invMassB;

                if (invSum > eps) {
                    double base = penetration * positionalCorrection;
                    double corrA = base * (invMassA / invSum);
                    double corrB = base * (invMassB / invSum);

                    double maxCorr = std::max(0.0, rsum * 0.5);
                    if (corrA > maxCorr) corrA = maxCorr;
                    if (corrB > maxCorr) corrB = maxCorr;

                    if (!A->isStatic) applyPositionDelta(A, -normal * corrA);
                    if (!B->isStatic) applyPositionDelta(B, normal * corrB);
                }

                // apply impulse using the horizontal normal (so vertical velocities are preserved)
                applyImpulse(A, B, normal);
            }
        }
    }

    // Integrate velocities -> positions
    for (auto b : bodies_) {
        if (b->isStatic) continue;
        Vec3d delta = b->vel * dt;
        applyPositionDelta(b, delta);
    }

    // Plane collisions: treat plane bodies as infinite/static planes aligned with +Y.
    for (auto plane : bodies_) {
        if (!plane || plane->colliderType != COLLIDER_PLANE) continue;
        // only consider static planes
        if (!plane->isStatic) continue;
        double planeY = plane->planeY;
        for (auto b : bodies_) {
            if (!b || b->isStatic || b == plane) continue;
            // skip non-enabled or owner-disabled
            if (!b->enabled) continue;
            if (b->owner && !b->owner->collisionEnabled) continue;

            // compute bottom of b depending on collider
            Vec3d posB = getBodyPos(b);
            double bottom = (b->colliderType == COLLIDER_CYLINDER) ? (posB.y - b->height * 0.5) : (posB.y - b->radius);
            if (bottom < planeY) {
                // push body up so bottom == planeY
                double newY = planeY + ((b->colliderType == COLLIDER_CYLINDER) ? (b->height * 0.5) : b->radius);
                if (b->owner) b->owner->position.y = newY;
                else b->position.y = newY;
                // reflect vertical velocity with restitution
                if (b->vel.y < 0.0) b->vel.y = -b->vel.y * restitution;
            }
        }
    }

    // Optional: simple ground plane fallback (y = -50) to avoid falling forever for non-plane worlds
    for (auto b : bodies_) {
        if (!b || b->isStatic || !b->enabled) continue;
        Vec3d pos = getBodyPos(b);
        double bottom = (b->colliderType == COLLIDER_CYLINDER) ? (pos.y - b->height * 0.5) : (pos.y - b->radius);
        double groundY = -50.0;
        if (bottom < groundY) {
            double newY = groundY + ((b->colliderType == COLLIDER_CYLINDER) ? (b->height * 0.5) : b->radius);
            if (b->owner) b->owner->position.y = newY;
            else b->position.y = newY;
            if (b->vel.y < 0.0) b->vel.y = -b->vel.y * restitution;
        }
    }
}

bool World::isOnGround(Object* owner, double eps) const {
    if (!owner) return false;
    for (auto b : bodies_) {
        if (b->owner == owner) return isOnGround(b, eps);
    }
    return false;
}

bool World::isOnGround(RigidBody* b, double eps) const {
    if (!b) return false;
    Vec3d pos = getBodyPos(b);
    double bottom = (b->colliderType == COLLIDER_CYLINDER) ? (pos.y - b->height * 0.5) : (pos.y - b->radius);
    double groundY = -50.0;
    if (bottom <= (groundY + eps)) return true;

    // check nearby bodies below
    for (auto other : bodies_) {
        if (other == b) continue;
        if (!other->enabled) continue;
        if (other->owner && !other->owner->collisionEnabled) continue;
        Vec3d otherPos = getBodyPos(other);

        // vertical check: other top must be below or near our bottom
        double otherTop = (other->colliderType == COLLIDER_CYLINDER) ? (otherPos.y + other->height * 0.5) : (otherPos.y + other->radius);
        if (otherTop > pos.y + eps) continue; // other is above us

        // horizontal distance
        Vec3d dh(otherPos.x - pos.x, 0.0, otherPos.z - pos.z);
        double horiz = dh.length();
        if (horiz < (b->radius + other->radius) + eps) return true;
    }
    return false;
}