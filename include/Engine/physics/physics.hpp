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

		double friction;      // [0,1] friction coefficient (default 0.5)
		double restitution;   // [0,1] bounciness (default 0.2)

		RigidBody(Object* o = nullptr)
			: owner(o), position(NMATH::Vec3d(0.0)), vel(NMATH::Vec3d(0.0)), force(NMATH::Vec3d(0.0)),
			mass(1.0), isStatic(false), radius(0.5), height(1.0), colliderType(COLLIDER_SPHERE),
			enabled(true), planeY(0.0), friction(0.5), restitution(0.2) {}
	};

	class World {
	public:
		World();
		~World();

		// Create/destroy bodies
		RigidBody* createBody(Object* owner, bool isStatic = false, double mass = 1.0, double radius = 0.5,
							  const NMATH::Vec3d& position = NMATH::Vec3d(0.0),
							  int colliderType = COLLIDER_SPHERE, double height = 1.0);
		void removeBody(Object* owner);

		// Advance physics
		void step(float dt);

		// Global physics parameters (now public, but use setters for runtime changes)
		NMATH::Vec3d gravity;
		double restitution;           // default bounce
		double positionalCorrection;  // penetration correction factor
		double friction;              // default friction coefficient
		bool enabled;                 // global physics enable/disable

		// Setters for runtime adjustment (for editor UI)
		void setGravity(const NMATH::Vec3d& g);
		void setRestitution(double r);
		void setPositionalCorrection(double c);
		void setFriction(double f);
		void setEnabled(bool e);

		// Query helpers
		bool isOnGround(Object* owner, double eps = 0.05) const;
		bool isOnGround(RigidBody* body, double eps = 0.05) const;

		// Expose bodies for debug (SceneManager uses it)
		const std::vector<RigidBody*>& bodies() const { return bodies_; }

		std::vector<RigidBody*> bodies_;
	};
}

#endif