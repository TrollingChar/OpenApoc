#pragma once

#include "game/state/rules/vehicle_type.h"
#include "game/state/stateobject.h"

#include "game/state/tileview/tile.h"
#include "library/strings.h"
#include "library/vec.h"
#include <list>
#include <map>

namespace OpenApoc
{

static const int TELEPORTER_SPREAD = 10;
static const int SELF_DESTRUCT_TIMER = 12 * TICKS_PER_HOUR;

class Vehicle;
class Tile;
class TileMap;
class Building;
class UString;

class FlyingVehicleTileHelper : public CanEnterTileHelper
{
  private:
	TileMap &map;
	VehicleType::Type type;
	bool crashed;
	Vec2<int> size;
	bool large;
	int altitude;

  public:
	FlyingVehicleTileHelper(TileMap &map, Vehicle &v);
	FlyingVehicleTileHelper(TileMap &map, VehicleType &vehType, bool crashed, int altitude);
	FlyingVehicleTileHelper(TileMap &map, VehicleType::Type type, bool crashed, Vec2<int> size,
	                        int altitude);

	bool canEnterTile(Tile *from, Tile *to, bool ignoreStaticUnits = false,
	                  bool ignoreMovingUnits = true, bool ignoreAllUnits = false) const override;

	float pathOverheadAlloawnce() const override;

	// Support 'from' being nullptr for if a vehicle is being spawned in the map
	bool canEnterTile(Tile *from, Tile *to, bool, bool &, float &cost, bool &, bool, bool,
	                  bool) const override;

	float adjustCost(Vec3<int> nextPosition, int z) const override;

	float getDistance(Vec3<float> from, Vec3<float> to) const override;

	float getDistance(Vec3<float> from, Vec3<float> toStart, Vec3<float> toEnd) const override;

	bool canLandOnTile(Tile *to) const;

	Vec3<int> findTileToLandOn(GameState &, sp<TileObjectVehicle> vTile) const;

	Vec3<float> findSidestep(GameState &state, sp<TileObjectVehicle> vTile,
	                         sp<TileObjectVehicle> targetTile, float distancePref) const;
};

class GroundVehicleTileHelper : public CanEnterTileHelper
{
  private:
	TileMap &map;
	VehicleType::Type type;
	bool crashed;

  public:
	GroundVehicleTileHelper(TileMap &map, Vehicle &v);
	GroundVehicleTileHelper(TileMap &map, VehicleType::Type type, bool crashed);

	bool canEnterTile(Tile *from, Tile *to, bool ignoreStaticUnits = false,
	                  bool ignoreMovingUnits = true, bool ignoreAllUnits = false) const override;

	float pathOverheadAlloawnce() const override;

	// Support 'from' being nullptr for if a vehicle is being spawned in the map
	bool canEnterTile(Tile *from, Tile *to, bool, bool &, float &cost, bool &, bool, bool,
	                  bool) const override;

	float getDistance(Vec3<float> from, Vec3<float> to) const override;

	float getDistance(Vec3<float> from, Vec3<float> toStart, Vec3<float> toEnd) const override;

	// Convert vector direction into index for tube array
	int convertDirection(Vec3<int> dir) const;

	bool isMoveAllowedRoad(Scenery &scenery, int dir) const;
	bool isMoveAllowedATV(Scenery &scenery, int dir) const;
};

class VehicleMission
{
  private:
	// INTERNAL: Not to be used directly (Only works when in building)
	static VehicleMission *takeOff(Vehicle &v);
	// INTERNAL: Not to be used directly (Only works if directly above a pad)
	static VehicleMission *land(Vehicle &v, StateRef<Building> b);
	// INTERNAL: This checks if mission is actually finished. Called by isFinished.
	// If it is finished, update() is called by isFinished so that any remaining work could be done
	bool isFinishedInternal(GameState &state, Vehicle &v);

	// Adjusts target to match closest tile valid for road vehicles
	static bool adjustTargetToClosestRoad(Vehicle &v, Vec3<int> &target);
	// Adjusts target to match closest tile valid for ATVs
	static bool adjustTargetToClosestGround(Vehicle &v, Vec3<int> &target);
	// Adjusts target to match closest tile valid for Flyers
	// Ignore vehicles short version
	static bool adjustTargetToClosestFlying(GameState &state, Vehicle &v, Vec3<int> &target);
	// Adjusts target to match closest tile valid for Flyers
	static bool adjustTargetToClosestFlying(GameState &state, Vehicle &v, Vec3<int> &target,
	                                        bool ignoreVehicles, bool pickNearest,
	                                        bool &pickedNearest);

	bool takeOffCheck(GameState &state, Vehicle &v);
	bool teleportCheck(GameState &state, Vehicle &v);

  public:
	VehicleMission() = default;

	// Methods used in pathfinding etc.
	bool getNextDestination(GameState &state, Vehicle &v, Vec3<float> &destPos, float &destFacing);
	void update(GameState &state, Vehicle &v, unsigned int ticks, bool finished = false);
	bool isFinished(GameState &state, Vehicle &v, bool callUpdateIfFinished = true);
	void start(GameState &state, Vehicle &v);
	void setPathTo(GameState &state, Vehicle &v, Vec3<int> target, int maxIterations,
	               bool checkValidity = true, bool giveUpIfInvalid = false);
	bool advanceAlongPath(GameState &state, Vehicle &v, Vec3<float> &destPos, float &destFacing);
	bool isTakingOff(Vehicle &v);
	int getDefaultIterationCount(Vehicle &v);

	// Methods to create new missions

	static VehicleMission *gotoLocation(GameState &state, Vehicle &v, Vec3<int> target,
	                                    bool allowTeleporter = false, bool pickNearest = false,
	                                    int reRouteAttempts = 20);
	static VehicleMission *gotoPortal(GameState &state, Vehicle &v);
	static VehicleMission *gotoPortal(GameState &state, Vehicle &v, Vec3<int> target);
	// With now building goes home
	static VehicleMission *gotoBuilding(GameState &state, Vehicle &v,
	                                    StateRef<Building> target = nullptr,
	                                    bool allowTeleporter = false);
	static VehicleMission *infiltrateOrSubvertBuilding(GameState &state, Vehicle &v,
	                                                   StateRef<Building> target,
	                                                   bool subvert = false);
	static VehicleMission *attackVehicle(GameState &state, Vehicle &v, StateRef<Vehicle> target);
	static VehicleMission *attackBuilding(GameState &state, Vehicle &v, StateRef<Building> target);
	static VehicleMission *followVehicle(GameState &state, Vehicle &v, StateRef<Vehicle> target);
	static VehicleMission *recoverVehicle(GameState &state, Vehicle &v, StateRef<Vehicle> target);
	static VehicleMission *offerService(GameState &state, Vehicle &v,
	                                    StateRef<Building> target = nullptr);
	static VehicleMission *snooze(GameState &state, Vehicle &v, unsigned int ticks);
	static VehicleMission *selfDestruct(GameState &state, Vehicle &v);
	static VehicleMission *restartNextMission(GameState &state, Vehicle &v);
	static VehicleMission *crashLand(GameState &state, Vehicle &v);
	static VehicleMission *patrol(GameState &state, Vehicle &v, bool home = false,
	                              unsigned int counter = 10);
	static VehicleMission *teleport(GameState &state, Vehicle &v, Vec3<int> target = {-1, -1, -1});
	UString getName();

	enum class MissionType
	{
		GotoLocation,
		GotoBuilding,
		FollowVehicle,
		RecoverVehicle,
		AttackVehicle,
		AttackBuilding,
		RestartNextMission,
		Snooze,
		TakeOff,
		Land,
		Crash,
		Patrol,
		GotoPortal,
		InfiltrateSubvert,
		OfferService,
		Teleport,
		SelfDestruct
	};

	MissionType type = MissionType::GotoLocation;

	// GotoLocation InfiltrateSubvert TakeOff GotoPortal Patrol
	Vec3<int> targetLocation = {0, 0, 0};
	// GotoLocation GotoBuilding
	bool allowTeleporter = false;
	// How many times will vehicle try to re-route until it gives up
	int reRouteAttempts = 0;
	// GotoLocation - should it pick nearest point or random point if destination unreachable
	bool pickNearest = false;
	// Patrol - should patrol around home building only
	bool patrolHome = false;
	// GotoLocation - picked nearest (allows finishing mission without reaching destination)
	bool pickedNearest = false;
	// GotoBuilding AttackBuilding Land Infiltrate
	StateRef<Building> targetBuilding;
	// FollowVehicle AttackVehicle
	StateRef<Vehicle> targetVehicle;
	// Snooze, SelfDestruct
	unsigned int timeToSnooze = 0;
	// RecoverVehicle, InfiltrateSubvert, Patrol: waypoints
	unsigned int missionCounter = 0;
	// InfiltrateSubvert: mode
	bool subvert = false;
	// AttackVehicle
	bool attackCrashed = false;

	bool cancelled = false;

	std::list<Vec3<int>> currentPlannedPath;
};
} // namespace OpenApoc
