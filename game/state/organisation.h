#pragma once

#include "game/state/gametime.h"
#include "game/state/stateobject.h"
#include "library/strings.h"
#include <list>
#include <map>
#include <set>
#include <vector>

namespace OpenApoc
{

// Original game checked one org every 5 minutes, which resulted in 125 minutes per check
// We are going to randomize accumulated ticks instead and check every 125 minutes,
// this would provide the same frequency (and thus the same takeover chance) regardless
// of how many orgs there are
static const unsigned TICKS_PER_TAKEOVER_ATTEMPT = TICKS_PER_MINUTE * 125;

class Vehicle;
class VehicleType;
class Building;
class AgentType;
class VEquipmentType;
class City;
class AEquipmentType;
class VAmmoType;
class GameState;
class VehicleType;
class UfopaediaEntry;

class Organisation : public StateObject
{
	STATE_OBJECT(Organisation)
  public:
	enum class PurchaseResult
	{
		OK,
		NoTransportAvailable,
		OrgHostile,
		OrgHasNoBuildings,
	};
	enum class Relation
	{
		Allied,
		Friendly,
		Neutral,
		Unfriendly,
		Hostile
	};
	enum class LootPriority
	{
		A,
		B,
		C
	};
	class MissionPattern
	{
	  public:
		enum class Target
		{
			Owned,
			OwnedOrOther,
			Other,
			ArriveFromSpace,
			DepartToSpace,
		};
		uint64_t minIntervalRepeat = 0;
		uint64_t maxIntervalRepeat = 0;
		unsigned minAmount = 0;
		unsigned maxAmount = 0;
		std::set<StateRef<VehicleType>> allowedTypes;
		Target target = Target::Owned;
		std::set<Relation> relation;

		MissionPattern() = default;
		MissionPattern(uint64_t minIntervalRepeat, uint64_t maxIntervalRepeat, unsigned minAmount,
		               unsigned maxAmount, std::set<StateRef<VehicleType>> allowedTypes,
		               Target target, std::set<Relation> relation = {});
	};
	class Mission
	{
	  public:
		uint64_t next = 0;
		MissionPattern pattern;

		void execute(GameState &state, StateRef<Organisation> owner);

		Mission() = default;
		Mission(uint64_t next, uint64_t minIntervalRepeat, uint64_t maxIntervalRepeat,
		        unsigned minAmount, unsigned maxAmount,
		        std::set<StateRef<VehicleType>> allowedTypes, MissionPattern::Target target,
		        std::set<Relation> relation = {});
	};

	UString id;
	UString name;
	int balance = 0;
	int income = 0;
	int infiltrationValue = 0;
	// Modified for all infiltration attempts at this org
	int infiltrationSpeed = 0;
	bool takenOver = false;
	unsigned int ticksTakeOverAttemptAccumulated = 0;

	int tech_level = 1;
	int average_guards = 1;
	// What guard types can spawn, supports duplicates to provide variable probability
	std::list<StateRef<AgentType>> guard_types_reinforcements;
	std::list<StateRef<AgentType>> guard_types_human;
	std::list<StateRef<AgentType>> guard_types_alien;

	std::map<LootPriority, std::vector<StateRef<AEquipmentType>>> loot;

	StateRef<UfopaediaEntry> ufopaedia_entry;

	std::map<StateRef<City>, std::list<Mission>> missions;
	std::map<StateRef<VehicleType>, int> vehiclePark;
	bool providesTransportationServices = false;
	int minHireePool = 0;
	int maxHireePool = 0;
	std::set<StateRef<AgentType>> hirableTypes;

	Organisation() = default;

	void updateMissions(GameState &state);
	void updateHirableAgents(GameState &state);
	void updateInfiltration(GameState &state);
	void updateTakeOver(GameState &state, unsigned int ticks);
	void updateVehicleAgentPark(GameState &state);

	int getGuardCount(GameState &state) const;

	void takeOver(GameState &state, bool forced = false);

	PurchaseResult canPurchaseFrom(GameState &state, const StateRef<Building> &buyer,
	                               bool vehicle) const;
	StateRef<Building> getPurchaseBuilding(GameState &state, const StateRef<Building> &buyer) const;
	void purchase(GameState &state, const StateRef<Building> &buyer,
	              StateRef<VEquipmentType> vehicleEquipment, int count);
	void purchase(GameState &state, const StateRef<Building> &buyer,
	              StateRef<VAmmoType> vehicleAmmo, int count);
	void purchase(GameState &state, const StateRef<Building> &buyer,
	              StateRef<AEquipmentType> agentEquipment, int count);
	void purchase(GameState &state, const StateRef<Building> &buyer, StateRef<VehicleType> vehicle,
	              int count);

	Relation isRelatedTo(const StateRef<Organisation> &other) const;
	bool isPositiveTo(const StateRef<Organisation> &other) const;
	bool isNegativeTo(const StateRef<Organisation> &other) const;
	float getRelationTo(const StateRef<Organisation> &other) const;
	void adjustRelationTo(GameState &state, StateRef<Organisation> other, float value);
	std::map<StateRef<Organisation>, float> current_relations;
};

}; // namespace OpenApoc
