#include "game/state/gamestate.h"
#include "framework/configfile.h"
#include "framework/data.h"
#include "framework/framework.h"
#include "framework/sound.h"
#include "framework/trace.h"
#include "game/state/aequipment.h"
#include "game/state/base/base.h"
#include "game/state/base/facility.h"
#include "game/state/battle/battle.h"
#include "game/state/battle/battlecommonsamplelist.h"
#include "game/state/battle/battlemap.h"
#include "game/state/battle/battlemappart_type.h"
#include "game/state/battle/battleunitanimationpack.h"
#include "game/state/battle/battleunitimagepack.h"
#include "game/state/city/baselayout.h"
#include "game/state/city/building.h"
#include "game/state/city/city.h"
#include "game/state/city/citycommonimagelist.h"
#include "game/state/city/citycommonsamplelist.h"
#include "game/state/city/doodad.h"
#include "game/state/city/projectile.h"
#include "game/state/city/scenery.h"
#include "game/state/city/vehicle.h"
#include "game/state/city/vehiclemission.h"
#include "game/state/gameevent.h"
#include "game/state/gametime.h"
#include "game/state/message.h"
#include "game/state/organisation.h"
#include "game/state/rules/aequipment_type.h"
#include "game/state/rules/damage.h"
#include "game/state/rules/doodad_type.h"
#include "game/state/rules/scenery_tile_type.h"
#include "game/state/rules/ufo_growth.h"
#include "game/state/rules/ufo_incursion.h"
#include "game/state/rules/vammo_type.h"
#include "game/state/rules/vehicle_type.h"
#include "game/state/tileview/tile.h"
#include "game/state/tileview/tileobject_vehicle.h"
#include "game/state/ufopaedia.h"
#include "library/strings_format.h"
#include <random>

namespace OpenApoc
{

GameState::GameState() : player(this) {}

GameState::~GameState()
{
	if (this->current_battle)
	{
		Battle::finishBattle(*this);
		Battle::exitBattle(*this);
	}
	for (auto &a : this->agents)
	{
		a.second->destroy();
	}
	for (auto &v : this->vehicles)
	{
		auto vehicle = v.second;
		if (vehicle->tileObject)
		{
			vehicle->tileObject->removeFromMap();
		}
		vehicle->tileObject = nullptr;
		// Detatch some back-pointers otherwise we get circular sp<> depedencies and leak
		// FIXME: This is not a 'good' way of doing this, maybe add a destroyVehicle() function? Or
		// make StateRefWeak<> or something?
		//
		vehicle->city.clear();
		vehicle->homeBuilding.clear();
		vehicle->currentBuilding.clear();
		vehicle->missions.clear();
		vehicle->equipment.clear();
		vehicle->mover = nullptr;
	}
	for (auto &b : this->player_bases)
	{
		for (auto &f : b.second->facilities)
		{
			if (f->lab)
				f->lab->current_project = "";
			f->lab = "";
		}
		b.second->building.clear();
	}
	for (auto &org : this->organisations)
	{
		org.second->current_relations.clear();
	}
}

// Just a handy shortcut since it's shown on every single screen
UString GameState::getPlayerBalance() const
{
	return Strings::fromInteger(this->getPlayer()->balance);
}

StateRef<Organisation> GameState::getOrganisation(const UString &orgID)
{
	return StateRef<Organisation>(this, orgID);
}

const StateRef<Organisation> &GameState::getPlayer() const { return this->player; }
StateRef<Organisation> GameState::getPlayer() { return this->player; }
const StateRef<Organisation> &GameState::getAliens() const { return this->aliens; }
StateRef<Organisation> GameState::getAliens() { return this->aliens; }
const StateRef<Organisation> &GameState::getGovernment() const { return this->government; }
StateRef<Organisation> GameState::getGovernment() { return this->government; }
const StateRef<Organisation> &GameState::getCivilian() const { return this->civilian; }
StateRef<Organisation> GameState::getCivilian() { return this->civilian; }

void GameState::initState()
{
	// FIXME: reseed rng when game starts

	if (current_battle)
	{
		current_battle->initBattle(*this);
	}

	for (auto &c : this->cities)
	{
		auto &city = c.second;
		for (auto &s : city->scenery)
		{
			for (auto &b : city->buildings)
			{
				auto &building = b.second;
				Vec2<int> pos2d{s->initialPosition.x, s->initialPosition.y};
				if (building->bounds.within(pos2d))
				{
					s->building = {this, building};
					if (s->isAlive() && !s->type->commonProperty)
					{
						s->building->buildingParts.insert(s->initialPosition);
					}
					break;
				}
			}
		}
	}
	for (auto &c : this->cities)
	{
		auto &city = c.second;
		city->initMap(*this);
		if (newGame)
		{
			if (c.first == "CITYMAP_HUMAN")
			{
				city->initialSceneryLinkUp();
			}
		}
		// Add vehicles to map
		for (auto &v : this->vehicles)
		{
			auto vehicle = v.second;
			if (vehicle->city == city && !vehicle->currentBuilding)
			{
				city->map->addObjectToMap(vehicle);
			}
		}
		for (auto &p : c.second->projectiles)
		{
			if (p->trackedVehicle)
				p->trackedObject = p->trackedVehicle->tileObject;
		}
		if (city->portals.empty())
		{
			city->generatePortals(*this);
		}
	}
	for (auto &v : this->vehicles)
	{
		v.second->strategyImages = city_common_image_list->strategyImages;
		v.second->setupMover();
		if (v.second->crashed)
		{
			v.second->smokeDoodad =
			    v.second->city->placeDoodad({this, "DOODAD_13_SMOKE_FUME"}, v.second->position);
		}
	}
	// Fill links for weapon's ammo
	for (auto &t : this->agent_equipment)
	{
		for (auto &w : t.second->weapon_types)
		{
			w->ammo_types.emplace_back(this, t.first);
		}
	}
	for (auto &a : this->agent_types)
	{
		a.second->gravLiftSfx = battle_common_sample_list->gravlift;
	}
	for (auto &a : this->agents)
	{
		a.second->leftHandItem = a.second->getFirstItemInSlot(EquipmentSlotType::LeftHand, false);
		a.second->rightHandItem = a.second->getFirstItemInSlot(EquipmentSlotType::RightHand, false);
	}
	// Run nessecary methods for different types
	research.updateTopicList();
	// Apply mods (Stub until we actually have mods)
	applyMods();
	// Validate
	validate();
}

void GameState::applyMods()
{
	if (config().getBool("OpenApoc.Mod.ATVTank"))
	{
		vehicle_types["VEHICLETYPE_GRIFFON_AFV"]->type = VehicleType::Type::ATV;
	}
	else
	{
		vehicle_types["VEHICLETYPE_GRIFFON_AFV"]->type = VehicleType::Type::Road;
	}

	if (config().getBool("OpenApoc.Mod.BSKLauncherSound"))
	{
		agent_equipment["AEQUIPMENTTYPE_BRAINSUCKER_LAUNCHER"]->fire_sfx =
		    fw().data->loadSample("RAWSOUND:xcom3/rawsound/tactical/weapons/sucklaun.raw:22050");
	}
	else
	{
		agent_equipment["AEQUIPMENTTYPE_BRAINSUCKER_LAUNCHER"]->fire_sfx =
		    fw().data->loadSample("RAWSOUND:xcom3/rawsound/tactical/weapons/powers.raw:22050");
	}

	bool crashVehicles = config().getBool("OpenApoc.Mod.CrashingVehicles");
	for (auto &e : vehicle_types)
	{
		if (e.second->type != VehicleType::Type::UFO)
		{
			e.second->crash_health = crashVehicles ? e.second->health / 7 : 0;
		}
	}
}

void GameState::validate()
{
	LogWarning("Validating GameState");
	validateResearch();
	validateScenery();
	LogWarning("Validated GameState");
}

void GameState::validateResearch()
{
	for (auto &t : research.topics)
	{
		if (t.second->type == ResearchTopic::Type::Engineering)
		{
			if (t.second->itemId.length() == 0)
			{
				LogError("EMPTY REFERENCE resulting item for %s", t.first);
			}
			else
			{
				bool fail = false;
				switch (t.second->item_type)
				{
					case ResearchTopic::ItemType::VehicleEquipment:
						if (vehicle_equipment.find(t.second->itemId) == vehicle_equipment.end())
						{
							fail = true;
						}
						break;
					case ResearchTopic::ItemType::VehicleEquipmentAmmo:
						if (vehicle_ammo.find(t.second->itemId) == vehicle_ammo.end())
						{
							fail = true;
						}
						break;
					case ResearchTopic::ItemType::AgentEquipment:
						if (agent_equipment.find(t.second->itemId) == agent_equipment.end())
						{
							fail = true;
						}
						break;
					case ResearchTopic::ItemType::Craft:
						if (vehicle_types.find(t.second->itemId) == vehicle_types.end())
						{
							fail = true;
						}
						break;
				}
				if (fail)
				{
					LogError("%s DOES NOT EXIST: referenced as manufactured by %s",
					         t.second->itemId, t.first);
				}
			}
		}
		for (auto &rd : t.second->dependencies.research)
		{
			for (auto &topic : rd.topics)
			{
				if (topic.id.length() == 0)
				{
					LogError("EMPTY REFERENCE required topic for %s", t.first);
				}
				else if (research.topics.find(topic.id) == research.topics.end())
				{
					LogError("%s DOES NOT EXIST: referenced as required topic for %s", topic.id,
					         t.first);
				}
			}
		}
		for (auto &entry : t.second->dependencies.items.agentItemsRequired)
		{
			if (entry.first.id.length() == 0)
			{
				LogError("EMPTY REFERENCE required item for %s", t.first);
			}
			else if (agent_equipment.find(entry.first.id) == agent_equipment.end())
			{
				LogError("%s DOES NOT EXIST: referenced as required item for %s", entry.first.id,
				         t.first);
			}
		}
		for (auto &entry : t.second->dependencies.items.agentItemsConsumed)
		{
			if (entry.first.id.length() == 0)
			{
				LogError("EMPTY REFERENCE consumed item for %s", t.first);
			}
			else if (agent_equipment.find(entry.first.id) == agent_equipment.end())
			{
				LogError("%s DOES NOT EXIST: referenced as consumed item for %s", entry.first.id,
				         t.first);
			}
		}
		for (auto &entry : t.second->dependencies.items.vehicleItemsRequired)
		{
			if (entry.first.id.length() == 0)
			{
				LogError("EMPTY REFERENCE required item for %s", t.first);
			}
			else if (vehicle_equipment.find(entry.first.id) == vehicle_equipment.end())
			{
				LogError("%s DOES NOT EXIST: referenced as required item for %s", entry.first.id,
				         t.first);
			}
		}
		for (auto &entry : t.second->dependencies.items.vehicleItemsConsumed)
		{
			if (entry.first.id.length() == 0)
			{
				LogError("EMPTY REFERENCE consumed item for %s", t.first);
			}
			else if (vehicle_equipment.find(entry.first.id) == vehicle_equipment.end())
			{
				LogError("%s DOES NOT EXIST: referenced as consumed item for %s", entry.first.id,
				         t.first);
			}
		}
		for (auto &entry : t.second->dependencies.items.agentItemsConsumed)
		{
			if (t.second->dependencies.items.agentItemsRequired.find(entry.first) ==
			    t.second->dependencies.items.agentItemsRequired.end())
			{
				LogError("Consumed agent item %s not in required list for topic %s", entry.first.id,
				         t.first);
			}
			else if (t.second->dependencies.items.agentItemsRequired.at(entry.first) < entry.second)
			{
				LogError("Consumed agent items %s has bigger count than required for topic %s",
				         entry.first.id, t.first);
			}
		}
		for (auto &entry : t.second->dependencies.items.vehicleItemsConsumed)
		{
			if (t.second->dependencies.items.vehicleItemsRequired.find(entry.first) ==
			    t.second->dependencies.items.vehicleItemsRequired.end())
			{
				LogError("Consumed vehicle item %s not in required list for topic %s",
				         entry.first.id, t.first);
			}
			else if (t.second->dependencies.items.vehicleItemsRequired.at(entry.first) <
			         entry.second)
			{
				LogError("Consumed vehicle item %s has bigger count than required for topic %s",
				         entry.first.id, t.first);
			}
		}
	}
}

void GameState::validateScenery()
{
	for (auto &c : cities)
	{
		for (auto &sc : c.second->tile_types)
		{
			if (sc.second->getATVMode() == SceneryTileType::WalkMode::Onto &&
			    sc.second->height == 0)
			{
				/*LogError("City %s Scenery %s has no height and WalkMode::Onto? Missing voxelmap?",
				         c.first, sc.first);*/
			}
		}
	}
}

void GameState::fillOrgStartingProperty()
{
	auto buildingIt = this->cities["CITYMAP_HUMAN"]->buildings.begin();

	for (auto &o : this->organisations)
	{
		o.second->updateVehicleAgentPark(*this);
		for (auto &m : o.second->missions[{this, "CITYMAP_HUMAN"}])
		{
			m.next += TICKS_PER_HOUR * 12 + randBoundsInclusive(rng, (uint64_t)0,
			                                                    m.pattern.maxIntervalRepeat -
			                                                        m.pattern.minIntervalRepeat) -
			          m.pattern.minIntervalRepeat / 2;
		}
	}
}

void GameState::startGame()
{
	agentEquipmentTemplates.resize(10);

	// Setup orgs
	for (auto &pair : this->organisations)
	{
		pair.second->ticksTakeOverAttemptAccumulated =
		    randBoundsExclusive(rng, (unsigned)0, TICKS_PER_TAKEOVER_ATTEMPT);
		// Initial relationship randomiser
		// Not for player or civilians
		if (pair.first == player.id || pair.first == civilian.id)
		{
			continue;
		}
		for (auto &entry : pair.second->current_relations)
		{
			// Not for civilians or perfect relationships
			if (entry.second == 100.0f || entry.first == civilian)
			{
				continue;
			}
			// First step: adjust based on difficulty
			// higher difficulty will produce a bigger sway
			if (difficulty > 0)
			{
				// Relationship vs player is adjusted by flat 0/5/0/-5/-10
				if (entry.first == player)
				{
					entry.second += 10 - 5 * difficulty;
				}
				// Positive relationship is improved randomly
				else if (entry.second >= 0.0f)
				{
					entry.second += randBoundsInclusive(rng, 0, 3 * difficulty);
				}
				// Negative relationship with non-aliens is worsened randomly
				else if (entry.first != aliens)
				{
					entry.second -= randBoundsInclusive(rng, 0, 5 * difficulty);
				}
			}
			// Second step: random +- 10
			entry.second += randBoundsInclusive(rng, -10, 10);

			// Finally stay in bounds
			entry.second = clamp(entry.second, -100.0f, 100.0f);
		}
	}
	// Setup buildings
	for (auto &pair : this->cities)
	{
		for (auto &b : pair.second->buildings)
		{
			b.second->ticksDetectionAttemptAccumulated =
			    randBoundsExclusive(rng, (unsigned)0, TICKS_PER_DETECTION_ATTEMPT[difficulty]);
		}
	}
	// Setup scenery
	for (auto &pair : this->cities)
	{
		auto &city = pair.second;
		// Start the game with all buildings whole
		for (auto &tilePair : city->initial_tiles)
		{
			auto s = mksp<Scenery>();

			s->type = tilePair.second;
			s->initialPosition = tilePair.first;
			s->currentPosition = s->initialPosition;

			city->scenery.push_back(s);
		}
	}

	// Add aliens into random building
	int counter = 0;
	int giveUpCount = 100;
	auto buildingIt = this->cities["CITYMAP_HUMAN"]->buildings.begin();
	do
	{
		int buildID =
		    randBoundsExclusive(rng, 0, (int)this->cities["CITYMAP_HUMAN"]->buildings.size());
		buildingIt = this->cities["CITYMAP_HUMAN"]->buildings.begin();
		for (int i = 0; i < buildID; i++)
		{
			buildingIt++;
		}
		counter++;
	} while (buildingIt->second->owner->current_relations[player] < 0 || counter >= giveUpCount);

	for (auto &l : initial_aliens.at(difficulty))
	{
		buildingIt->second->current_crew[l.first] =
		    randBoundsExclusive(rng, l.second.x, l.second.y);
	}

	gameTime = GameTime::midday();

	newGame = true;
	firstDetection = true;
}

// Fills out initial player property
void GameState::fillPlayerStartingProperty()
{
	// Create the intial starting base
	// Randomly shuffle buildings until we find one with a base layout
	sp<City> humanCity = this->cities["CITYMAP_HUMAN"];
	this->current_city = {this, humanCity};

	std::vector<sp<Building>> buildingsWithBases;
	for (auto &b : humanCity->buildings)
	{
		if (b.second->base_layout)
			buildingsWithBases.push_back(b.second);
	}

	if (buildingsWithBases.empty())
	{
		LogError("City map has no buildings with valid base layouts");
	}

	std::uniform_int_distribution<int> bldDist(0, buildingsWithBases.size() - 1);

	auto bld = buildingsWithBases[bldDist(this->rng)];

	auto base = mksp<Base>(*this, StateRef<Building>{this, bld});
	base->startingBase(*this);
	base->name = "Base " + Strings::fromInteger(this->player_bases.size() + 1);
	this->player_bases[Base::getPrefix() + Strings::fromInteger(this->player_bases.size() + 1)] =
	    base;
	bld->owner = this->getPlayer();
	bld->base = {this, base};
	this->current_base = {this, base};

	// Give the player one of each equipable vehicle
	for (auto &it : this->vehicle_types)
	{
		auto &type = it.second;
		if (!type->equipment_screen)
			continue;
		auto v = current_city->placeVehicle(*this, {this, type}, this->getPlayer(), {this, bld});
		v->homeBuilding = v->currentBuilding;
	}
	// Give that base some vehicle inventory
	for (auto &pair : this->vehicle_equipment)
	{
		auto &equipmentID = pair.first;
		base->inventoryVehicleEquipment[equipmentID] = 10;
	}
	// Give base starting agent equipment
	for (auto &pair : this->initial_base_agent_equipment)
	{
		auto &equipmentID = pair.first;
		base->inventoryAgentEquipment[equipmentID] = pair.second;
	}
	// Give starting agents and their equipment
	for (auto &agentTypePair : this->initial_agents)
	{
		auto type = agentTypePair.first;
		auto count = agentTypePair.second;
		auto it = initial_agent_equipment.begin();
		while (count > 0)
		{
			auto agent = this->agent_generator.createAgent(*this, this->getPlayer(), type);
			agent->homeBuilding = base->building;
			agent->city = agent->homeBuilding->city;
			agent->enterBuilding(*this, agent->homeBuilding);
			count--;
			if (type == AgentType::Role::Soldier && it != initial_agent_equipment.end())
			{
				for (auto &t : *it)
				{
					if (t->type == AEquipmentType::Type::Armor)
					{
						EquipmentSlotType slotType = EquipmentSlotType::General;
						switch (t->body_part)
						{
							case BodyPart::Body:
								slotType = EquipmentSlotType::ArmorBody;
								break;
							case BodyPart::Legs:
								slotType = EquipmentSlotType::ArmorLegs;
								break;
							case BodyPart::Helmet:
								slotType = EquipmentSlotType::ArmorHelmet;
								break;
							case BodyPart::LeftArm:
								slotType = EquipmentSlotType::ArmorLeftHand;
								break;
							case BodyPart::RightArm:
								slotType = EquipmentSlotType::ArmorRightHand;
								break;
						}
						agent->addEquipmentByType(*this, {this, t->id}, slotType, false);
					}
					else if (t->type == AEquipmentType::Type::Ammo ||
					         t->type == AEquipmentType::Type::MediKit ||
					         t->type == AEquipmentType::Type::Grenade)
					{
						agent->addEquipmentByType(*this, {this, t->id}, EquipmentSlotType::General,
						                          false);
					}
					else
					{
						agent->addEquipmentByType(*this, {this, t->id}, false);
					}
				}
				it++;
			}
		}
	}
}

bool GameState::canTurbo() const
{
	if (!this->current_city->projectiles.empty())
	{
		return false;
	}
	for (auto &v : this->vehicles)
	{
		if (!v.second->isDead() && v.second->city == this->current_city &&
		    v.second->tileObject != nullptr)
		{
			if (v.second->type->aggressiveness > 0 &&
			    v.second->owner->isRelatedTo(this->getPlayer()) ==
			        Organisation::Relation::Hostile &&
			    !v.second->crashed)
			{
				return false;
			}
			for (auto &m : v.second->missions)
			{
				if (m->type == VehicleMission::MissionType::AttackBuilding ||
				    m->type == VehicleMission::MissionType::AttackVehicle)
				{
					return false;
				}
			}
		}
	}
	return true;
}

void GameState::update(unsigned int ticks)
{
	if (this->current_battle)
	{
		Trace::start("GameState::update::battles");
		this->current_battle->update(*this, ticks);
		Trace::end("GameState::update::battles");
		gameTime.addTicks(ticks);
	}
	else
	{
		// Roll back to time before battle and stuff
		if (gameTimeBeforeBattle.getTicks() != 0)
		{
			upateAfterBattle();
		}

		Trace::start("GameState::update::cities");
		current_city->update(*this, ticks);
		Trace::end("GameState::update::cities");

		Trace::start("GameState::update::organisations");
		for (auto &o : this->organisations)
		{
			o.second->updateMissions(*this);
		}
		Trace::end("GameState::update::organisations");

		Trace::start("GameState::update::vehicles");
		for (auto &v : this->vehicles)
		{
			if (v.second->city == current_city)
			{
				v.second->update(*this, ticks);
			}
		}
		Trace::end("GameState::update::vehicles");

		Trace::start("GameState::update::agents");
		for (auto &a : this->agents)
		{
			if (a.second->city == current_city)
			{
				a.second->update(*this, ticks);
			}
		}
		Trace::end("GameState::update::agents");

		gameTime.addTicks(ticks);
		if (gameTime.secondPassed())
		{
			this->updateEndOfSecond();
		}
		if (gameTime.fiveMinutesPassed())
		{
			this->updateEndOfFiveMinutes();
		}
		if (gameTime.hourPassed())
		{
			this->updateEndOfHour();
		}
		if (gameTime.dayPassed())
		{
			this->updateEndOfDay();
		}
		if (gameTime.weekPassed())
		{
			this->updateEndOfWeek();
		}
		gameTime.clearFlags();
	}
}

void GameState::updateEndOfSecond()
{
	Trace::start("GameState::updateEachSecond::buildings");
	for (auto &b : current_city->buildings)
	{
		b.second->updateCargo(*this);
	}
	Trace::end("GameState::updateEachSecond::buildings");
	Trace::start("GameState::updateEachSecond::vehicles");
	for (auto &v : vehicles)
	{
		if (v.second->city == current_city)
		{
			v.second->updateEachSecond(*this);
		}
	}
	Trace::end("GameState::updateEachSecond::vehicles");
	Trace::start("GameState::updateEachSecond::agents");
	for (auto &a : this->agents)
	{
		if (a.second->city == current_city)
		{
			a.second->updateEachSecond(*this);
		}
	}
	Trace::end("GameState::updateEachSecond::agents");
}

void GameState::updateEndOfFiveMinutes()
{
	// TakeOver calculation stops when org is taken over
	Trace::start("GameState::updateEndOfFiveMinutes::organisations");
	for (auto &o : this->organisations)
	{
		if (o.second->takenOver)
		{
			continue;
		}
		o.second->updateTakeOver(*this, TICKS_PER_MINUTE * 5);
		if (o.second->takenOver)
		{
			break;
		}
	}
	Trace::end("GameState::updateEndOfFiveMinutes::organisations");

	// Detection calculation stops when detection happens
	Trace::start("GameState::updateEndOfFiveMinutes::buildings");
	for (auto &b : current_city->buildings)
	{
		bool detected = b.second->ticksDetectionTimeOut > 0;
		b.second->updateDetection(*this, TICKS_PER_MINUTE * 5);
		if (b.second->ticksDetectionTimeOut > 0 && !detected)
		{
			break;
		}
	}
	for (auto &b : current_city->buildings)
	{
		b.second->updateCargo(*this);
	}
	Trace::end("GameState::updateEndOfFiveMinutes::buildings");
}

void GameState::updateEndOfHour()
{
	Trace::start("GameState::updateEndOfHour::labs");
	for (auto &lab : this->research.labs)
	{
		Lab::update(TICKS_PER_HOUR, {this, lab.second}, shared_from_this());
	}
	Trace::end("GameState::updateEndOfHour::labs");
	Trace::start("GameState::updateEndOfHour::cities");
	for (auto &c : this->cities)
	{
		c.second->hourlyLoop(*this);
	}
	Trace::end("GameState::updateEndOfHour::cities");
	Trace::start("GameState::updateEndOfHour::organisations");
	for (auto &o : this->organisations)
	{
		o.second->updateInfiltration(*this);
	}
	Trace::end("GameState::updateEndOfHour::organisations");
}

void GameState::updateEndOfDay()
{
	for (auto &b : this->player_bases)
	{
		for (auto &f : b.second->facilities)
		{
			if (f->buildTime > 0)
			{
				f->buildTime--;
				if (f->buildTime == 0)
				{
					fw().pushEvent(
					    new GameFacilityEvent(GameEventType::FacilityCompleted, b.second, f));
				}
			}
		}
	}
	Trace::start("GameState::updateEndOfDay::organisations");
	for (auto &o : this->organisations)
	{
		o.second->updateVehicleAgentPark(*this);
		o.second->updateHirableAgents(*this);
	}
	Trace::end("GameState::updateEndOfDay::organisations");
	Trace::start("GameState::updateEndOfDay::agents");
	for (auto &a : this->agents)
	{
		a.second->updateDaily(*this);
	}
	Trace::end("GameState::updateEndOfDay::agents");
	Trace::start("GameState::updateEndOfDay::cities");
	for (auto &c : this->cities)
	{
		c.second->dailyLoop(*this);
	}
	Trace::end("GameState::updateEndOfDay::cities");

	// SPAWN ALIENS
	return;
	for (int i = 0; i < 5; i++)
	{
		StateRef<City> city = {this, "CITYMAP_HUMAN"};

		auto portal = city->portals.begin();
		std::uniform_int_distribution<int> portal_rng(0, city->portals.size() - 1);
		std::advance(portal, portal_rng(this->rng));

		auto bld_iter = city->buildings.begin();
		std::uniform_int_distribution<int> bld_rng(0, city->buildings.size() - 1);
		std::advance(bld_iter, bld_rng(this->rng));
		StateRef<Building> bld = {this, (*bld_iter).second};

		auto vehicleType = this->vehicle_types.find("VEHICLETYPE_ALIEN_ASSAULT_SHIP");
		if (vehicleType != this->vehicle_types.end())
		{
			auto &type = (*vehicleType).second;

			auto v = city->placeVehicle(*this, {this, (*vehicleType).first}, type->manufacturer,
			                            (*portal)->getPosition());
			v->city = city;
			v->missions.emplace_back(VehicleMission::infiltrateOrSubvertBuilding(*this, *v, bld));
			v->missions.front()->start(*this, *v);
			fw().soundBackend->playSample(city_common_sample_list->dimensionShiftOut, v->position);

			fw().pushEvent(new GameVehicleEvent(GameEventType::UfoSpotted, {this, v}));
		}
	}
}

void GameState::updateEndOfWeek()
{
	int week = this->gameTime.getWeek();
	auto growth = this->ufo_growth_lists.find(format("%s%d", UFOGrowth::getPrefix(), week));
	if (growth == this->ufo_growth_lists.end())
	{
		growth = this->ufo_growth_lists.find(format("%s%s", UFOGrowth::getPrefix(), "DEFAULT"));
	}

	if (growth != this->ufo_growth_lists.end())
	{
		StateRef<City> city = {this, "CITYMAP_ALIEN"};
		StateRef<Organisation> alienOrg = {this, "ORG_ALIEN"};
		std::uniform_int_distribution<int> xyPos(20, 120);

		for (auto &vehicleEntry : growth->second->vehicleTypeList)
		{
			auto vehicleType = this->vehicle_types.find(vehicleEntry.first);
			if (vehicleType != this->vehicle_types.end())
			{
				for (int i = 0; i < vehicleEntry.second; i++)
				{
					auto &type = (*vehicleType).second;

					auto v = city->placeVehicle(*this, {this, (*vehicleType).first}, alienOrg,
					                            {xyPos(rng), xyPos(rng), city->map->size.z - 1});
				}
			}
		}
	}
}

void GameState::updateTurbo()
{
	if (!this->canTurbo())
	{
		LogError("Called when canTurbo() is false");
	}
	unsigned ticksToUpdate = TURBO_TICKS;
	// Turbo always re-aligns to TURBO_TICKS (5 minutes)
	unsigned int align = this->gameTime.getTicks() % TURBO_TICKS;
	if (align != 0)
	{
		ticksToUpdate -= align;
	}
	this->update(ticksToUpdate);
	this->updateAfterTurbo();
}

void GameState::updateAfterTurbo()
{
	Trace::start("GameState::updateAfterTurbo::vehicles");
	for (auto &v : this->vehicles)
	{
		if (player->isRelatedTo(v.second->owner) == Organisation::Relation::Hostile)
		{
			continue;
		}
		if (v.second->city != current_city)
		{
			continue;
		}
		v.second->update(*this, randBoundsExclusive(rng, (unsigned)0, 20 * TICKS_PER_SECOND));
	}
	Trace::end("GameState::updateAfterTurbo::vehicles");
}

void GameState::updateBeforeBattle()
{
	// Save time to roll back to
	gameTimeBeforeBattle = GameTime(gameTime.getTicks());
	// Some useless event just to know if something was reported
	eventFromBattle = GameEventType::WeeklyReport;
	missionLocationBattle = current_battle->mission_location_id;
}

void GameState::upateAfterBattle()
{
	gameTime = GameTime(gameTimeBeforeBattle.getTicks());
	gameTimeBeforeBattle = GameTime(0);

	switch (eventFromBattle)
	{
		case GameEventType::MissionCompletedBuildingAlien:
		{
			fw().pushEvent(new GameEvent(eventFromBattle));
			break;
		}
		case GameEventType::MissionCompletedBuildingNormal:
		{
			fw().pushEvent(new GameBuildingEvent(eventFromBattle, {this, missionLocationBattle}));
			break;
		}
		case GameEventType::MissionCompletedBase:
		{
			fw().pushEvent(new GameBaseEvent(
			    eventFromBattle, StateRef<Building>(this, missionLocationBattle)->base));
			break;
		}
		case GameEventType::BaseDestroyed:
		{
			auto building = StateRef<Building>{this, missionLocationBattle};
			fw().pushEvent(new GameSomethingDiedEvent(eventFromBattle, eventFromBattleText,
			                                          "bySomeone", building->crewQuarters));
			break;
		}
		case GameEventType::MissionCompletedBuildingRaid:
		{
			fw().pushEvent(new GameBuildingEvent(eventFromBattle, {this, missionLocationBattle}));
			break;
		}
		case GameEventType::MissionCompletedVehicle:
		{
			fw().pushEvent(new GameEvent(eventFromBattle));
			break;
		}
	}
}

void GameState::logEvent(GameEvent *ev)
{
	if (messages.size() == MAX_MESSAGES)
	{
		messages.pop_front();
	}
	Vec3<int> location = EventMessage::NO_LOCATION;
	if (GameVehicleEvent *gve = dynamic_cast<GameVehicleEvent *>(ev))
	{
		location = gve->vehicle->position;
	}
	else if (GameBuildingEvent *gve = dynamic_cast<GameBuildingEvent *>(ev))
	{
		location = {gve->building->bounds.p0.x, gve->building->bounds.p0.y, 0};
	}
	else if (GameAgentEvent *gae = dynamic_cast<GameAgentEvent *>(ev))
	{
		if (gae->agent->unit)
		{
			location = gae->agent->unit->position;
		}
		else
		{
			location = gae->agent->position;
		}
	}
	else if (GameBaseEvent *gbe = dynamic_cast<GameBaseEvent *>(ev))
	{
		location =
		    Vec3<int>(gbe->base->building->bounds.p0.x + gbe->base->building->bounds.p1.x,
		              gbe->base->building->bounds.p0.y + gbe->base->building->bounds.p1.y, 0) /
		    2;
	}
	// TODO: Other event types
	messages.emplace_back(EventMessage{gameTime, ev->message(), location});
}

uint64_t getNextObjectID(GameState &state, const UString &objectPrefix)
{
	std::lock_guard<std::mutex> l(state.objectIdCountLock);
	return state.objectIdCount[objectPrefix]++;
}

}; // namespace OpenApoc
