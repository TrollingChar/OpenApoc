#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include "game/state/city/vehicle.h"
#include "framework/configfile.h"
#include "framework/framework.h"
#include "framework/logger.h"
#include "framework/sound.h"
#include "game/state/base/base.h"
#include "game/state/city/agentmission.h"
#include "game/state/city/building.h"
#include "game/state/city/city.h"
#include "game/state/city/citycommonsamplelist.h"
#include "game/state/city/doodad.h"
#include "game/state/city/projectile.h"
#include "game/state/city/scenery.h"
#include "game/state/city/vehiclemission.h"
#include "game/state/city/vequipment.h"
#include "game/state/gameevent.h"
#include "game/state/gamestate.h"
#include "game/state/organisation.h"
#include "game/state/rules/aequipment_type.h"
#include "game/state/rules/scenery_tile_type.h"
#include "game/state/rules/vammo_type.h"
#include "game/state/rules/vehicle_type.h"
#include "game/state/rules/vequipment_type.h"
#include "game/state/tileview/collision.h"
#include "game/state/tileview/tile.h"
#include "game/state/tileview/tileobject_projectile.h"
#include "game/state/tileview/tileobject_shadow.h"
#include "game/state/tileview/tileobject_vehicle.h"
#include "library/sp.h"
#include <glm/glm.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <limits>
#include <queue>
#include <random>

namespace OpenApoc
{

namespace
{
static const float M_2xPI = 2.0f * M_PI;

VehicleType::Direction getDirectionLarge(float facing)
{
	static std::map<float, VehicleType::Direction> DirectionMap = {
	    {0.0f * (float)M_PI, VehicleType::Direction::N},
	    {0.125f * (float)M_PI, VehicleType::Direction::NNE},
	    {0.25f * (float)M_PI, VehicleType::Direction::NE},
	    {0.375f * (float)M_PI, VehicleType::Direction::NEE},
	    {0.5f * (float)M_PI, VehicleType::Direction::E},
	    {0.625f * (float)M_PI, VehicleType::Direction::SEE},
	    {0.75f * (float)M_PI, VehicleType::Direction::SE},
	    {0.875f * (float)M_PI, VehicleType::Direction::SSE},
	    {1.0f * (float)M_PI, VehicleType::Direction::S},
	    {1.125f * (float)M_PI, VehicleType::Direction::SSW},
	    {1.25f * (float)M_PI, VehicleType::Direction::SW},
	    {1.375f * (float)M_PI, VehicleType::Direction::SWW},
	    {1.5f * (float)M_PI, VehicleType::Direction::W},
	    {1.625f * (float)M_PI, VehicleType::Direction::NWW},
	    {1.75f * (float)M_PI, VehicleType::Direction::NW},
	    {1.875f * (float)M_PI, VehicleType::Direction::NNW},
	};

	float closestDiff = FLT_MAX;
	VehicleType::Direction closestDir = VehicleType::Direction::N;
	for (auto &p : DirectionMap)
	{
		float d1 = p.first - facing;
		if (d1 < 0.0f)
		{
			d1 += M_2xPI;
		}
		float d2 = facing - p.first;
		if (d2 < 0.0f)
		{
			d2 += M_2xPI;
		}
		float diff = std::min(d1, d2);
		if (diff < closestDiff)
		{
			closestDiff = diff;
			closestDir = p.second;
		}
	}
	return closestDir;
}

VehicleType::Direction getDirectionSmall(float facing)
{
	static std::map<float, VehicleType::Direction> DirectionMap = {
	    {0.0f * (float)M_PI, VehicleType::Direction::N},
	    {0.25f * (float)M_PI, VehicleType::Direction::NE},
	    {0.5f * (float)M_PI, VehicleType::Direction::E},
	    {0.75f * (float)M_PI, VehicleType::Direction::SE},
	    {1.0f * (float)M_PI, VehicleType::Direction::S},
	    {1.25f * (float)M_PI, VehicleType::Direction::SW},
	    {1.5f * (float)M_PI, VehicleType::Direction::W},
	    {1.75f * (float)M_PI, VehicleType::Direction::NW},
	};

	float closestDiff = FLT_MAX;
	VehicleType::Direction closestDir = VehicleType::Direction::N;
	for (auto &p : DirectionMap)
	{
		float d1 = p.first - facing;
		if (d1 < 0.0f)
		{
			d1 += M_2xPI;
		}
		float d2 = facing - p.first;
		if (d2 < 0.0f)
		{
			d2 += M_2xPI;
		}
		float diff = std::min(d1, d2);
		if (diff < closestDiff)
		{
			closestDiff = diff;
			closestDir = p.second;
		}
	}
	return closestDir;
}
}

const UString &Vehicle::getPrefix()
{
	static UString prefix = "VEHICLE_";
	return prefix;
}
const UString &Vehicle::getTypeName()
{
	static UString name = "Vehicle";
	return name;
}

const UString &Vehicle::getId(const GameState &state, const sp<Vehicle> ptr)
{
	static const UString emptyString = "";
	for (auto &v : state.vehicles)
	{
		if (v.second == ptr)
			return v.first;
	}
	LogError("No vehicle matching pointer %p", ptr.get());
	return emptyString;
}

class FlyingVehicleMover : public VehicleMover
{
  public:
	FlyingVehicleMover(Vehicle &v) : VehicleMover(v) {}
	// Vehicle is considered idle whenever it's at goal in its tile, even if it has missions to do
	void updateIdle(GameState &state)
	{
		// Crashed/falling/sliding aren't doing anything
		if (vehicle.crashed || vehicle.falling || vehicle.sliding)
		{
			return;
		}
		// Vehicles on take off mission don't do anything
		if (!vehicle.missions.empty())
		{
			if (vehicle.missions.front()->type == VehicleMission::MissionType::TakeOff ||
			    vehicle.missions.front()->type == VehicleMission::MissionType::Land)
			{
				return;
			}
		}
		// Vehicles below ground don't do anything
		if (vehicle.position.z < 2.0f)
		{
			return;
		}
		// Don't idle every frame
		if (vehicle.ticksAutoActionAvailable > state.gameTime.getTicks())
		{
			return;
		}
		vehicle.ticksAutoActionAvailable = state.gameTime.getTicks() + TICKS_AUTO_ACTION_DELAY;

		// Step 01: Drop carried vehicle if we ever are w/o mission
		if (vehicle.missions.empty() && vehicle.carriedVehicle)
		{
			vehicle.carriedVehicle->crashed = false;
			vehicle.carriedVehicle->startFalling(state);
			vehicle.carriedVehicle->carriedByVehicle.clear();
			vehicle.carriedVehicle.clear();
		}

		// Step 02: Try to move to preferred altitude if no mission
		if (vehicle.missions.empty() && (int)vehicle.position.z != (int)vehicle.altitude)
		{
			auto targetPos = vehicle.position;
			if (vehicle.position.z < (int)vehicle.altitude)
			{
				targetPos.z += 1.0f;
			}
			else
			{
				targetPos.z -= 1.0f;
			}
			auto tFrom = vehicle.tileObject->getOwningTile();
			auto tTo = tFrom->map.getTile(targetPos);
			if (FlyingVehicleTileHelper{vehicle.tileObject->map, vehicle}.canEnterTile(tFrom, tTo))
			{
				if (!vehicle.missions.empty())
				{
					// Will need new path after moving
					vehicle.missions.front()->currentPlannedPath.clear();
				}
				auto adjustHeightMission = VehicleMission::gotoLocation(state, vehicle, targetPos);
				adjustHeightMission->currentPlannedPath.emplace_front(targetPos);
				adjustHeightMission->currentPlannedPath.emplace_front(vehicle.position);
				vehicle.addMission(state, adjustHeightMission);
				return;
			}
		}

		// Step 03: Find projectiles to dodge
		// FIXME: Read vehicle engagement rules, instead for now chance to dodge is flat 80%
		if (randBoundsExclusive(state.rng, 0, 100) < 80)
		{
			for (auto &p : state.current_city->projectiles)
			{
				// Step 02.01: Figure out if projectile can theoretically hit the vehicle

				// FIXME: Do not dodge projectiles that are too low damage vs us?
				// Is this also in rules of engagement?
				// Do not dodge our own projectiles we just fired
				if (p->lifetime < 36 && p->firerVehicle == vehicle.shared_from_this())
				{
					continue;
				}
				// Find distance from vehicle to projectile path
				// Vehicle position relative to projectile
				auto point = vehicle.position - p->position;
				// Final projectile position before expiry (or 1 second passes)
				auto line = p->velocity * (float)std::min(TICKS_PER_SECOND, p->lifetime - p->age) /
				            (float)TICK_SCALE / p->velocityScale;
				if (glm::length(line) == 0.0f)
				{
					continue;
				}
				auto lineNorm = glm::normalize(line);
				auto pointNorm = glm::normalize(point);
				float angle = glm::angle(lineNorm, pointNorm);
				float distanceToHit = glm::length(point) * cosf(angle);
				Vec3<float> hitPoint = {0.0f, 0.0f, 0.0f};
				if (angle > M_PI_2)
				{
					// Projectile going the other way, it can only hit us now or never
					// So hit point is where projectile is now, which is 0, 0, 0
					// Which we've set above, so we do nothing
				}
				else if (distanceToHit > glm::length(line))
				{
					// Projectile won't reach us for a full right triangle to be formed
					// So hit point is where projectile will expire
					hitPoint = line;
				}
				else
				{
					// Otherwise hit point forms a right triangle with 0, 0, 0 and our point
					// Which means we need to find one of the sides in a right triangle,
					// where we know angle adjacent and hypotenuse, so it's simple
					hitPoint = lineNorm * distanceToHit;
				}
				// Find furthest distance from vehicle that can be hit
				auto size = vehicle.type->size.begin()->second;
				float maxSize = std::max(size.x, size.y) * 1.41f / 2.0f;
				if (glm::length(point - hitPoint) > maxSize)
				{
					continue;
				}

				// Step 02.02: Figure out which direction to dodge in

				// Rotate space so that we see where the hit point is
				// Calculate change-of-basis matrix
				glm::mat3 transform;
				if (pointNorm.x == 0 && pointNorm.z == 0)
				{
					if (pointNorm.y < 0) // rotate 180 degrees
						transform =
						    glm::mat3(glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
						              glm::vec3(0.0f, 0.0f, 1.0f));
					// else if lineNorm.y >= 0, leave transform as the identity matrix.
				}
				else
				{
					auto new_y = pointNorm;
					auto new_z = glm::normalize(glm::cross(new_y, Vec3<float>{0.0f, 1.0f, 0.0f}));
					auto new_x = glm::normalize(glm::cross(new_y, new_z));

					transform = glm::mat3(new_x, new_y, new_z);
				}
				// The point on which we hit our craft
				// (Craft at 0,0,0, forward facing matching positive Y axis)
				// So, +x = hit on right side, +z = hit on top etc.)
				auto hitPointRel = (hitPoint - point) * transform;
				// Rule:
				//   - if hit within 0.125f we can dodge either way on this axis
				//   - otherwise it doesn't matter, since projectile is too close to our center
				bool dodgeLeft = hitPointRel.x > -0.125f;
				bool dodgeRight = hitPointRel.x < 0.125f;
				bool dodgeDown = hitPointRel.z > -0.125f;
				bool dodgeUp = hitPointRel.z < 0.125f;

				// Step 02.03: Figure out which tile we can dodge into

				// We need to figure out what's "left" and what's "right" here
				// Rule:
				//   At least Pi/2 should be between vector we'll take and the projectile vector
				//   Otherwise we think we'll be moving too slow to dodge (basically moving
				//   backwards into projectile or forwards alongside it and still getting hit)
				auto point2d = glm::normalize(Vec2<float>{point.x, point.y});
				// Gather all allowed dodge locations according to the rules above
				std::list<Vec3<int>> possibleDodgeLocations;
				for (int x = -1; x <= 1; x++)
				{
					for (int y = -1; y <= 1; y++)
					{
						// Check if this is left/right unless going up/down strictly
						if (x != 0 || y != 0)
						{
							// Angle between vector to us and vector towards location
							auto angle = glm::angle(point2d, glm::normalize(Vec2<float>{x, y}));
							// Wether this location lies to our right side
							bool right =
							    asinf(glm::angle(point2d,
							                     glm::normalize(point2d + Vec2<float>{x, y}))) >= 0;
							// Can't dodge this way at all
							if ((right && !dodgeRight) || (!right && !dodgeLeft))
							{
								continue;
							}
							// Angle too small
							if (angle < M_PI_2 || angle > 1.5f * (float)M_PI_2)
							{
								continue;
							}
							// Dodging horizontally
							possibleDodgeLocations.emplace_back(
							    vehicle.position.x + x, vehicle.position.y + y, vehicle.position.z);
						}
						// Dodging vertically
						if (dodgeUp)
						{
							possibleDodgeLocations.emplace_back(vehicle.position.x + x,
							                                    vehicle.position.y + y,
							                                    vehicle.position.z + 1.0f);
						}
						if (dodgeDown)
						{
							possibleDodgeLocations.emplace_back(vehicle.position.x + x,
							                                    vehicle.position.y + y,
							                                    vehicle.position.z + -1.0f);
						}
					}
				}

				// Step 02.04: Pick a dodge location that is valid for our vehicle to move into
				// right now

				std::list<Vec3<int>> dodgeLocations;
				for (auto &targetPos : possibleDodgeLocations)
				{
					if (!vehicle.tileObject->map.tileIsValid(targetPos))
					{
						continue;
					}
					auto tFrom = vehicle.tileObject->getOwningTile();
					auto tTo = tFrom->map.getTile(targetPos);
					if (FlyingVehicleTileHelper{vehicle.tileObject->map, vehicle}.canEnterTile(
					        tFrom, tTo))
					{
						dodgeLocations.emplace_back(targetPos);
					}
				}
				if (!dodgeLocations.empty())
				{
					auto targetPos = listRandomiser(state.rng, dodgeLocations);

					if (!vehicle.missions.empty())
					{
						// Will need new path after moving
						vehicle.missions.front()->currentPlannedPath.clear();
					}
					auto dodgeMission = VehicleMission::gotoLocation(state, vehicle, targetPos);
					dodgeMission->currentPlannedPath.emplace_front(targetPos);
					dodgeMission->currentPlannedPath.emplace_front(vehicle.position);
					vehicle.addMission(state, dodgeMission);
					return;
				}
			}
		}
	}

	void update(GameState &state, unsigned int ticks) override
	{
		if (vehicle.falling)
		{
			updateFalling(state, ticks);
			return;
		}
		if (vehicle.sliding)
		{
			updateSliding(state, ticks);
			return;
		}
		if (vehicle.carriedByVehicle)
		{
			auto newPos = vehicle.carriedByVehicle->position;
			newPos.z = std::max(0.0f, newPos.z - 0.5f);
			vehicle.setPosition(newPos);
			vehicle.facing = vehicle.carriedByVehicle->facing;
			vehicle.updateSprite(state);
			return;
		}
		if (vehicle.crashed)
		{
			updateCrashed(state, ticks);
			return;
		}
		auto ticksToTurn = ticks;
		auto ticksToMove = ticks;

		unsigned lastTicksToTurn = 0;
		unsigned lastTicksToMove = 0;

		// Flag wether we need to update banking and direction
		bool updateSprite = false;
		// Move until we become idle or run out of ticks
		while (ticksToMove != lastTicksToMove || ticksToTurn != lastTicksToTurn)
		{
			lastTicksToMove = ticksToMove;
			lastTicksToTurn = ticksToTurn;

			// We may have left the map and need to cease to move
			auto vehicleTile = this->vehicle.tileObject;
			if (!vehicleTile)
			{
				break;
			}

			// Advance vehicle facing to goal
			if (ticksToTurn > 0 && vehicle.facing != vehicle.goalFacing)
			{
				updateSprite = true;
				if (vehicle.ticksToTurn > 0)
				{
					if (vehicle.ticksToTurn > ticksToTurn)
					{
						vehicle.ticksToTurn -= ticksToTurn;
						vehicle.facing += vehicle.angularVelocity * (float)ticksToTurn;
						ticksToTurn = 0;
						if (vehicle.facing < 0.0f)
						{
							vehicle.facing += M_2xPI;
						}
						if (vehicle.facing >= M_2xPI)
						{
							vehicle.facing -= M_2xPI;
						}
					}
					else
					{
						ticksToTurn -= vehicle.ticksToTurn;
						vehicle.facing = vehicle.goalFacing;
						vehicle.ticksToTurn = 0;
						vehicle.angularVelocity = 0.0f;
						vehicle.ticksAutoActionAvailable = 0;
					}
				}
			}

			// Advance vehicle position to goal
			if (ticksToMove > 0 && vehicle.goalPosition != vehicle.position)
			{
				updateSprite = true;
				Vec3<float> vectorToGoal = vehicle.goalPosition - vehicle.position;
				int distanceToGoal =
				    glm::length(vectorToGoal * VELOCITY_SCALE_CITY) /
				    std::max(0.00001f, glm::length(vehicle.velocity / (float)TICK_SCALE));
				// Cannot reach in one go
				if (distanceToGoal > ticksToMove)
				{
					auto newPos = vehicle.position;
					newPos += vehicle.velocity * (float)ticksToMove / VELOCITY_SCALE_CITY /
					          (float)TICK_SCALE;
					vehicle.setPosition(newPos);
					ticksToMove = 0;
				}
				else
				// Can reach in one go
				{
					vehicle.setPosition(vehicle.goalPosition);
					vehicle.velocity = {0.0f, 0.0f, 0.0f};
					ticksToMove -= distanceToGoal;
				}
			}

			// Request new goal
			if (vehicle.position == vehicle.goalPosition && vehicle.facing == vehicle.goalFacing)
			{
				// Need to pop before checking idle
				vehicle.popFinishedMissions(state);
				// Vehicle is considered idle if at goal even if there's more missions to do
				updateIdle(state);
				// Get new goal from mission
				if (!vehicle.getNewGoal(state))
				{
					if (!vehicle.tileObject)
					{
						return;
					}
					break;
				}
				float speed = vehicle.getSpeed();
				// New position goal acquired, set velocity and angles
				if (vehicle.position != vehicle.goalPosition)
				{
					Vec3<float> vectorToGoal =
					    (vehicle.goalPosition - vehicle.position) * VELOCITY_SCALE_CITY;
					vehicle.velocity = glm::normalize(vectorToGoal) * speed;
					Vec2<float> targetFacingVector = {vectorToGoal.x, vectorToGoal.y};
					// New facing as well?
					if (targetFacingVector.x != 0.0f || targetFacingVector.y != 0.0f)
					{
						targetFacingVector = glm::normalize(targetFacingVector);
						float a1 = acosf(-targetFacingVector.y);
						float a2 = asinf(targetFacingVector.x);
						vehicle.goalFacing = a2 >= 0 ? a1 : M_2xPI - a1;
					}
				}
				// If new position requires new facing or we acquired new facing only
				if (vehicle.facing != vehicle.goalFacing)
				{
					float d1 = vehicle.goalFacing - vehicle.facing;
					if (d1 < 0.0f)
					{
						d1 += M_2xPI;
					}
					float d2 = vehicle.facing - vehicle.goalFacing;
					if (d2 < 0.0f)
					{
						d2 += M_2xPI;
					}
					// FIXME: Proper turning speed
					// This value was hand-made to look proper on annihilators
					float TURNING_MULT =
					    (float)M_PI / (float)TICK_SCALE / VELOCITY_SCALE_CITY.x / 1.5f;
					if (d1 <= d2)
					{
						vehicle.angularVelocity = speed * TURNING_MULT;
						// Nudge vehicle in the other direction to make animation look even
						// (otherwise, first frame is 1/2 of other frames)
						vehicle.facing -= 0.06f * (float)M_PI;
						if (vehicle.facing < 0.0f)
						{
							vehicle.facing += M_2xPI;
						}
					}
					else
					{
						vehicle.angularVelocity = -speed * TURNING_MULT;
						// Nudge vehicle in the other direction to make animation look even
						// (otherwise, first frame is 1/2 of other frames)
						vehicle.facing += 0.06f * (float)M_PI;
						if (vehicle.facing >= M_2xPI)
						{
							vehicle.facing -= M_2xPI;
						}
					}
					// Establish ticks to turn
					// (turn further than we need, again, for animation purposes)
					float turnDist = std::min(d1, d2);
					turnDist += 0.12f * (float)M_PI;
					vehicle.ticksToTurn = floorf(std::abs(turnDist / vehicle.angularVelocity));

					// FIXME: Introduce proper turning speed
					// Here we just slow down velocity if we're moving too quickly
					if (vehicle.position != vehicle.goalPosition)
					{
						Vec3<float> vectorToGoal =
						    (vehicle.goalPosition - vehicle.position) * VELOCITY_SCALE_CITY;
						int ticksToMove =
						    floorf(glm::length(vectorToGoal) / glm::length(vehicle.velocity) *
						           (float)TICK_SCALE) -
						    5.0f;
						if (ticksToMove < vehicle.ticksToTurn)
						{
							vehicle.velocity *= (float)ticksToMove / (float)vehicle.ticksToTurn;
						}
					}
				}
			}
		}
		// Update sprite if required
		if (updateSprite)
		{
			vehicle.updateSprite(state);
		}
	}
};

class GroundVehicleMover : public VehicleMover
{
  public:
	GroundVehicleMover(Vehicle &v) : VehicleMover(v) {}
	// Vehicle is considered idle whenever it's at goal in its tile, even if it has missions to do
	void updateIdle(GameState &state)
	{
		if (vehicle.ticksAutoActionAvailable > state.gameTime.getTicks())
		{
			return;
		}
		vehicle.ticksAutoActionAvailable = state.gameTime.getTicks() + TICKS_AUTO_ACTION_DELAY;

		// Do ground vehicles even do anything when idle? Do they dodge?
	}

	void update(GameState &state, unsigned int ticks) override
	{
		if (vehicle.falling)
		{
			updateFalling(state, ticks);
			return;
		}
		if (vehicle.sliding)
		{
			updateSliding(state, ticks);
			return;
		}
		if (vehicle.carriedByVehicle)
		{
			auto newPos = vehicle.carriedByVehicle->position;
			newPos.z = std::max(0.0f, newPos.z - 0.5f);
			vehicle.setPosition(newPos);
			vehicle.facing = vehicle.carriedByVehicle->facing;
			vehicle.updateSprite(state);
			return;
		}
		if (vehicle.crashed)
		{
			updateCrashed(state, ticks);
			return;
		}

		auto ticksToMove = ticks;

		unsigned lastTicksToTurn = 0;
		unsigned lastTicksToMove = 0;

		// See that we're not in the air
		if (vehicle.tileObject && !vehicle.tileObject->getOwningTile()->presentScenery &&
		    !vehicle.city->map->getTile(vehicle.goalPosition)->presentScenery &&
		    (vehicle.goalWaypoints.empty() ||
		     !vehicle.city->map->getTile(vehicle.goalWaypoints.back())->presentScenery))
		{
			if (config().getBool("OpenApoc.NewFeature.CrashingGroundVehicles"))
			{
				vehicle.startFalling(state);
			}
			else
			{
				vehicle.die(state);
			}
			return;
		}

		// Flag wether we need to update banking and direction
		bool updateSprite = false;
		// Move until we become idle or run out of ticks
		while (ticksToMove != lastTicksToMove)
		{
			lastTicksToMove = ticksToMove;

			// We may have left the map and need to cease to move
			auto vehicleTile = this->vehicle.tileObject;
			if (!vehicleTile)
			{
				break;
			}

			// Advance vehicle position to goal
			if (ticksToMove > 0 && vehicle.goalPosition != vehicle.position)
			{
				updateSprite = true;
				Vec3<float> vectorToGoal = vehicle.goalPosition - vehicle.position;
				int distanceToGoal =
				    glm::length(vectorToGoal * VELOCITY_SCALE_CITY) /
				    std::max(0.00001f, glm::length(vehicle.velocity / (float)TICK_SCALE));
				// Cannot reach in one go
				if (distanceToGoal > ticksToMove)
				{
					auto newPos = vehicle.position;
					newPos += vehicle.velocity * (float)ticksToMove / VELOCITY_SCALE_CITY /
					          (float)TICK_SCALE;
					vehicle.setPosition(newPos);
					ticksToMove = 0;
				}
				else
				// Can reach in one go
				{
					vehicle.setPosition(vehicle.goalPosition);
					vehicle.velocity = {0.0f, 0.0f, 0.0f};
					ticksToMove -= distanceToGoal;
				}
			}

			// Request new goal
			if (vehicle.position == vehicle.goalPosition)
			{
				bool waypoint = false;
				if (!vehicle.goalWaypoints.empty())
				{
					vehicle.goalPosition = vehicle.goalWaypoints.front();
					vehicle.goalWaypoints.pop_front();
					waypoint = true;
				}
				else
				{
					// Need to pop before checking idle
					vehicle.popFinishedMissions(state);
					// Vehicle is considered idle if at goal even if there's more missions to do
					updateIdle(state);
					// Get new goal from mission
					if (!vehicle.getNewGoal(state))
					{
						if (!vehicle.tileObject)
						{
							return;
						}
						break;
					}
				}
				float speed = vehicle.getSpeed();
				// New position goal acquired, set velocity and angles
				if (vehicle.position != vehicle.goalPosition)
				{
					// Set up waypoint if got new position from mission
					if (!waypoint)
					{
						// If changing height
						if (vehicle.position.z != vehicle.goalPosition.z)
						{
							auto heightCurrent = vehicle.position.z - floorf(vehicle.position.z);
							auto heightGoal =
							    vehicle.goalPosition.z - floorf(vehicle.goalPosition.z);
							bool fromFlat = heightCurrent < 0.25f || heightCurrent > 0.75f;
							bool toFlat = heightGoal < 0.25f || heightGoal > 0.75f;
							// If we move from flat to flat then we're changing from into to onto
							// Change Z in the middle of the way
							if (fromFlat && toFlat)
							{
								vehicle.goalWaypoints.push_back(vehicle.goalPosition);
								// Add waypoint after midpoint at target z level
								Vec3<float> waypoint = {
								    vehicle.position.x * 0.45f + vehicle.goalPosition.x * 0.55f,
								    vehicle.position.y * 0.45f + vehicle.goalPosition.y * 0.55f,
								    vehicle.goalPosition.z};
								vehicle.goalWaypoints.push_front(waypoint);
								// Add waypoint before midpoint at current z level
								vehicle.goalPosition.x =
								    vehicle.position.x * 0.55f + vehicle.goalPosition.x * 0.45f;
								vehicle.goalPosition.y =
								    vehicle.position.y * 0.55f + vehicle.goalPosition.y * 0.45f;
								vehicle.goalPosition.z = vehicle.position.z;
							}
							else
							    // If we're on flat surface then first move to midpoint then start
							    // to
							    // change Z
							    if (fromFlat)
							{
								vehicle.goalWaypoints.push_back(vehicle.goalPosition);
								// Add midpoint waypoint at target z level
								vehicle.goalPosition.x =
								    (vehicle.position.x + vehicle.goalPosition.x) / 2.0f;
								vehicle.goalPosition.y =
								    (vehicle.position.y + vehicle.goalPosition.y) / 2.0f;
								vehicle.goalPosition.z = vehicle.position.z;
							}
							// Else if we end on flat surface first change Z then move flat
							else if (toFlat)
							{
								vehicle.goalWaypoints.push_back(vehicle.goalPosition);
								// Add midpoint waypoint at current z level
								vehicle.goalPosition.x =
								    (vehicle.position.x + vehicle.goalPosition.x) / 2.0f;
								vehicle.goalPosition.y =
								    (vehicle.position.y + vehicle.goalPosition.y) / 2.0f;
							}
							// If we're moving from nonflat to nonflat then we need no midpoint at
							// all
						}
					}

					Vec3<float> vectorToGoal =
					    (vehicle.goalPosition - vehicle.position) * VELOCITY_SCALE_CITY;
					vehicle.velocity = glm::normalize(vectorToGoal) * speed;
					Vec2<float> targetFacingVector = {vectorToGoal.x, vectorToGoal.y};
					// New facing as well?
					if (targetFacingVector.x != 0.0f || targetFacingVector.y != 0.0f)
					{
						targetFacingVector = glm::normalize(targetFacingVector);
						float a1 = acosf(-targetFacingVector.y);
						float a2 = asinf(targetFacingVector.x);
						vehicle.goalFacing = a2 >= 0 ? a1 : M_2xPI - a1;
					}
				}
				// If new position requires new facing or we acquired new facing only
				if (vehicle.facing != vehicle.goalFacing)
				{
					vehicle.facing = vehicle.goalFacing;
					updateSprite = true;
				}
			}
		}
		// Update sprite if required
		if (updateSprite)
		{
			vehicle.updateSprite(state);
		}
	}
};

VehicleMover::VehicleMover(Vehicle &v) : vehicle(v) {}

namespace
{
float inline reduceAbsValue(float value, float by)
{
	if (value > 0)
	{
		if (value > by)
		{
			value -= by;
		}
		else
		{
			value = 0;
		}
	}
	else
	{
		if (value < -by)
		{
			value += by;
		}
		else
		{
			value = 0;
		}
	}
	return value;
}
}

void VehicleMover::updateFalling(GameState &state, unsigned int ticks)
{
	auto fallTicksRemaining = ticks;

	auto &map = *vehicle.city->map;

	if (vehicle.angularVelocity != 0)
	{
		vehicle.facing += vehicle.angularVelocity * (float)ticks;
		if (vehicle.facing < 0.0f)
		{
			vehicle.facing += M_2xPI;
		}
		if (vehicle.facing >= M_2xPI)
		{
			vehicle.facing -= M_2xPI;
		}
	}

	while (fallTicksRemaining-- > 0)
	{
		auto newPosition = vehicle.position;

		// Random doodads 2% chance if low health
		if (vehicle.getMaxHealth() / vehicle.getHealth() >= 3 &&
		    randBoundsExclusive(state.rng, 0, 100) < 2)
		{
			UString doodadId = randBool(state.rng) ? "DOODAD_1_AUTOCANNON" : "DOODAD_2_AIRGUARD";
			auto doodadPos = vehicle.position;
			doodadPos.x += (float)randBoundsInclusive(state.rng, -3, 3) / 10.0f;
			doodadPos.y += (float)randBoundsInclusive(state.rng, -3, 3) / 10.0f;
			doodadPos.z += (float)randBoundsInclusive(state.rng, -3, 3) / 10.0f;
			vehicle.city->placeDoodad({&state, doodadId}, doodadPos);
			fw().soundBackend->playSample(state.city_common_sample_list->vehicleExplosion,
			                              vehicle.position, 0.25f);
		}

		vehicle.velocity.z -= FV_ACCELERATION;
		vehicle.velocity.x = reduceAbsValue(vehicle.velocity.x, FV_ACCELERATION / 8);
		vehicle.velocity.y = reduceAbsValue(vehicle.velocity.y, FV_ACCELERATION / 8);
		newPosition += vehicle.velocity / (float)TICK_SCALE / VELOCITY_SCALE_BATTLE;

		// If we fell downwards see if we went from a tile with into scenery
		if ((int)vehicle.position.z != (int)newPosition.z)
		{
			auto presentScenery = vehicle.tileObject->getOwningTile()->presentScenery;
			if (presentScenery &&
			    presentScenery->type->getATVMode() == SceneryTileType::WalkMode::Into)
			{
				// We went through "Into" scenery, force landing on it
				newPosition = vehicle.position;
				newPosition.z = floorf(vehicle.position.z);
			}
		}

		// Fell outside map
		if (!map.tileIsValid(newPosition))
		{
			vehicle.die(state, nullptr);
			return;
		}

		// Check tile we're in (or falling into)
		auto tile = map.getTile(newPosition);
		bool newTile = (Vec3<int>)newPosition != (Vec3<int>)vehicle.position;
		if (tile->presentScenery)
		{
			auto collisionDamage =
			    std::max((float)vehicle.type->health * FV_COLLISION_DAMAGE_MIN,
			             (float)std::min(FV_COLLISION_DAMAGE_LIMIT,
			                             (float)tile->presentScenery->type->constitution *
			                                 FV_COLLISION_DAMAGE_CONSTITUTION_MULTIPLIER));
			auto atvMode = tile->presentScenery->type->getATVMode();
			bool tryPlowThrough = tile->presentScenery->initialPosition.z != -1;
			if (tryPlowThrough)
			{
				switch (atvMode)
				{
					case SceneryTileType::WalkMode::Into:
					case SceneryTileType::WalkMode::Onto:
						if (newPosition.z >= tile->getRestingPosition(false, true).z)
						{
							tryPlowThrough = false;
						}
						break;
					case SceneryTileType::WalkMode::None:
						if (tile->presentScenery->type->height >= 12)
						{
							// Only try to plow through high Nones once
							tryPlowThrough = newTile;
						}
						else if (newPosition.z >= tile->getRestingPosition(false, true).z)
						{
							tryPlowThrough = false;
						}
						break;
				}
			}
			bool plowedThrough = false;
			if (tryPlowThrough)
			{
				// Crash chance depends on velocity and weight, scenery resists with constitution
				// Weight of Annihilator is >4500, Hawk >5300, Valkyrie >3300, Phoenix ~900
				// Provided:
				// - Velocity mult of 1.5 for high speed, divisor of 125 and flat value of 50
				// - Constitution of 20
				// Typical results are:
				// - Fast Hawk			: 113%/73% plow chance before/after reduction
				// - Fast Annihilator	: 104%/64% plow chance before/after reduction
				// - Fast Valkyrie		: 90%/50% plow chance before/after reduction
				// - Fast Phoenix		: 61%/21% plow chance before/after reduction
				float velocityMult =
				    (glm::length(vehicle.velocity) > FV_PLOW_CHANCE_HIGH_SPEED_THRESHOLD)
				        ? FV_PLOW_CHANCE_HIGH_SPEED_MULTIPLIER
				        : 1.0f;
				int plowThroughChance =
				    FV_PLOW_CHANCE_FLAT +
				    velocityMult * vehicle.getWeight() * FV_PLOW_CHANCE_WEIGHT_MULTIPLIER -
				    (float)tile->presentScenery->type->constitution *
				        FV_PLOW_CHANCE_CONSTITUTION_MULTIPLIER;
				plowedThrough = randBoundsExclusive(state.rng, 0, 100) < plowThroughChance;
				if (plowedThrough)
				{
					// Allow "into" to remain damaged, kill others outright
					tile->presentScenery->die(state, atvMode != SceneryTileType::WalkMode::Into);

					// "None" scenery damages our face if we plowed through it
					// Otherwise (Into/Onto) no damage as we will still get damage on landing
					// A 12.5% chance to evade damage
					if (atvMode == SceneryTileType::WalkMode::None &&
					    randBoundsExclusive(state.rng, 0,
					                        FV_COLLISION_DAMAGE_ONE_IN_CHANCE_TO_EVADE) > 0 &&
					    vehicle.applyDamage(state, collisionDamage, 0))
					{
						// Died
						return;
					}
				}
			}
			if (!plowedThrough && atvMode == SceneryTileType::WalkMode::None &&
			    tile->presentScenery->type->height >= 12)
			{
				// Didn't plow through
				bool movedToTheSide = false;
				if ((int)vehicle.position.x != (int)newPosition.x)
				{
					movedToTheSide = true;
					newPosition.x = vehicle.position.x;
					vehicle.velocity.x = 0.0f;
				}
				if ((int)vehicle.position.y != (int)newPosition.y)
				{
					movedToTheSide = true;
					newPosition.y = vehicle.position.y;
					vehicle.velocity.y = 0.0f;
				}
				// If we moved to the side of other tile into scenery then
				// cancel movement on this tick and try again with capped velocity
				if (movedToTheSide)
				{
					// "None" scenery gives half damage for bouncing off
					// A 12.5% chance to evade damage
					if (randBoundsExclusive(state.rng, 0,
					                        FV_COLLISION_DAMAGE_ONE_IN_CHANCE_TO_EVADE) > 0 &&
					    vehicle.applyDamage(state, collisionDamage / 2, 0))
					{
						// Died
						return;
					}
					// Cancel movement
					continue;
				}
				// If we moved only downwards then we land on the scenery, so force it
				newPosition.z = floorf(newPosition.z);
			}
		}
		vehicle.setPosition(newPosition);

		// See if we've landed
		tile = vehicle.tileObject->getOwningTile();
		auto presentScenery = tile->presentScenery;
		if (presentScenery)
		{
			auto atvMode = presentScenery->type->getATVMode();
			switch (atvMode)
			{
				case SceneryTileType::WalkMode::None:
				case SceneryTileType::WalkMode::Onto:
				case SceneryTileType::WalkMode::Into:
				{
					if (newPosition.z < tile->getRestingPosition(false, true).z)
					{
						vehicle.falling = false;
					}
					break;
				}
			}
			// Landed
			if (!vehicle.falling)
			{
				// A 12.5% chance to evade damage
				auto collisionDamage =
				    std::max((float)vehicle.type->health * FV_COLLISION_DAMAGE_MIN,
				             std::min(FV_COLLISION_DAMAGE_LIMIT,
				                      (float)presentScenery->type->constitution *
				                          FV_COLLISION_DAMAGE_CONSTITUTION_MULTIPLIER));
				if (randBoundsExclusive(state.rng, 0, FV_COLLISION_DAMAGE_ONE_IN_CHANCE_TO_EVADE) >
				        0 &&
				    vehicle.applyDamage(state, collisionDamage / 2, 0))
				{
					// Died
					return;
				}
				// Move to resting position in the tile
				Vec3<float> newGoal = (Vec3<int>)newPosition;
				newGoal.z = tile->getRestingPosition(false, true).z;
				vehicle.goalWaypoints.push_back(newGoal);
				newPosition.z = newGoal.z;
				// Translate Z velocity into XY velocity
				Vec2<float> vel2d = {vehicle.velocity.x, vehicle.velocity.y};
				if (vel2d.x != 0.0f || vel2d.y != 0.0f)
				{
					vel2d = glm::normalize(vel2d) * vehicle.velocity.z / 3.0f;
					vehicle.velocity.x -= vel2d.x;
					vehicle.velocity.y -= vel2d.y;
				}
				vehicle.velocity.z = 0;
				vehicle.setPosition(newPosition);
				vehicle.goalPosition = vehicle.position;
				vehicle.angularVelocity /= 2.0f;
				// Start sliding and eventually crash if:
				// - Flying vehicle
				// - Ground vehicle fell into an unpassable tile
				// - Ground vehicle fell into a tile which has different resting height and vehicle
				// height
				//   (which is something like a tunnel or terminus at base)
				if (!vehicle.type->isGround() || atvMode == SceneryTileType::WalkMode::None ||
				    (vehicle.type->type == VehicleType::Type::Road &&
				     presentScenery->type->tile_type != SceneryTileType::TileType::Road) ||
				    vehicle.position.z != tile->getRestingPosition(false, true).z)
				{
					vehicle.sliding = true;
					vehicle.goalWaypoints.clear();
				}
				// Fell into passable tile -> will move to resting position
				else
				{
					// Stop residual movement
					vehicle.velocity = {0.0f, 0.0f, 0.0f};
					vehicle.angularVelocity = 0.0f;
				}
				break;
			}
		}
	}

	vehicle.updateSprite(state);
}

void VehicleMover::updateCrashed(GameState &state, unsigned int ticks)
{
	// Tile underneath us is dead?
	auto presentScenery = vehicle.tileObject->getOwningTile()->presentScenery;
	if (!presentScenery)
	{
		if (vehicle.type->type == VehicleType::Type::UFO)
		{
			vehicle.die(state);
		}
		else
		{
			vehicle.crashed = false;
			if (vehicle.smokeDoodad)
			{
				vehicle.smokeDoodad->remove(state);
				vehicle.smokeDoodad.reset();
			}
			vehicle.startFalling(state);
		}
	}
}

void VehicleMover::updateSliding(GameState &state, unsigned int ticks)
{
	// Slided off?
	auto presentScenery = vehicle.tileObject->getOwningTile()->presentScenery;
	if (!presentScenery)
	{
		vehicle.sliding = false;
		vehicle.startFalling(state);
		return;
	}

	auto crashTicksRemaining = ticks;

	auto &map = *vehicle.city->map;

	if (vehicle.angularVelocity != 0)
	{
		vehicle.facing += vehicle.angularVelocity * (float)ticks;
		if (vehicle.facing < 0.0f)
		{
			vehicle.facing += M_2xPI;
		}
		if (vehicle.facing >= M_2xPI)
		{
			vehicle.facing -= M_2xPI;
		}
	}

	while (crashTicksRemaining-- > 0)
	{
		auto newPosition = vehicle.position;

		// Random doodads 2% chance if low health
		if (vehicle.getMaxHealth() / vehicle.getHealth() >= 3 &&
		    randBoundsExclusive(state.rng, 0, 100) < 2)
		{
			UString doodadId = randBool(state.rng) ? "DOODAD_1_AUTOCANNON" : "DOODAD_2_AIRGUARD";
			auto doodadPos = vehicle.position;
			doodadPos.x += (float)randBoundsInclusive(state.rng, -3, 3) / 10.0f;
			doodadPos.y += (float)randBoundsInclusive(state.rng, -3, 3) / 10.0f;
			doodadPos.z += (float)randBoundsInclusive(state.rng, -3, 3) / 10.0f;
			vehicle.city->placeDoodad({&state, doodadId}, doodadPos);
			fw().soundBackend->playSample(state.city_common_sample_list->vehicleExplosion,
			                              vehicle.position, 0.25f);
		}

		vehicle.velocity.x = reduceAbsValue(vehicle.velocity.x, FV_ACCELERATION);
		vehicle.velocity.y = reduceAbsValue(vehicle.velocity.y, FV_ACCELERATION);
		if (vehicle.velocity.x == 0.0f && vehicle.velocity.y == 0.0f)
		{
			vehicle.angularVelocity = 0.0f;
			vehicle.sliding = false;
			vehicle.crash(state, nullptr);
			vehicle.updateSprite(state);
			break;
		}

		newPosition += vehicle.velocity / (float)TICK_SCALE / VELOCITY_SCALE_BATTLE;

		// Fell outside map
		if (!map.tileIsValid(newPosition))
		{
			vehicle.die(state, nullptr);
			return;
		}

		// If moved do a different tile it must not have higher resting position than us and be
		// valid
		if ((Vec3<int>)newPosition != (Vec3<int>)vehicle.position)
		{
			// If no scenery in toTile that means we've slided off
			auto toTile = map.getTile(newPosition);
			if (!toTile->presentScenery)
			{
				vehicle.setPosition(newPosition);
				vehicle.updateSprite(state);
				vehicle.sliding = false;
				vehicle.startFalling(state);
				return;
			}
			// Expecting to have scenery in fromTile as checked above
			auto fromTile = map.getTile(newPosition);

			auto fromATVMode = fromTile->presentScenery->type->getATVMode();
			auto toATVMode = toTile->presentScenery->type->getATVMode();
			switch (fromATVMode)
			{
				case SceneryTileType::WalkMode::Into:
					// Bumped into something, stop
					if (toATVMode != SceneryTileType::WalkMode::Into)
					{
						// Stop moving and cancel this tick movement (will crash on next update)
						vehicle.velocity = {0.0f, 0.0f, 0.0f};
						continue;
					}
					break;
				case SceneryTileType::WalkMode::None:
				case SceneryTileType::WalkMode::Onto:
					// Went from high enough onto/none to into, falling
					if (toATVMode == SceneryTileType::WalkMode::Into &&
					    newPosition.z - floorf(newPosition.z) > 0.15f)
					{
						vehicle.setPosition(newPosition);
						vehicle.updateSprite(state);
						vehicle.sliding = false;
						vehicle.startFalling(state);
						return;
					}
					// Otherwise see that nothing blocks us
					auto upPos = newPosition;
					upPos.z += 1.0f;
					if (map.tileIsValid(upPos) && map.getTile(upPos)->presentScenery)
					{
						// Stop moving and cancel this tick movement (will crash on next update)
						vehicle.velocity = {0.0f, 0.0f, 0.0f};
						continue;
					}
					break;
			}
		}
		vehicle.setPosition(newPosition);
	}
	vehicle.updateSprite(state);
}

VehicleMover::~VehicleMover() = default;

Vehicle::Vehicle()
    : attackMode(AttackMode::Standard), altitude(Altitude::Standard), position(0, 0, 0),
      velocity(0, 0, 0)
{
}

Vehicle::~Vehicle() = default;

void Vehicle::leaveBuilding(GameState &state, Vec3<float> initialPosition, float initialFacing)
{
	LogInfo("Launching %s", this->name);
	if (this->tileObject)
	{
		LogError("Trying to launch already-launched vehicle");
		return;
	}
	auto bld = this->currentBuilding;
	if (bld)
	{
		bld->currentVehicles.erase({&state, shared_from_this()});
		this->currentBuilding = "";
	}
	this->position = initialPosition;
	this->goalPosition = initialPosition;
	this->facing = initialFacing;
	this->goalFacing = initialFacing;
	city->map->addObjectToMap(shared_from_this());
}

void Vehicle::enterBuilding(GameState &state, StateRef<Building> b)
{
	carriedByVehicle.clear();
	crashed = false;
	if (this->currentBuilding)
	{
		LogError("Vehicle already in a building?");
		return;
	}
	this->currentBuilding = b;
	b->currentVehicles.insert({&state, shared_from_this()});
	if (carriedVehicle)
	{
		carriedVehicle->enterBuilding(state, b);
		carriedVehicle.clear();
	}
	if (tileObject)
	{
		this->tileObject->removeFromMap();
		this->tileObject.reset();
	}
	if (shadowObject)
	{
		this->shadowObject->removeFromMap();
		this->shadowObject = nullptr;
	}
	this->position =
	    type->isGround() ? b->carEntranceLocations.front() : b->landingPadLocations.front();
	this->position += Vec3<float>{0.5f, 0.5f, 0.5f};
	this->facing = 0.0f;
	this->goalFacing = 0.0f;
	this->ticksToTurn = 0;
	this->angularVelocity = 0.0f;
}

void Vehicle::setupMover()
{
	if (type->isGround())
	{
		this->mover.reset(new GroundVehicleMover(*this));
	}
	else
	{
		this->mover.reset(new FlyingVehicleMover(*this));
	}
	animationDelay = 0;
	animationFrame = type->animation_sprites.begin();
}

void Vehicle::provideService(GameState &state, bool otherOrg)
{
	if (!currentBuilding)
	{
		LogError("Called provideService when not in building, wtf?");
		return;
	}
	bool agentPriority = type->provideFreightAgent;
	if (agentPriority)
	{
		provideServicePassengers(state, otherOrg);
		if (type->provideFreightBio || !otherOrg)
		{
			provideServiceCargo(state, true, otherOrg);
		}
		if (type->provideFreightCargo || !otherOrg)
		{
			provideServiceCargo(state, false, otherOrg);
		}
	}
	else
	{
		if (type->provideFreightBio || !otherOrg)
		{
			provideServiceCargo(state, true, otherOrg);
		}
		if (type->provideFreightCargo || !otherOrg)
		{
			provideServiceCargo(state, false, otherOrg);
		}
		if (type->provideFreightAgent || !otherOrg)
		{
			provideServicePassengers(state, otherOrg);
		}
	}
}

void Vehicle::provideServiceCargo(GameState &state, bool bio, bool otherOrg)
{
	StateRef<Building> destination = getServiceDestination(state);
	int spaceRemaining = bio ? getMaxBio() - getBio() : getMaxCargo() - getCargo();
	for (auto &c : currentBuilding->cargo)
	{
		// No space left
		if (spaceRemaining == 0)
		{
			break;
		}
		// Cargo spent
		if (c.count == 0)
		{
			continue;
		}
		// Won't ferry other orgs
		if (c.destination->owner != owner && !otherOrg)
		{
			continue;
		}
		// Won't ferry because dislikes
		if (otherOrg &&
		    (config().getBool("OpenApoc.NewFeature.FerryChecksRelationshipWhenBuying") ||
		     c.cost == 0))
		{
			if (owner->isRelatedTo(c.destination->owner) == Organisation::Relation::Hostile)
			{
				continue;
			}
			if (c.originalOwner &&
			    owner->isRelatedTo(c.originalOwner) == Organisation::Relation::Hostile)
			{
				continue;
			}
		}
		// Won't ferry different kind of cargo
		if ((c.type == Cargo::Type::Bio) != bio)
		{
			continue;
		}
		// Won't ferry if already picked destination and doesn't match
		if (destination && c.destination != destination)
		{
			continue;
		}
		// How much can we pick up
		int maxAmount = std::min(spaceRemaining / c.space * c.divisor, c.count);
		if (maxAmount == 0)
		{
			continue;
		}
		// Here's where we're going
		if (!destination)
		{
			destination = c.destination;
		}
		// Split cargo and load up
		auto newCargo = c;
		newCargo.count = maxAmount;
		c.count -= maxAmount;
		cargo.push_back(newCargo);
		spaceRemaining -= maxAmount * c.space / c.divisor;
	}
}

void Vehicle::provideServicePassengers(GameState &state, bool otherOrg)
{
	StateRef<Building> destination = getServiceDestination(state);
	int spaceRemaining = getMaxPassengers() - getPassengers();
	bool pickedUpPassenger = false;
	do
	{
		pickedUpPassenger = false;
		for (auto a : currentBuilding->currentAgents)
		{
			// No space left
			if (spaceRemaining == 0)
			{
				break;
			}
			// Agent doesn't want pickup
			if (a->missions.empty() ||
			    a->missions.front()->type != AgentMission::MissionType::AwaitPickup)
			{
				continue;
			}
			// Won't ferry other orgs
			if (a->missions.front()->targetBuilding->owner != owner && !otherOrg)
			{
				continue;
			}
			// Won't ferry because dislikes
			if (otherOrg && owner->isRelatedTo(a->owner) == Organisation::Relation::Hostile)
			{
				continue;
			}
			// Won't ferry if already picked destination and doesn't match
			if (destination && a->missions.front()->targetBuilding != destination)
			{
				continue;
			}
			// Here's where we're going
			if (!destination)
			{
				destination = a->missions.front()->targetBuilding;
			}
			// Load up
			a->enterVehicle(state, {&state, shared_from_this()});
			spaceRemaining--;
			pickedUpPassenger = true;
			break;
		}
	} while (pickedUpPassenger);
}

StateRef<Building> Vehicle::getServiceDestination(GameState &state)
{
	bool fromTactical = false;
	bool agentsArrived = false;
	bool cargoArrived = false;
	bool bioArrived = false;
	bool recoveryArrived = false;
	bool transferArrived = false;
	std::set<StateRef<Organisation>> suppliers;
	StateRef<Building> destination;

	// Step 01: Find first cargo destination and remove arrived cargo
	for (auto it = cargo.begin(); it != cargo.end();)
	{
		if (it->destination == currentBuilding)
		{
			if (!it->originalOwner)
			{
				fromTactical = true;
			}
			it->arrive(state, cargoArrived, bioArrived, recoveryArrived, transferArrived,
			           suppliers);
			it = cargo.erase(it);
		}
		else
		{
			// Only force-ferry non-loot cargoes
			if (it->count != 0 && !destination && it->originalOwner)
			{
				destination = it->destination;
			}
			it++;
		}
	}
	// Step 02: Find first agent destination and remove arrived agents
	std::list<StateRef<Agent>> agentsToRemove;
	for (auto a : currentAgents)
	{
		// Remove agents if wounded and this is their home base
		if (a->modified_stats.health < a->current_stats.health &&
		    a->homeBuilding == currentBuilding)
		{
			agentsToRemove.push_back(a);
		}
		// Skip agents that are just on this craft without a mission
		if (a->missions.empty() ||
		    a->missions.front()->type != AgentMission::MissionType::AwaitPickup)
		{
			continue;
		}
		if (a->missions.front()->targetBuilding == currentBuilding)
		{
			agentsToRemove.push_back(a);
			continue;
		}
		if (!destination)
		{
			destination = a->missions.front()->targetBuilding;
		}
	}
	for (auto &a : agentsToRemove)
	{
		a->enterBuilding(state, currentBuilding);
	}

	// Step 03: Arrival events
	// Transfer
	if (transferArrived)
	{
		fw().pushEvent(new GameBaseEvent(GameEventType::TransferArrived, currentBuilding->base,
		                                 nullptr, false));
	}
	if (bioArrived)
	{
		fw().pushEvent(new GameBaseEvent(GameEventType::TransferArrived, currentBuilding->base,
		                                 nullptr, true));
	}
	// Loot
	if (recoveryArrived)
	{
		fw().pushEvent(new GameBaseEvent(GameEventType::RecoveryArrived, currentBuilding->base));
	}
	// Purchase
	if (cargoArrived)
	{
		for (auto &o : suppliers)
		{
			fw().pushEvent(
			    new GameBaseEvent(GameEventType::CargoArrived, currentBuilding->base, o));
		}
	}
	return destination;
}

void Vehicle::die(GameState &state, StateRef<Vehicle> attacker, bool silent)
{
	health = 0;
	if (!silent)
	{
		auto doodad = city->placeDoodad(StateRef<DoodadType>{&state, "DOODAD_3_EXPLOSION"},
		                                this->tileObject->getCenter());
		fw().soundBackend->playSample(state.city_common_sample_list->vehicleExplosion, position);
	}
	auto id = getId(state, shared_from_this());
	if (carriedByVehicle)
	{
		carriedByVehicle->carriedVehicle.clear();
		carriedByVehicle.clear();
	}
	// Clear projectiles
	for (auto &p : city->projectiles)
	{
		if (p->trackedVehicle && p->trackedVehicle.id == id)
		{
			p->turnRate = 0;
			p->trackedVehicle.clear();
			p->trackedObject = nullptr;
		}
	}
	// Clear targets
	for (auto &v : state.vehicles)
	{
		for (auto &m : v.second->missions)
		{
			if (m->targetVehicle.id == id)
			{
				m->targetVehicle.clear();
			}
		}
	}
	if (tileObject)
	{
		this->tileObject->removeFromMap();
		this->tileObject.reset();
	}
	if (shadowObject)
	{
		this->shadowObject->removeFromMap();
		this->shadowObject.reset();
	}
	if (smokeDoodad)
	{
		smokeDoodad->remove(state);
		smokeDoodad.reset();
	}
	while (!currentAgents.empty())
	{
		// For some reason need to assign first before calling die()
		auto agent = *currentAgents.begin();
		// Dying will remove agent from current agents list
		agent->die(state, true);
	}

	// Adjust relationships
	if (attacker && !crashed)
	{
		adjustRelationshipOnDowned(state, attacker);
	}

	if (!silent && city == state.current_city)
	{
		fw().pushEvent(new GameSomethingDiedEvent(GameEventType::VehicleDestroyed, name,
		                                          attacker ? attacker->name : "", position));
	}
	state.vehicles.erase(id);
}

void Vehicle::crash(GameState &state, StateRef<Vehicle> attacker)
{
	// Dislike attacker
	if (attacker)
	{
		adjustRelationshipOnDowned(state, attacker);
	}
	// Drop carried vehicle
	if (carriedVehicle)
	{
		carriedVehicle->crashed = false;
		carriedVehicle->startFalling(state);
		carriedVehicle->carriedByVehicle.clear();
		carriedVehicle.clear();
	}
	// Actually crash
	crashed = true;
	health = std::min(health, (type->crash_health > 0) ? type->crash_health : type->health / 10);
	switch (type->type)
	{
		case VehicleType::Type::UFO:
			setMission(state, VehicleMission::crashLand(state, *this));
			addMission(state, VehicleMission::selfDestruct(state, *this), true);
			break;
		case VehicleType::Type::Flying:
		case VehicleType::Type::ATV:
		case VehicleType::Type::Road:
			setMission(state, VehicleMission::selfDestruct(state, *this));
			break;
	}
}

void Vehicle::startFalling(GameState &state, StateRef<Vehicle> attacker)
{
	// Dislike attacker
	if (attacker)
	{
		adjustRelationshipOnDowned(state, attacker);
	}
	// Drop carried vehicle
	if (carriedVehicle)
	{
		carriedVehicle->crashed = false;
		carriedVehicle->startFalling(state);
		carriedVehicle->carriedByVehicle.clear();
		carriedVehicle.clear();
	}
	// Actually start falling
	falling = true;
	if (angularVelocity == 0.0f)
	{
		float vel = getSpeed() * (float)M_PI / (float)TICK_SCALE / VELOCITY_SCALE_CITY.x / 1.5f;

		switch (randBoundsInclusive(state.rng, -1, 1))
		{
			case -1:
				angularVelocity = -vel;
				break;
			case 0:
				break;
			case 1:
				angularVelocity = vel;
				break;
		}
	}
}

void Vehicle::adjustRelationshipOnDowned(GameState &state, StateRef<Vehicle> attacker)
{
	// If we're hostile to attacker - lose 5 points
	if (owner->isRelatedTo(attacker->owner) == Organisation::Relation::Hostile)
	{
		owner->adjustRelationTo(state, attacker->owner, -5.0f);
	}
	// If we're not hostile to attacker - lose 30 points
	else
	{
		owner->adjustRelationTo(state, attacker->owner, -30.0f);
	}
	// Our allies lose 15 points, enemies gain 5 points
	// Otherwise 20+ relationship is +-3 points, 10+ is +-1 points
	for (auto &org : state.organisations)
	{
		if (org.first != attacker->owner.id && org.first != state.getCivilian().id)
		{
			if (org.second->isRelatedTo(owner) == Organisation::Relation::Hostile)
			{
				org.second->adjustRelationTo(state, attacker->owner, 5.0f);
			}
			else if (org.second->isRelatedTo(owner) == Organisation::Relation::Allied)
			{
				org.second->adjustRelationTo(state, attacker->owner, -15.0f);
			}
			else
			{
				auto rel = org.second->getRelationTo(owner);
				if (rel > 20.0f)
				{
					org.second->adjustRelationTo(state, attacker->owner, -3.0f);
				}
				else if (rel > 10.0f)
				{
					org.second->adjustRelationTo(state, attacker->owner, -1.0f);
				}
				else if (rel < -10.0f)
				{
					org.second->adjustRelationTo(state, attacker->owner, 1.0f);
				}
				else if (rel < -20.0f)
				{
					org.second->adjustRelationTo(state, attacker->owner, 3.0f);
				}
			}
		}
	}
}

bool Vehicle::isDead() const { return health <= 0; }

Vec3<float> Vehicle::getMuzzleLocation() const
{
	return type->isGround()
	           ? Vec3<float>(position.x, position.y, position.z + (float)type->height / 16.0f)
	           : Vec3<float>(position.x, position.y,
	                         position.z - tileObject->getVoxelOffset().z +
	                             (float)type->height / 16.0f);
}

void Vehicle::update(GameState &state, unsigned int ticks)
{
	bool turbo = ticks > TICKS_PER_SECOND;

	if (cloakTicksAccumulated < CLOAK_TICKS_REQUIRED_VEHICLE)
	{
		cloakTicksAccumulated += ticks;
	}
	if (!hasCloak())
	{
		cloakTicksAccumulated = 0;
	}
	if (teleportTicksAccumulated < TELEPORT_TICKS_REQUIRED_VEHICLE)
	{
		teleportTicksAccumulated += ticks;
	}
	if (!hasTeleporter())
	{
		teleportTicksAccumulated = 0;
	}
	if (!this->missions.empty())
	{
		this->missions.front()->update(state, *this, ticks);
	}
	popFinishedMissions(state);

	int maxShield = this->getMaxShield();
	if (maxShield)
	{
		this->shieldRecharge += ticks;
		if (this->shieldRecharge > TICKS_PER_SECOND)
		{
			this->shield += this->getShieldRechargeRate() * this->shieldRecharge / TICKS_PER_SECOND;
			this->shieldRecharge %= TICKS_PER_SECOND;
			if (this->shield > maxShield)
			{
				this->shield = maxShield;
			}
		}
	}

	// Moving
	if (this->mover)
	{
		this->mover->update(state, ticks);
	}

	// Animation and firing (not on turbo)
	auto vehicleTile = this->tileObject;
	if (vehicleTile && !turbo)
	{
		if (!this->type->animation_sprites.empty())
		{
			nextFrame(ticks);
		}

		// Vehicles that are taking off or landing don't attempt to fire
		bool attemptFire = true;
		if (!type->isGround() && !missions.empty())
		{
			if (missions.front()->type == VehicleMission::MissionType::TakeOff ||
			    missions.front()->type == VehicleMission::MissionType::Land)
			{
				attemptFire = false;
			}
		}
		if (attemptFire)
		{
			bool has_active_weapon = false;
			bool has_active_pd = false;
			Vec2<int> arc = {0, 0};
			Vec2<int> arcPD = {0, 0};
			for (auto &equipment : this->equipment)
			{
				if (equipment->type->type != EquipmentSlotType::VehicleWeapon)
					continue;
				equipment->update(ticks);
				if (!crashed && !falling && this->attackMode != Vehicle::AttackMode::Evasive &&
				    equipment->canFire())
				{
					has_active_weapon = true;
					if (arc.x < equipment->type->firing_arc_1)
					{
						// FIXME: Are vertical firing arcs actually working in vanilla? I think
						// not..
						arc = {equipment->type->firing_arc_1, 8};
					}
					has_active_pd = equipment->type->point_defence;
					if (has_active_pd && arcPD.x < equipment->type->firing_arc_1)
					{
						// FIXME: Are vertical firing arcs actually working in vanilla? I think
						// not..
						arcPD = {equipment->type->firing_arc_1, 8};
					}
				}
			}

			if (has_active_weapon)
			{
				// First fire where told to manually
				if (manualFire)
				{
					fireWeaponsManual(state, arc);
				}
				// Try firing point defense weapons
				else if (!has_active_pd || !fireWeaponsPointDefense(state, arcPD))
				{
					// Fire normal weapons
					fireWeaponsNormal(state, arc);
				}
			}
		}
	}
	manualFire = false;
}

void Vehicle::updateEachSecond(GameState &state)
{
	updateCargo(state);
	if (missions.empty() && !currentBuilding && owner != state.getPlayer())
	{
		setMission(state,
		           owner == state.getAliens() ? VehicleMission::patrol(state, *this)
		                                      : VehicleMission::gotoBuilding(state, *this));
	}
}

void Vehicle::updateCargo(GameState &state)
{
	if (crashed || falling)
	{
		return;
	}
	// Cannot order to ferry if aggressive and in city
	if (!currentBuilding && attackMode == AttackMode::Aggressive)
	{
		return;
	}
	// Already ferrying
	if (!missions.empty() && missions.back()->type == VehicleMission::MissionType::OfferService)
	{
		return;
	}
	// See if need to ferry
	bool needFerry = false;
	for (auto &c : cargo)
	{
		// Either this is non-combat loot, or this loot belongs to this building
		// Don't keep trying to ferry combat loot to other building as we're NOT going to move
		// when we check it in getServiceDestination method!
		if (c.originalOwner || c.destination == currentBuilding)
		{
			needFerry = true;
			break;
		}
	}
	if (!needFerry)
	{
		for (auto &a : currentAgents)
		{
			if (!a->missions.empty() &&
			    a->missions.front()->type == AgentMission::MissionType::AwaitPickup)
			{
				needFerry = true;
				break;
			}
		}
	}
	if (needFerry)
	{
		setMission(state, VehicleMission::offerService(state, *this));
	}
}

void Vehicle::updateSprite(GameState &state)
{
	// Set banking
	if (ticksToTurn > 0 && angularVelocity > 0.0f)
	{
		banking = VehicleType::Banking::Right;
	}
	else if (ticksToTurn > 0 && angularVelocity < 0.0f)
	{
		banking = VehicleType::Banking::Left;
	}
	else if ((velocity.x != 0.0f || velocity.y != 0.0f) && velocity.z > 0.0f)
	{
		banking = VehicleType::Banking::Ascending;
	}
	else if ((velocity.x != 0.0f || velocity.y != 0.0f) && velocity.z < 0.0f)
	{
		banking = VehicleType::Banking::Descending;
	}
	else
	{
		banking = VehicleType::Banking::Flat;
	}
	// UFOs don't care about banking and direction being correct
	// Otherwise ensure banking is valid
	if (type->type != VehicleType::Type::UFO)
	{
		if (type->directional_sprites.find(banking) == type->directional_sprites.end())
		{
			banking = VehicleType::Banking::Flat;
		}
	}

	// Set direction
	switch (banking)
	{
		case VehicleType::Banking::Right:
		case VehicleType::Banking::Left:
			direction = getDirectionLarge(facing);
			// UFOs don't care about banking and direction being correct
			// Otherwise ensure direction is valid
			if (type->type == VehicleType::Type::UFO)
			{
				break;
			}
			if (type->directional_sprites.at(banking).find(direction) !=
			    type->directional_sprites.at(banking).end())
			{
				break;
			}
		// Fall-through since this direction is not valid
		case VehicleType::Banking::Ascending:
		case VehicleType::Banking::Descending:
		case VehicleType::Banking::Flat:
			direction = getDirectionSmall(facing);
			// If still invalid we must cancel banking (can happen for grounds)
			if (type->directional_sprites.at(banking).find(direction) !=
			    type->directional_sprites.at(banking).end())
			{
				break;
			}
			banking = VehicleType::Banking::Flat;
			break;
	}

	// Set shadow direction
	shadowDirection = direction;
	if (!type->directional_shadow_sprites.empty() &&
	    type->directional_shadow_sprites.find(shadowDirection) ==
	        type->directional_shadow_sprites.end())
	{
		switch (shadowDirection)
		{
			case VehicleType::Direction::NNE:
			case VehicleType::Direction::NEE:
			case VehicleType::Direction::SEE:
			case VehicleType::Direction::SSE:
			case VehicleType::Direction::SSW:
			case VehicleType::Direction::SWW:
			case VehicleType::Direction::NWW:
			case VehicleType::Direction::NNW:
				// If direction is from large set then try small set
				shadowDirection = getDirectionSmall(facing);
				if (type->directional_shadow_sprites.find(shadowDirection) !=
				    type->directional_shadow_sprites.end())
				{
					break;
				}
			// Fall-through, we have to settle for north
			default:
				shadowDirection = VehicleType::Direction::N;
				break;
		}
	}
}

bool Vehicle::applyDamage(GameState &state, int damage, float armour)
{
	bool soundHandled = false;
	return applyDamage(state, damage, armour, soundHandled, nullptr);
}

bool Vehicle::applyDamage(GameState &state, int damage, float armour, bool &soundHandled,
                          StateRef<Vehicle> attacker)
{
	if (this->shield <= damage)
	{
		if (this->shield > 0)
		{
			damage -= this->shield;
			this->shield = 0;

			// destroy the shield modules
			for (auto it = this->equipment.begin(); it != this->equipment.end();)
			{
				if ((*it)->type->type == EquipmentSlotType::VehicleGeneral &&
				    (*it)->type->shielding > 0)
				{
					it = this->equipment.erase(it);
				}
				else
				{
					++it;
				}
			}
		}

		damage -= (int)armour;
		if (damage > 0)
		{
			bool wasBelowCrashThreshold = health <= type->crash_health;
			this->health -= damage;
			if (this->health <= 0)
			{
				die(state, attacker);
				soundHandled = true;
				return true;
			}
			else if (!wasBelowCrashThreshold && health <= type->crash_health && !crashed)
			{
				if (type->type == VehicleType::Type::UFO)
				{
					crash(state, attacker);
				}
				else
				{
					if (!falling)
					{
						startFalling(state, attacker);
					}
				}
				return false;
			}
		}
	}
	else
	{
		this->shield -= damage;
		if (config().getBool("OpenApoc.NewFeature.AlternateVehicleShieldSound"))
		{
			fw().soundBackend->playSample(state.city_common_sample_list->shieldHit, position);
			soundHandled = true;
		}
	}
	return false;
}

bool Vehicle::handleCollision(GameState &state, Collision &c, bool &soundHandled)
{
	if (!this->tileObject)
	{
		LogError("It's possible multiple projectiles hit the same tile in the same tick (?)");
		return false;
	}

	auto projectile = c.projectile.get();
	if (projectile)
	{
		auto vehicleDir = glm::round(type->directionToVector(direction));
		auto projectileDir = glm::normalize(projectile->getVelocity());
		auto dir = vehicleDir + projectileDir;
		dir = glm::round(dir);

		auto armourDirection = VehicleType::ArmourDirection::Right;
		if (dir.x == 0 && dir.y == 0 && dir.z == 0)
		{
			armourDirection = VehicleType::ArmourDirection::Front;
		}
		else if (dir * 0.5f == vehicleDir)
		{
			armourDirection = VehicleType::ArmourDirection::Rear;
		}
		// FIXME: vehicle Z != 0
		else if (dir.z < 0)
		{
			armourDirection = VehicleType::ArmourDirection::Top;
		}
		else if (dir.z > 0)
		{
			armourDirection = VehicleType::ArmourDirection::Bottom;
		}
		else if ((vehicleDir.x == 0 && dir.x != dir.y) || (vehicleDir.y == 0 && dir.x == dir.y))
		{
			armourDirection = VehicleType::ArmourDirection::Left;
		}

		float armourValue = 0.0f;
		auto armour = this->type->armour.find(armourDirection);
		if (armour != this->type->armour.end())
		{
			armourValue = armour->second;
		}

		return applyDamage(state, randDamage050150(state.rng, projectile->damage), armourValue,
		                   soundHandled, projectile->firerVehicle);
	}
	return false;
}

sp<TileObjectVehicle> Vehicle::findClosestEnemy(GameState &state, sp<TileObjectVehicle> vehicleTile,
                                                Vec2<int> arc)
{
	// Find the closest enemy within the firing arc
	float closestEnemyRange = std::numeric_limits<float>::max();
	sp<TileObjectVehicle> closestEnemy;
	for (auto &pair : state.vehicles)
	{
		auto otherVehicle = pair.second;
		if (otherVehicle.get() == this)
		{
			/* Can't fire at yourself */
			continue;
		}
		if (otherVehicle->crashed || otherVehicle->falling || otherVehicle->sliding)
		{
			// Can't fire at crashed vehicles
			continue;
		}
		if (otherVehicle->city != this->city)
		{
			/* Can't fire on things a world away */
			continue;
		}
		if (this->owner->isRelatedTo(otherVehicle->owner) != Organisation::Relation::Hostile)
		{
			/* Not hostile, skip */
			continue;
		}
		auto otherVehicleTile = otherVehicle->tileObject;
		if (!otherVehicleTile)
		{
			/* Not in the map, ignore */
			continue;
		}
		// Check firing arc
		if (arc.x < 8 || arc.y < 8)
		{
			auto facing = type->directionToVector(direction);
			auto vecToTarget = otherVehicleTile->getPosition() - position;
			float angleXY = glm::angle(glm::normalize(Vec2<float>{facing.x, facing.y}),
			                           glm::normalize(Vec2<float>{vecToTarget.x, vecToTarget.y}));
			float vecToTargetXY =
			    sqrtf(vecToTarget.x * vecToTarget.x + vecToTarget.y * vecToTarget.y);
			float angleZ = glm::angle(Vec2<float>{1.0f, 0.0f},
			                          glm::normalize(Vec2<float>{vecToTargetXY, vecToTarget.z}));
			if (angleXY > (float)arc.x * (float)M_PI / 8.0f ||
			    angleZ > (float)arc.y * (float)M_PI / 8.0f)
			{
				continue;
			}
		}
		// Finally add closest
		float distance = vehicleTile->getDistanceTo(otherVehicleTile);
		if (distance < closestEnemyRange)
		{
			closestEnemyRange = distance;
			closestEnemy = otherVehicleTile;
		}
	}
	return closestEnemy;
}

sp<TileObjectProjectile> Vehicle::findClosestHostileMissile(GameState &state,
                                                            sp<TileObjectVehicle> vehicleTile,
                                                            Vec2<int> arc)
{
	// Find the closest missile within the firing arc
	float closestEnemyRange = std::numeric_limits<float>::max();
	sp<TileObjectProjectile> closestEnemy;
	for (auto &projectile : state.current_city->projectiles)
	{
		// Can't shoot down projectiles w/o voxelMap
		if (!projectile->voxelMap)
		{
			continue;
		}
#ifndef DEBUG_ALLOW_PROJECTILE_ON_PROJECTILE_FRIENDLY_FIRE
		// Can't fire at friendly projectiles
		if (projectile->firerVehicle->owner == owner ||
		    owner->isRelatedTo(projectile->firerVehicle->owner) != Organisation::Relation::Hostile)
		{
			continue;
		}
#endif // ! DEBUG_ALLOW_PROJECTILE_ON_PROJECTILE_FRIENDLY_FIRE
		// Check firing arc
		if (arc.x < 8 || arc.y < 8)
		{
			auto facing = type->directionToVector(direction);
			auto vecToTarget = projectile->getPosition() - position;
			float angleXY = glm::angle(glm::normalize(Vec2<float>{facing.x, facing.y}),
			                           glm::normalize(Vec2<float>{vecToTarget.x, vecToTarget.y}));
			float vecToTargetXY =
			    sqrtf(vecToTarget.x * vecToTarget.x + vecToTarget.y * vecToTarget.y);
			float angleZ = glm::angle(Vec2<float>{1.0f, 0.0f},
			                          glm::normalize(Vec2<float>{vecToTargetXY, vecToTarget.z}));
			if (angleXY > (float)arc.x * (float)M_PI / 8.0f ||
			    angleZ > (float)arc.y * (float)M_PI / 8.0f)
			{
				continue;
			}
		}
		// Finally add closest
		float distance = vehicleTile->getDistanceTo(projectile->position);
		if (distance < closestEnemyRange)
		{
			closestEnemyRange = distance;
			closestEnemy = projectile->tileObject;
		}
	}
	return closestEnemy;
}

bool Vehicle::fireWeaponsPointDefense(GameState &state, Vec2<int> arc)
{
	// Find something to shoot at!
	sp<TileObjectProjectile> missile = findClosestHostileMissile(state, tileObject, arc);
	if (missile)
	{
		return attackTarget(state, missile);
	}
	return false;
}

void Vehicle::fireWeaponsNormal(GameState &state, Vec2<int> arc)
{
	auto firingRange = getFiringRange();
	if (!missions.empty() && missions.front()->type == VehicleMission::MissionType::AttackBuilding)
	{
		auto target = missions.front()->targetBuilding;
		if (!target->buildingParts.empty())
		{
			bool inRange = target->bounds.within(Vec2<int>{position.x, position.y});
			if (!inRange)
			{
				auto targetVector =
				    Vec3<float>{std::min(std::abs(position.x - target->bounds.p0.x),
				                         std::abs(position.x - target->bounds.p1.x)),
				                std::min(std::abs(position.y - target->bounds.p0.y),
				                         std::abs(position.x - target->bounds.p1.y)),
				                0};
				inRange = tileObject->getDistanceTo(position + targetVector) <= firingRange;
			}
			// Look for a tile in front of us so that we can hit it easilly
			auto forwardPos = position;
			if (velocity.x != 0 || velocity.y != 0)
			{
				forwardPos += glm::normalize(Vec3<float>{velocity.x, velocity.y, 0.0f}) * 4.0f;
			}
			if (inRange)
			{
				float closestDistance = FLT_MAX;
				Vec3<float> closestPart;
				for (auto &p : target->buildingParts)
				{
					auto distance = glm::length((Vec3<float>)p - forwardPos);
					if (distance < closestDistance)
					{
						closestDistance = distance;
						closestPart = p;
					}
				}
				closestPart += Vec3<float>{0.5f, 0.5f, 0.5f};
				// Expecting to have a part ready
				if (tileObject->getDistanceTo(closestPart) <= firingRange)
				{
					attackTarget(state, closestPart);
					return;
				}
			}
		}
	}

	// Find something to shoot at!
	sp<TileObjectVehicle> enemy;
	if (!missions.empty() && missions.front()->type == VehicleMission::MissionType::AttackVehicle &&
	    tileObject->getDistanceTo(missions.front()->targetVehicle->tileObject) <= firingRange)
	{
		enemy = missions.front()->targetVehicle->tileObject;
	}
	else
	{
		if (type->aggressiveness > 0)
		{
			enemy = findClosestEnemy(state, tileObject, arc);
		}
	}

	if (enemy)
	{
		attackTarget(state, enemy);
	}
}

void Vehicle::fireWeaponsManual(GameState &state, Vec2<int> arc)
{
	attackTarget(state, manualFirePosition);
}

void Vehicle::attackTarget(GameState &state, sp<TileObjectVehicle> enemyTile)
{
	static const std::set<TileObject::Type> scenerySet = {TileObject::Type::Scenery,
	                                                      TileObject::Type::Vehicle};

	auto firePosition = getMuzzleLocation();
	auto target = enemyTile->getVoxelCentrePosition();
	auto distanceTiles = glm::length(position - target);

	auto distanceVoxels = this->tileObject->getDistanceTo(enemyTile);

	for (auto &eq : this->equipment)
	{
		// Not a weapon
		if (eq->type->type != EquipmentSlotType::VehicleWeapon)
		{
			continue;
		}
		// Out of ammo or on cooldown
		if (eq->canFire() == false)
		{
			continue;
		}
		// Out of range
		if (distanceVoxels > eq->getRange())
		{
			continue;
		}
		// Check firing arc
		if (eq->type->firing_arc_1 < 8 || eq->type->firing_arc_2 < 8)
		{
			Vec2<int> arc = {eq->type->firing_arc_1, 8};
			auto facing = type->directionToVector(direction);
			auto vecToTarget = enemyTile->getPosition() - position;
			float angleXY = glm::angle(glm::normalize(Vec2<float>{facing.x, facing.y}),
			                           glm::normalize(Vec2<float>{vecToTarget.x, vecToTarget.y}));
			float vecToTargetXY =
			    sqrtf(vecToTarget.x * vecToTarget.x + vecToTarget.y * vecToTarget.y);
			float angleZ = glm::angle(Vec2<float>{1.0f, 0.0f},
			                          glm::normalize(Vec2<float>{vecToTargetXY, vecToTarget.z}));
			if (angleXY > (float)arc.x * (float)M_PI / 8.0f ||
			    angleZ > (float)arc.y * (float)M_PI / 8.0f)
			{
				continue;
			}
		}

		// Lead the target
		auto targetPosAdjusted = target;
		auto projectileVelocity = eq->type->speed * PROJECTILE_VELOCITY_MULTIPLIER;
		auto targetVelocity = enemyTile->getVehicle()->velocity;
		float timeToImpact = distanceVoxels * (float)TICK_SCALE / projectileVelocity;
		targetPosAdjusted +=
		    Collision::getLeadingOffset(target - firePosition, projectileVelocity * timeToImpact,
		                                targetVelocity * timeToImpact);

		// Check if have sight to target
		// Two attempts, at second attempt try to fire at target itself
		bool hitSomethingBad = false;
		for (int i = 0; i < 2; i++)
		{
			hitSomethingBad = false;
			auto hitObject = tileObject->map.findCollision(firePosition, targetPosAdjusted,
			                                               scenerySet, tileObject);
			if (hitObject)
			{
				if (hitObject.obj->getType() == TileObject::Type::Vehicle)
				{
					auto vehicle = std::static_pointer_cast<TileObjectVehicle>(hitObject.obj);
					if (owner->isRelatedTo(vehicle->getVehicle()->owner) !=
					    Organisation::Relation::Hostile)
					{
						LogWarning("Hit vehicle");
						hitSomethingBad = true;
					}
				}
				else if (hitObject.obj->getType() == TileObject::Type::Scenery)
				{
					hitSomethingBad = true;
				}
			}
			if (hitSomethingBad)
			{
				// Can't fire at where it will be so at least fire at where it's now
				targetPosAdjusted = target;
			}
			else
			{
				break;
			}
		}
		if (hitSomethingBad)
		{
			continue;
		}

		// Cancel cloak
		cloakTicksAccumulated = 0;

		// Let enemy dodge us
		auto enemyVehicle = enemyTile->getVehicle();
		enemyVehicle->ticksAutoActionAvailable = 0;

		// Fire
		eq->fire(state, targetPosAdjusted, {&state, enemyVehicle});
	}
	return;
}

bool Vehicle::attackTarget(GameState &state, sp<TileObjectProjectile> projectileTile)
{
	static const std::set<TileObject::Type> scenerySet = {TileObject::Type::Scenery,
	                                                      TileObject::Type::Vehicle};

	auto firePosition = getMuzzleLocation();
	auto target = projectileTile->getVoxelCentrePosition();
	auto distanceTiles = glm::length(position - target);

	auto distanceVoxels = this->tileObject->getDistanceTo(projectileTile->getPosition());

	for (auto &eq : this->equipment)
	{
		// Not a weapon
		if (eq->type->type != EquipmentSlotType::VehicleWeapon)
		{
			continue;
		}
		// Not a PD weapon
		if (!eq->type->point_defence)
		{
			continue;
		}
		// Out of ammo or on cooldown
		if (eq->canFire() == false)
		{
			continue;
		}
		// Out of range
		if (distanceVoxels > eq->getRange())
		{
			continue;
		}
		// Check firing arc
		if (eq->type->firing_arc_1 < 8 || eq->type->firing_arc_2 < 8)
		{
			Vec2<int> arc = {eq->type->firing_arc_1, 8};
			auto facing = type->directionToVector(direction);
			auto vecToTarget = projectileTile->getPosition() - position;
			float angleXY = glm::angle(glm::normalize(Vec2<float>{facing.x, facing.y}),
			                           glm::normalize(Vec2<float>{vecToTarget.x, vecToTarget.y}));
			float vecToTargetXY =
			    sqrtf(vecToTarget.x * vecToTarget.x + vecToTarget.y * vecToTarget.y);
			float angleZ = glm::angle(Vec2<float>{1.0f, 0.0f},
			                          glm::normalize(Vec2<float>{vecToTargetXY, vecToTarget.z}));
			if (angleXY > (float)arc.x * (float)M_PI / 8.0f ||
			    angleZ > (float)arc.y * (float)M_PI / 8.0f)
			{
				continue;
			}
		}

		// Lead the target
		auto targetPosAdjusted = target;
		auto projectileVelocity = eq->type->speed * PROJECTILE_VELOCITY_MULTIPLIER;
		auto targetVelocity = projectileTile->getProjectile()->velocity;
		float timeToImpact = distanceVoxels * (float)TICK_SCALE / projectileVelocity;
		targetPosAdjusted +=
		    Collision::getLeadingOffset(target - firePosition, projectileVelocity * timeToImpact,
		                                targetVelocity * timeToImpact);

		// Check if have sight to target
		// Two attempts, at second attempt try to fire at target itself
		bool hitSomethingBad = false;
		for (int i = 0; i < 2; i++)
		{
			hitSomethingBad = false;
			auto hitObject = tileObject->map.findCollision(firePosition, targetPosAdjusted,
			                                               scenerySet, tileObject);
			if (hitObject)
			{
				if (hitObject.obj->getType() == TileObject::Type::Vehicle)
				{
					auto vehicle = std::static_pointer_cast<TileObjectVehicle>(hitObject.obj);
					if (owner->isRelatedTo(vehicle->getVehicle()->owner) !=
					    Organisation::Relation::Hostile)
					{
						hitSomethingBad = true;
					}
				}
				else if (hitObject.obj->getType() == TileObject::Type::Scenery)
				{
					hitSomethingBad = true;
				}
			}
			if (hitSomethingBad)
			{
				// Can't fire at where it will be so at least fire at where it's now
				targetPosAdjusted = target;
			}
			else
			{
				break;
			}
		}
		if (hitSomethingBad)
		{
			continue;
		}

		// Cancel cloak
		cloakTicksAccumulated = 0;

		// Fire
		eq->fire(state, targetPosAdjusted);
		return true;
	}

	return false;
}

void Vehicle::attackTarget(GameState &state, Vec3<float> target)
{
	auto firePosition = getMuzzleLocation();
	auto distanceTiles = glm::length(position - target);

	auto distanceVoxels = this->tileObject->getDistanceTo(target);

	for (auto &eq : this->equipment)
	{
		// Not a weapon
		if (eq->type->type != EquipmentSlotType::VehicleWeapon)
		{
			continue;
		}
		// Out of ammo or on cooldown
		if (eq->canFire() == false)
		{
			continue;
		}
		// Out of range
		if (distanceVoxels > eq->getRange())
		{
			continue;
		}
		// Check firing arc
		if (eq->type->firing_arc_1 < 8 || eq->type->firing_arc_2 < 8)
		{
			Vec2<int> arc = {eq->type->firing_arc_1, 8};
			auto facing = type->directionToVector(direction);
			auto vecToTarget = target - position;
			float angleXY = glm::angle(glm::normalize(Vec2<float>{facing.x, facing.y}),
			                           glm::normalize(Vec2<float>{vecToTarget.x, vecToTarget.y}));
			float vecToTargetXY =
			    sqrtf(vecToTarget.x * vecToTarget.x + vecToTarget.y * vecToTarget.y);
			float angleZ = glm::angle(Vec2<float>{1.0f, 0.0f},
			                          glm::normalize(Vec2<float>{vecToTargetXY, vecToTarget.z}));
			if (angleXY > (float)arc.x * (float)M_PI / 8.0f ||
			    angleZ > (float)arc.y * (float)M_PI / 8.0f)
			{
				continue;
			}
		}

		// Cancel cloak
		cloakTicksAccumulated = 0;

		// Fire
		eq->fire(state, target);
		return;
	}

	return;
}

float Vehicle::getFiringRange() const
{
	float range = 0;
	for (auto &equipment : this->equipment)
	{
		if (equipment->type->type != EquipmentSlotType::VehicleWeapon)
			continue;

		if (range < equipment->getRange())
		{
			range = equipment->getRange();
		}
	}
	return range;
}

void Vehicle::setPosition(const Vec3<float> &pos)
{
	this->position = pos;
	if (!this->tileObject)
	{
		LogError("setPosition called on vehicle with no tile object");
	}
	else
	{
		this->tileObject->setPosition(pos);
	}

	if (this->shadowObject)
	{
		this->shadowObject->setPosition(pos);
	}
}

void Vehicle::setManualFirePosition(const Vec3<float> &pos)
{
	manualFirePosition = pos;
	manualFire = true;
}

bool Vehicle::addMission(GameState &state, VehicleMission *mission, bool toBack)
{
	bool canPlaceInFront = false;
	switch (mission->type)
	{
		// - Can place in front
		// - Can place on crashed vehicles
		// - Can place on carrying vehicles
		case VehicleMission::MissionType::Snooze:
		case VehicleMission::MissionType::RestartNextMission:
			canPlaceInFront = true;
			break;
		// - Cannot place in front but
		// - Can place on crashed vehicles
		// - Can place on carrying vehicles
		case VehicleMission::MissionType::Crash:
		case VehicleMission::MissionType::SelfDestruct:
			break;
		// - Cannot place in front
		// - Cannot place on crashed vehicles
		// - Can place on carrying vehicles
		case VehicleMission::MissionType::GotoLocation:
		case VehicleMission::MissionType::Land:
			if (crashed || sliding || falling)
			{
				delete mission;
				return false;
			}
			break;
		// - Cannot place in front
		// - Cannot place on crashed vehicles
		// - Cannot place on carrying vehicles
		case VehicleMission::MissionType::GotoBuilding:
		case VehicleMission::MissionType::FollowVehicle:
		case VehicleMission::MissionType::RecoverVehicle:
		case VehicleMission::MissionType::AttackVehicle:
		case VehicleMission::MissionType::AttackBuilding:
		case VehicleMission::MissionType::TakeOff:
		case VehicleMission::MissionType::Patrol:
		case VehicleMission::MissionType::GotoPortal:
		case VehicleMission::MissionType::InfiltrateSubvert:
		case VehicleMission::MissionType::OfferService:
		case VehicleMission::MissionType::Teleport:
			if (crashed || sliding || falling || carriedVehicle)
			{
				delete mission;
				return false;
			}
			break;
	}
	if (!toBack && !canPlaceInFront && !missions.empty() &&
	    (missions.front()->type == VehicleMission::MissionType::Land ||
	     missions.front()->type == VehicleMission::MissionType::TakeOff))
	{
		missions.emplace(++missions.begin(), mission);
	}
	else if (!toBack || canPlaceInFront || missions.empty())
	{
		missions.emplace_front(mission);
		missions.front()->start(state, *this);
	}
	else
	{
		missions.emplace_back(mission);
	}
	return true;
}

bool Vehicle::setMission(GameState &state, VehicleMission *mission)
{
	bool forceClear = false;
	switch (mission->type)
	{
		case VehicleMission::MissionType::Crash:
			forceClear = true;
			break;
		case VehicleMission::MissionType::Snooze:
		case VehicleMission::MissionType::SelfDestruct:
			break;
		case VehicleMission::MissionType::GotoLocation:
		case VehicleMission::MissionType::GotoBuilding:
		case VehicleMission::MissionType::FollowVehicle:
		case VehicleMission::MissionType::RecoverVehicle:
		case VehicleMission::MissionType::AttackVehicle:
		case VehicleMission::MissionType::AttackBuilding:
		case VehicleMission::MissionType::RestartNextMission:
		case VehicleMission::MissionType::TakeOff:
		case VehicleMission::MissionType::Land:
		case VehicleMission::MissionType::Patrol:
		case VehicleMission::MissionType::GotoPortal:
		case VehicleMission::MissionType::InfiltrateSubvert:
		case VehicleMission::MissionType::OfferService:
		case VehicleMission::MissionType::Teleport:
			if (crashed || sliding || falling || carriedVehicle)
			{
				delete mission;
				return false;
			}
			break;
	}
	clearMissions(state, forceClear);
	addMission(state, mission, true);
	return true;
}

bool Vehicle::clearMissions(GameState &state, bool forced)
{
	if (forced)
	{
		missions.clear();
		return true;
	}
	for (auto it = missions.begin(); it != missions.end();)
	{
		if ((*it)->type == VehicleMission::MissionType::Land ||
		    (*it)->type == VehicleMission::MissionType::TakeOff)
		{
			it++;
		}
		else
		{
			it = missions.erase(it);
		}
	}
	return missions.empty();
}

bool Vehicle::popFinishedMissions(GameState &state)
{
	bool popped = false;
	while (missions.size() > 0 && missions.front()->isFinished(state, *this))
	{
		LogWarning("Vehicle %s mission \"%s\" finished", name, missions.front()->getName());
		missions.pop_front();
		popped = true;
		if (!missions.empty())
		{
			LogWarning("Vehicle %s mission \"%s\" starting", name, missions.front()->getName());
			missions.front()->start(state, *this);
			continue;
		}
		else
		{
			LogWarning("No next vehicle mission, going idle");
			break;
		}
	}
	return popped;
}

bool Vehicle::getNewGoal(GameState &state)
{
	bool popped = false;
	bool acquired = false;
	// Pop finished missions if present
	popped = popFinishedMissions(state);
	do
	{
		// Try to get new destination
		if (!missions.empty())
		{
			acquired = missions.front()->getNextDestination(state, *this, goalPosition, goalFacing);
		}
		// Pop finished missions if present
		popped = popFinishedMissions(state);
	} while (popped && !acquired);
	return acquired;
}

float Vehicle::getSpeed() const
{
	// FIXME: This is somehow modulated by weight?
	float speed = this->type->top_speed;

	for (auto &e : this->equipment)
	{
		if (e->type->type != EquipmentSlotType::VehicleEngine)
			continue;
		speed += e->type->top_speed;
	}

	return speed;
}

int Vehicle::getMaxConstitution() const { return this->getMaxHealth() + this->getMaxShield(); }

int Vehicle::getConstitution() const { return this->getHealth() + this->getShield(); }

int Vehicle::getMaxHealth() const { return this->type->health; }

int Vehicle::getHealth() const { return this->health; }

int Vehicle::getMaxShield() const
{
	int maxShield = 0;

	for (auto &e : this->equipment)
	{
		if (e->type->type != EquipmentSlotType::VehicleGeneral)
			continue;
		maxShield += e->type->shielding;
	}

	return maxShield;
}

int Vehicle::getShieldRechargeRate() const
{
	int shieldRecharge = 0;

	for (auto &e : this->equipment)
	{
		if (e->type->type != EquipmentSlotType::VehicleGeneral)
			continue;
		shieldRecharge += e->type->shielding > 0 ? 1 : 0;
	}

	return shieldRecharge;
}

bool Vehicle::isCloaked() const
{
	// FIXME: Ensure vehicle cloak implemented correctly
	return cloakTicksAccumulated >= CLOAK_TICKS_REQUIRED_VEHICLE;
}

bool Vehicle::hasCloak() const
{
	for (auto &e : this->equipment)
	{
		if (e->type->type != EquipmentSlotType::VehicleGeneral)
			continue;
		if (e->type->cloaking)
		{
			return true;
		}
	}

	return false;
}

bool Vehicle::canTeleport() const
{
	return teleportTicksAccumulated >= TELEPORT_TICKS_REQUIRED_VEHICLE;
}

bool Vehicle::hasTeleporter() const
{
	// Ground can't use teleporter
	if (type->isGround())
	{
		return false;
	}
	for (auto &e : this->equipment)
	{
		if (e->type->type != EquipmentSlotType::VehicleGeneral)
			continue;
		if (e->type->teleporting)
		{
			return true;
		}
	}

	return false;
}

int Vehicle::getShield() const { return this->shield; }

int Vehicle::getArmor() const
{
	int armor = 0;
	// FIXME: Check this the sum of all directions
	for (auto &armorDirection : this->type->armour)
	{
		armor += armorDirection.second;
	}
	return armor;
}

int Vehicle::getAccuracy() const
{
	int accuracy = 0;
	std::priority_queue<int> accModifiers;

	for (auto &e : this->equipment)
	{
		if (e->type->type != EquipmentSlotType::VehicleGeneral || e->type->accuracy_modifier <= 0)
			continue;
		// accuracy percentages are inverted in the data (e.g. 10% module gives 90)
		accModifiers.push(100 - e->type->accuracy_modifier);
	}

	double moduleEfficiency = 1.0;
	while (!accModifiers.empty())
	{
		accuracy += accModifiers.top() * moduleEfficiency;
		moduleEfficiency /= 2;
		accModifiers.pop();
	}
	return accuracy;
}

// FIXME: Check int/float speed conversions
int Vehicle::getTopSpeed() const { return (int)this->getSpeed(); }

int Vehicle::getAcceleration() const
{
	// FIXME: This is somehow related to enginer 'power' and weight
	int weight = this->getWeight();
	int acceleration = this->type->acceleration;
	int power = 0;
	for (auto &e : this->equipment)
	{
		if (e->type->type != EquipmentSlotType::VehicleEngine)
			continue;
		power += e->type->power;
	}
	if (weight == 0)
	{
		LogError("Vehicle %s has zero weight", this->name);
		return 0;
	}
	acceleration += std::max(1, power / weight);

	if (power == 0 && acceleration == 0)
	{
		// No engine shows a '0' acceleration in the stats ui
		return 0;
	}
	return acceleration;
}

int Vehicle::getWeight() const
{
	int weight = this->type->weight;
	for (auto &e : this->equipment)
	{
		weight += e->type->weight;
	}
	if (weight == 0)
	{
		LogError("Vehicle with no weight");
	}
	return weight;
}

int Vehicle::getFuel() const
{
	// Zero fuel is normal on some vehicles (IE ufos/'dimension-capable' xcom)
	int fuel = 0;

	for (auto &e : this->equipment)
	{
		if (e->type->type != EquipmentSlotType::VehicleEngine)
			continue;
		fuel += e->type->max_ammo;
	}

	return fuel;
}

int Vehicle::getMaxPassengers() const
{
	int passengers = this->type->passengers;

	for (auto &e : this->equipment)
	{
		if (e->type->type != EquipmentSlotType::VehicleGeneral)
		{
			continue;
		}
		passengers += e->type->passengers;
	}
	return passengers;
}

int Vehicle::getPassengers() const { return (int)currentAgents.size(); }

int Vehicle::getMaxCargo() const
{
	int cargoMax = 0;

	for (auto &e : this->equipment)
	{
		if (e->type->type != EquipmentSlotType::VehicleGeneral)
		{
			continue;
		}
		cargoMax += e->type->cargo_space;
	}
	return cargoMax;
}

int Vehicle::getCargo() const
{
	int cargoAmount = 0;
	for (auto &c : cargo)
	{
		if (c.type == Cargo::Type::Bio)
		{
			continue;
		}
		cargoAmount += c.count * c.space / c.divisor;
	}
	return cargoAmount;
}

int Vehicle::getMaxBio() const
{
	int cargoMax = 0;

	for (auto &e : this->equipment)
	{
		if (e->type->type != EquipmentSlotType::VehicleGeneral)
		{
			continue;
		}
		cargoMax += e->type->alien_space;
	}
	return cargoMax;
}

int Vehicle::getBio() const
{
	int cargoAmount = 0;
	for (auto &c : cargo)
	{
		if (c.type != Cargo::Type::Bio)
		{
			continue;
		}
		cargoAmount += c.count * c.space;
	}
	return cargoAmount;
}

bool Vehicle::canAddEquipment(Vec2<int> pos, StateRef<VEquipmentType> type) const
{
	Vec2<int> slotOrigin;
	bool slotFound = false;
	// Check the slot this occupies hasn't already got something there
	for (auto &slot : this->type->equipment_layout_slots)
	{
		if (slot.bounds.within(pos))
		{
			slotOrigin = slot.bounds.p0;
			slotFound = true;
			break;
		}
	}
	// If this was not within a slot fail
	if (!slotFound)
	{
		return false;
	}
	// Check that the equipment doesn't overlap with any other and doesn't
	// go outside a slot of the correct type
	Rect<int> bounds{pos, pos + type->equipscreen_size};
	for (auto &otherEquipment : this->equipment)
	{
		// Something is already in that slot, fail
		if (otherEquipment->equippedPosition == slotOrigin)
		{
			return false;
		}
		Rect<int> otherBounds{otherEquipment->equippedPosition,
		                      otherEquipment->equippedPosition +
		                          otherEquipment->type->equipscreen_size};
		if (otherBounds.intersects(bounds))
		{
			return false;
		}
	}

	// Check that this doesn't go outside a slot of the correct type
	for (int y = 0; y < type->equipscreen_size.y; y++)
	{
		for (int x = 0; x < type->equipscreen_size.x; x++)
		{
			Vec2<int> slotPos = {x, y};
			slotPos += pos;
			bool validSlot = false;
			for (auto &slot : this->type->equipment_layout_slots)
			{
				if (slot.bounds.within(slotPos) && slot.type == type->type)
				{
					validSlot = true;
					break;
				}
			}
			if (!validSlot)
			{
				return false;
			}
		}
	}
	return true;
}

sp<VEquipment> Vehicle::addEquipment(GameState &state, Vec2<int> pos,
                                     StateRef<VEquipmentType> equipmentType)
{
	// We can't check this here, as some of the non-buyable vehicles have insane initial equipment
	// layouts
	// if (!this->canAddEquipment(pos, type))
	//{
	//	LogError("Trying to add \"%s\" at {%d,%d} on vehicle \"%s\" failed", type.id, pos.x,
	//	         pos.y, this->name);
	//}
	Vec2<int> slotOrigin;
	bool slotFound = false;
	// Check the slot this occupies hasn't already got something there
	for (auto &slot : this->type->equipment_layout_slots)
	{
		if (slot.bounds.within(pos))
		{
			slotOrigin = slot.bounds.p0;
			slotFound = true;
			break;
		}
	}
	// If this was not within a slow fail
	if (!slotFound)
	{
		LogError("Equipping \"%s\" on \"%s\" at %s failed: No valid slot", equipmentType->name,
		         this->name, pos);
		return nullptr;
	}

	switch (equipmentType->type)
	{
		case EquipmentSlotType::VehicleEngine:
		{
			auto engine = mksp<VEquipment>();
			engine->type = equipmentType;
			this->equipment.emplace_back(engine);
			engine->equippedPosition = slotOrigin;
			LogInfo("Equipped \"%s\" with engine \"%s\"", this->name, equipmentType->name);
			return engine;
		}
		case EquipmentSlotType::VehicleWeapon:
		{
			auto thisRef = StateRef<Vehicle>(&state, shared_from_this());
			auto weapon = mksp<VEquipment>();
			weapon->type = equipmentType;
			weapon->owner = thisRef;
			weapon->ammo = equipmentType->max_ammo;
			this->equipment.emplace_back(weapon);
			weapon->equippedPosition = slotOrigin;
			LogInfo("Equipped \"%s\" with weapon \"%s\"", this->name, equipmentType->name);
			return weapon;
		}
		case EquipmentSlotType::VehicleGeneral:
		{
			auto equipment = mksp<VEquipment>();
			equipment->type = equipmentType;
			LogInfo("Equipped \"%s\" with general equipment \"%s\"", this->name,
			        equipmentType->name);
			equipment->equippedPosition = slotOrigin;
			this->equipment.emplace_back(equipment);
			return equipment;
		}
		default:
			LogError("Equipment \"%s\" for \"%s\" at pos (%d,%d} has invalid type",
			         equipmentType->name, this->name, pos.x, pos.y);
			return nullptr;
	}
}

sp<VEquipment> Vehicle::addEquipment(GameState &state, StateRef<VEquipmentType> equipmentType)
{
	Vec2<int> pos;
	bool slotFound = false;
	for (auto &slot : type->equipment_layout_slots)
	{
		if (canAddEquipment(slot.bounds.p0, equipmentType))
		{
			pos = slot.bounds.p0;
			slotFound = true;
			break;
		}
	}
	if (!slotFound)
	{
		return nullptr;
	}

	return addEquipment(state, pos, equipmentType);
}

void Vehicle::removeEquipment(sp<VEquipment> object)
{
	this->equipment.remove(object);
	// TODO: Any other variable values here?
	// Clamp shield
	if (this->shield > this->getMaxShield())
	{
		this->shield = this->getMaxShield();
	}
}

void Vehicle::equipDefaultEquipment(GameState &state)
{
	LogInfo("Equipping \"%s\" with default equipment", this->type->name);
	for (auto &pair : this->type->initial_equipment_list)
	{
		auto &pos = pair.first;
		auto &etype = pair.second;

		this->addEquipment(state, pos, etype);
	}
}

void Vehicle::nextFrame(int ticks)
{
	animationDelay += ticks;
	if (animationDelay > 10)
	{
		animationDelay = 0;
		animationFrame++;
		if (animationFrame == type->animation_sprites.end())
		{
			animationFrame = type->animation_sprites.begin();
		}
	}
}
sp<Vehicle> Vehicle::get(const GameState &state, const UString &id)
{
	auto it = state.vehicles.find(id);
	if (it == state.vehicles.end())
	{
		LogError("No vehicle matching ID \"%s\"", id);
		return nullptr;
	}
	return it->second;
}

sp<Equipment> Vehicle::getEquipmentAt(const Vec2<int> &position) const
{
	Vec2<int> slotPosition = {0, 0};
	for (auto &slot : this->type->equipment_layout_slots)
	{
		if (slot.bounds.within(position))
		{
			slotPosition = slot.bounds.p0;
		}
	}
	for (auto &eq : this->equipment)
	{
		Rect<int> eqBounds{eq->equippedPosition, eq->equippedPosition + eq->type->equipscreen_size};
		if (eqBounds.within(slotPosition))
		{
			return eq;
		}
	}
	return nullptr;
}

const std::list<EquipmentLayoutSlot> &Vehicle::getSlots() const
{
	return this->type->equipment_layout_slots;
}

std::list<std::pair<Vec2<int>, sp<Equipment>>> Vehicle::getEquipment() const
{
	std::list<std::pair<Vec2<int>, sp<Equipment>>> equipmentList;

	for (auto &equipmentObject : this->equipment)
	{
		equipmentList.emplace_back(
		    std::make_pair(equipmentObject->equippedPosition, equipmentObject));
	}

	return equipmentList;
}

// FIXME: Implement economy
Cargo::Cargo(GameState &state, StateRef<AEquipmentType> equipment, int count,
             StateRef<Organisation> originalOwner, StateRef<Building> destination)
    : Cargo(state, equipment->bioStorage ? Type::Bio : Type::Agent, equipment.id, count,
            equipment->type == AEquipmentType::Type::Ammo ? equipment->max_ammo : 1,
            equipment->store_space, 1, originalOwner, destination)
{
}

// FIXME: Implement economy
Cargo::Cargo(GameState &state, StateRef<VEquipmentType> equipment, int count,
             StateRef<Organisation> originalOwner, StateRef<Building> destination)
    : Cargo(state, Type::VehicleEquipment, equipment.id, count, 1, equipment->store_space, 1,
            originalOwner, destination)
{
}

// FIXME: Implement economy
Cargo::Cargo(GameState &state, StateRef<VAmmoType> equipment, int count,
             StateRef<Organisation> originalOwner, StateRef<Building> destination)
    : Cargo(state, Type::VehicleAmmo, equipment.id, count, 1, equipment->store_space, 1,
            originalOwner, destination)
{
}

Cargo::Cargo(GameState &state, Type type, UString id, int count, int divisor, int space, int cost,
             StateRef<Organisation> originalOwner, StateRef<Building> destination)
    : type(type), id(id), count(count), divisor(divisor), space(space), cost(cost),
      originalOwner(originalOwner), destination(destination)
{
	suppressEvents = count == 0;
	expirationDate = state.gameTime.getTicks() + TICKS_CARGO_TTL;
}

bool Cargo::checkExpiryDate(GameState &state, StateRef<Building> currentBuilding)
{
	if (expirationDate == 0)
	{
		return false;
	}
	if (expirationDate < state.gameTime.getTicks())
	{
		refund(state, currentBuilding);
		return false;
	}
	if (warned)
	{
		return false;
	}
	if (expirationDate - state.gameTime.getTicks() < TICKS_CARGO_WARNING)
	{
		warned = true;
		return true;
	}
	return false;
}

void Cargo::refund(GameState &state, StateRef<Building> currentBuilding)
{
	if (cost > 0)
	{
		destination->owner->balance += cost * count;
		if (!originalOwner)
		{
			LogError("Bought cargo from nobody!? WTF?");
			return;
		}
		originalOwner->balance -= cost * count;
		if (destination->owner == state.getPlayer())
		{
			fw().pushEvent(
			    new GameBaseEvent(GameEventType::CargoExpired, destination->base, originalOwner));
		}
	}
	else if (currentBuilding && originalOwner == destination->owner)
	{
		destination = currentBuilding;
		if (destination->base && destination->owner == state.getPlayer())
		{
			fw().pushEvent(
			    new GameBaseEvent(GameEventType::CargoExpired, destination->base, originalOwner));
		}
		arrive(state);
	}
	// If no currentBuilding means cargo expired since base destroyed
	else if (currentBuilding)
	{
		if (destination->base && destination->owner == state.getPlayer())
		{
			fw().pushEvent(new GameBaseEvent(GameEventType::CargoExpired, destination->base));
		}
	}
	clear();
}

void Cargo::arrive(GameState &state)
{
	bool cargoArrived;
	bool bioArrived;
	bool recoveryArrived;
	bool transferArrived;
	std::set<StateRef<Organisation>> suppliers;
	arrive(state, cargoArrived, bioArrived, recoveryArrived, transferArrived, suppliers);
}

void Cargo::arrive(GameState &state, bool &cargoArrived, bool &bioArrived, bool &recoveryArrived,
                   bool &transferArrived, std::set<StateRef<Organisation>> &suppliers)
{
	if (destination->base)
	{
		switch (type)
		{
			case Type::Bio:
				destination->base->inventoryBioEquipment[id] += count;
				break;
			case Type::Agent:
				destination->base->inventoryAgentEquipment[id] += count;
				break;
			case Type::VehicleAmmo:
				destination->base->inventoryVehicleAmmo[id] += count;
				break;
			case Type::VehicleEquipment:
				destination->base->inventoryVehicleEquipment[id] += count;
				break;
		}
		// Transfer
		if (originalOwner == state.getPlayer())
		{
			if (type == Type::Bio)
			{
				bioArrived = true;
			}
			else
			{
				cargoArrived = true;
			}
		}
		// Loot
		else if (!originalOwner)
		{
			recoveryArrived = true;
		}
		// Purchase
		else
		{
			cargoArrived = true;
			suppliers.insert(originalOwner);
		}
	}
	destination.clear();
	count = 0;
}

void Cargo::seize(GameState &state, StateRef<Organisation> org)
{
	int worth = cost * count;
	LogWarning("Implement cargo seize message and adjust relationship accordingly to worth: %d",
	           worth);
	clear();
}

void Cargo::clear() { count = 0; }

}; // namespace OpenApoc
