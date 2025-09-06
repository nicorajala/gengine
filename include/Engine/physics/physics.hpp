#ifndef ENGINE_PHYSICS_HPP
#define ENGINE_PHYSICS_HPP

#include <vector>
#include "math/math.hpp"

class Object;

namespace Physics {
	enum ColliderType {
		COLLIDER_SPHERE = 0,
		COLLIDER_CYLINDER = 1,
		COLLIDER_PLANE = 2
	};

	struct RigidBody {
		Object* owner;      // owning object (may be nullptr)
		NMATH::Vec3d position; // world-space position when no owner is present (or mirror of owner->position)
		NMATH::Vec3d vel;
		NMATH::Vec3d force;
		double mass;
		bool isStatic;
		double radius;      // horizontal radius for sphere/cylinder
		double height;      // used for cylinder (total height)
		int colliderType;   // ColliderType
		bool enabled;

		// for plane collider: world-space Y (height) of the plane (plane normal assumed +Y)
		double planeY;

		RigidBody(Object* o = nullptr)
			: owner(o), position(NMATH::Vec3d(0.0)), vel(NMATH::Vec3d(0.0)), force(NMATH::Vec3d(0.0)),
			  mass(1.0), isStatic(false), radius(0.5), height(1.0), colliderType(COLLIDER_SPHERE),
			  enabled(true), planeY(0.0) {}
	};

	class World {
	public:
		World();
		~World();

		// Create/destroy bodies
		// owner may be nullptr to create a mesh-less body; position is used when owner==nullptr.
		RigidBody* createBody(Object* owner, bool isStatic = false, double mass = 1.0, double radius = 0.5,
							  const NMATH::Vec3d& position = NMATH::Vec3d(0.0),
							  int colliderType = COLLIDER_SPHERE, double height = 1.0);
		void removeBody(Object* owner);

		// Advance physics
		void step(float dt);

		// Simple global parameters
		NMATH::Vec3d gravity;
		double restitution; // bounce
		double positionalCorrection; // penetration correction factor

		// Query helpers
		// returns true if owner is considered on ground (contact with ground plane or another body within eps)
		bool isOnGround(Object* owner, double eps = 0.05) const;
		bool isOnGround(RigidBody* body, double eps = 0.05) const;

		// expose bodies for debug (SceneManager uses it)
		const std::vector<RigidBody*>& bodies() const { return bodies_; }

		std::vector<RigidBody*> bodies_;

	private:
	};
}

#endif