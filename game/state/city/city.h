#pragma once

#include "game/state/stateobject.h"
#include "library/sp.h"
#include "library/vec.h"
#include <list>
#include <map>
#include <set>

namespace OpenApoc
{

#define TILE_X_CITY (64)
#define TILE_Y_CITY (32)
#define TILE_Z_CITY (16)

#define VOXEL_X_CITY (32)
#define VOXEL_Y_CITY (32)
#define VOXEL_Z_CITY (16)

class Vehicle;
class VehicleType;
class GameState;
class Building;
class Projectile;
class Scenery;
class Doodad;
class DoodadType;
class SceneryTileType;
class Organisation;
class BaseLayout;
class Agent;
class TileMap;

class City : public StateObject
{
	STATE_OBJECT(City)
  public:
	City() = default;
	~City() override;

	void initMap(GameState &state);

	UString id;
	Vec3<int> size;

	StateRefMap<SceneryTileType> tile_types;
	std::map<Vec3<int>, StateRef<SceneryTileType>> initial_tiles;
	StateRefMap<Building> buildings;
	std::vector<sp<Scenery>> scenery;
	std::list<sp<Doodad>> doodads;
	std::vector<sp<Doodad>> portals;

	std::set<sp<Projectile>> projectiles;

	up<TileMap> map;

	// CityView and CityTileView settings, saved here so that we can return to them

	Vec3<float> cityViewScreenCenter = {0.0f, 0.0f, 0.0f};
	int cityViewPageIndex = 0;
	std::list<StateRef<Vehicle>> cityViewSelectedVehicles;
	std::list<StateRef<Agent>> cityViewSelectedAgents;

	void handleProjectileHit(GameState &state, sp<Projectile> projectile, bool displayDoodad,
	                         bool playSound);

	void update(GameState &state, unsigned int ticks);
	void hourlyLoop(GameState &state);
	void dailyLoop(GameState &state);

	void generatePortals(GameState &state);
	void updateInfiltration(GameState &state);

	void initialSceneryLinkUp();

	sp<Doodad> placeDoodad(StateRef<DoodadType> type, Vec3<float> position);
	sp<Vehicle> placeVehicle(GameState &state, StateRef<VehicleType> type,
	                         StateRef<Organisation> owner);
	sp<Vehicle> placeVehicle(GameState &state, StateRef<VehicleType> type,
	                         StateRef<Organisation> owner, StateRef<Building> building);
	sp<Vehicle> placeVehicle(GameState &state, StateRef<VehicleType> type,
	                         StateRef<Organisation> owner, Vec3<float> position,
	                         float facing = 0.0f);

	// Move a group of vehicles in formation
	void groupMove(GameState &state, std::list<StateRef<Vehicle>> &selectedVehicles,
	               Vec3<int> targetLocation, bool useTeleporter = false);

	static void accuracyAlgorithmCity(GameState &state, Vec3<float> firePosition,
	                                  Vec3<float> &target, int accuracy, bool cloaked);
};

}; // namespace OpenApoc
