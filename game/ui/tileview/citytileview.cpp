#include "game/ui/tileview/citytileview.h"
#include "framework/data.h"
#include "framework/event.h"
#include "framework/framework.h"
#include "framework/image.h"
#include "framework/keycodes.h"
#include "framework/renderer.h"
#include "framework/trace.h"
#include "game/state/agent.h"
#include "game/state/city/agentmission.h"
#include "game/state/city/building.h"
#include "game/state/city/city.h"
#include "game/state/city/citycommonimagelist.h"
#include "game/state/city/scenery.h"
#include "game/state/city/vehicle.h"
#include "game/state/city/vehiclemission.h"
#include "game/state/gamestate.h"
#include "game/state/organisation.h"
#include "game/state/rules/scenery_tile_type.h"
#include "game/state/rules/vehicle_type.h"
#include "game/state/tileview/tileobject_scenery.h"
#include "game/state/tileview/tileobject_vehicle.h"

namespace OpenApoc
{
CityTileView::CityTileView(TileMap &map, Vec3<int> isoTileSize, Vec2<int> stratTileSize,
                           TileViewMode initialMode, Vec3<float> screenCenterTile,
                           GameState &gameState)
    : TileView(map, isoTileSize, stratTileSize, initialMode), state(gameState)
{
	selectedTileImageBack = fw().data->loadImage("city/selected-citytile-back.png");
	selectedTileImageFront = fw().data->loadImage("city/selected-citytile-front.png");
	selectedTileImageOffset = {32, 16};
	pal = fw().data->loadPalette("xcom3/ufodata/pal_01.dat");
	alertImage = fw().data->loadImage("city/building-circle-red.png");
	cargoImage = fw().data->loadImage("city/building-circle-yellow.png");

	selectionBrackets.resize(3);
	selectionBrackets[0].push_back(fw().data->loadImage("city/vehicle-brackets-f0.png"));
	selectionBrackets[0].push_back(fw().data->loadImage("city/vehicle-brackets-f1.png"));
	selectionBrackets[0].push_back(fw().data->loadImage("city/vehicle-brackets-f2.png"));
	selectionBrackets[0].push_back(fw().data->loadImage("city/vehicle-brackets-f3.png"));
	selectionBrackets[1].push_back(fw().data->loadImage("city/vehicle-brackets-s0.png"));
	selectionBrackets[1].push_back(fw().data->loadImage("city/vehicle-brackets-s1.png"));
	selectionBrackets[1].push_back(fw().data->loadImage("city/vehicle-brackets-s2.png"));
	selectionBrackets[1].push_back(fw().data->loadImage("city/vehicle-brackets-s3.png"));
	selectionBrackets[2].push_back(fw().data->loadImage("city/vehicle-brackets-h0.png"));
	selectionBrackets[2].push_back(fw().data->loadImage("city/vehicle-brackets-h1.png"));
	selectionBrackets[2].push_back(fw().data->loadImage("city/vehicle-brackets-h2.png"));
	selectionBrackets[2].push_back(fw().data->loadImage("city/vehicle-brackets-h3.png"));

	selectionImageFriendlySmall = fw().data->loadImage("battle/map-selection-small.png");
	selectionImageFriendlyLarge = fw().data->loadImage("battle/map-selection-large.png");
	selectionImageHostileSmall = fw().data->loadImage("city/map-selection-hostile-small.png");
	selectionImageHostileLarge = fw().data->loadImage("city/map-selection-hostile-large.png");

	targetTacticalThisLevel = fw().data->loadImage("city/target.png");

	// FIXME: Load from save last screen location?
	setScreenCenterTile(screenCenterTile);
};

CityTileView::~CityTileView() = default;

void CityTileView::eventOccurred(Event *e)
{
	if (e->type() == EVENT_KEY_DOWN)
	{
		switch (e->keyboard().KeyCode)
		{
			case SDLK_F3:
			{
				DEBUG_SHOW_WALK_TYPE++;
				DEBUG_SHOW_WALK_TYPE = DEBUG_SHOW_WALK_TYPE % 6;
				if (DEBUG_SHOW_WALK_TYPE)
				{
					DEBUG_SHOW_SLOPES = false;
					DEBUG_SHOW_ALIEN_CREW = false;
					DEBUG_SHOW_TUBE = false;
					DEBUG_SHOW_ROADS = false;
				}
				LogWarning("Debug walk type display set to %s", DEBUG_SHOW_WALK_TYPE);
				return;
			}
			case SDLK_F5:
			{
				DEBUG_SHOW_VEHICLE_PATH = !DEBUG_SHOW_VEHICLE_PATH;
				return;
			}
			case SDLK_F4:
			{
				DEBUG_SHOW_ALIEN_CREW = !DEBUG_SHOW_ALIEN_CREW;
				if (DEBUG_SHOW_ALIEN_CREW)
				{
					DEBUG_SHOW_ROADS = false;
					DEBUG_SHOW_SLOPES = false;
					DEBUG_SHOW_TUBE = false;
				}
				LogWarning("Debug Alien display set to %s", DEBUG_SHOW_ALIEN_CREW);
				return;
			}
			case SDLK_F12:
			{
				DEBUG_SHOW_SLOPES = !DEBUG_SHOW_SLOPES;
				if (DEBUG_SHOW_SLOPES)
				{
					DEBUG_SHOW_ALIEN_CREW = false;
					DEBUG_SHOW_TUBE = false;
					DEBUG_SHOW_ROADS = false;
				}
				LogWarning("Debug slopes display set to %s", DEBUG_SHOW_SLOPES);
				return;
			}
			case SDLK_F11:
			{
				DEBUG_SHOW_ROADS = !DEBUG_SHOW_ROADS;
				if (DEBUG_SHOW_ROADS)
				{
					DEBUG_SHOW_ALIEN_CREW = false;
					DEBUG_SHOW_TUBE = false;
					DEBUG_SHOW_SLOPES = false;
				}
				LogWarning("Debug roads display set to %s", DEBUG_SHOW_ROADS);
				return;
			}
			case SDLK_F10:
			{
				DEBUG_SHOW_TUBE = !DEBUG_SHOW_TUBE;
				if (DEBUG_SHOW_TUBE)
				{
					DEBUG_SHOW_ROADS = false;
					DEBUG_SHOW_SLOPES = false;
					DEBUG_SHOW_ALIEN_CREW = false;
				}
				LogWarning("Debug tube display set to %s", DEBUG_SHOW_TUBE);
				return;
			}
			case SDLK_KP_0:
				DEBUG_DIRECTION = -1;
				return;
			case SDLK_KP_5:
				DEBUG_ONLY_TYPE = !DEBUG_ONLY_TYPE;
				return;
			case SDLK_KP_9:
				DEBUG_DIRECTION = 0;
				return;
			case SDLK_KP_3:
				DEBUG_DIRECTION = 1;
				return;
			case SDLK_KP_1:
				DEBUG_DIRECTION = 2;
				return;
			case SDLK_KP_7:
				DEBUG_DIRECTION = 3;
				return;
			case SDLK_KP_8:
				DEBUG_DIRECTION = 4;
				return;
			case SDLK_KP_2:
				DEBUG_DIRECTION = 5;
				return;
			case SDLK_F6:
			{
				LogWarning("Writing voxel view LOF to tileviewvoxels.png");
				auto imageOffset = -this->getScreenOffset();
				auto img = std::dynamic_pointer_cast<RGBImage>(
				    this->map.dumpVoxelView({imageOffset, imageOffset + dpySize}, *this, 12.99f));
				fw().data->writeImage("tileviewvoxels.png", img);
				return;
			}
			case SDLK_F7:
			{
				LogWarning("Writing voxel view LOF (fast) to tileviewvoxels.png");
				auto imageOffset = -this->getScreenOffset();
				auto img = std::dynamic_pointer_cast<RGBImage>(this->map.dumpVoxelView(
				    {imageOffset, imageOffset + dpySize}, *this, 12.99f, true));
				fw().data->writeImage("tileviewvoxels.png", img);
				return;
			}
			case SDLK_F8:
			{
				LogWarning("Writing voxel view LOS to tileviewvoxels.png");
				auto imageOffset = -this->getScreenOffset();
				auto img = std::dynamic_pointer_cast<RGBImage>(this->map.dumpVoxelView(
				    {imageOffset, imageOffset + dpySize}, *this, 12.99f, false, true));
				fw().data->writeImage("tileviewvoxels.png", img);
				return;
			}
			case SDLK_F9:
			{
				LogWarning("Writing voxel view LOS (fast) to tileviewvoxels.png");
				auto imageOffset = -this->getScreenOffset();
				auto img = std::dynamic_pointer_cast<RGBImage>(this->map.dumpVoxelView(
				    {imageOffset, imageOffset + dpySize}, *this, 11.0f, true, true));
				fw().data->writeImage("tileviewvoxels.png", img);
				return;
			}
		}
	}
	TileView::eventOccurred(e);
}

void CityTileView::render()
{
	TRACE_FN;
	Renderer &r = *fw().renderer;
	r.clear();
	r.setPalette(this->pal);

	// Rotate Icons
	{
		selectionFrameTicksAccumulated++;
		selectionFrameTicksAccumulated %= 2 * SELECTION_FRAME_ANIMATION_DELAY;
	}

	// screenOffset.x/screenOffset.y is the 'amount added to the tile coords' - so we want
	// the inverse to tell which tiles are at the screen bounds
	auto topLeft = offsetScreenToTileCoords(Vec2<int>{-isoTileSize.x, -isoTileSize.y}, 0);
	auto topRight = offsetScreenToTileCoords(Vec2<int>{dpySize.x, -isoTileSize.y}, 0);
	auto bottomLeft = offsetScreenToTileCoords(Vec2<int>{-isoTileSize.x, dpySize.y}, map.size.z);
	auto bottomRight = offsetScreenToTileCoords(Vec2<int>{dpySize.x, dpySize.y}, map.size.z);

	int minX = std::max(0, topLeft.x);
	int maxX = std::min(map.size.x, bottomRight.x);

	int minY = std::max(0, topRight.y);
	int maxY = std::min(map.size.y, bottomLeft.y);

	switch (this->viewMode)
	{
		case TileViewMode::Isometric:
		{
			// List of vehicles that require drawing of brackets
			std::set<sp<Vehicle>> vehiclesToDrawBrackets;
			std::map<sp<Vehicle>, int> vehiclesBracketsIndex;

			// Go through every selected vehicle and add target to list of bracket draws
			for (auto &vehicle : state.current_city->cityViewSelectedVehicles)
			{
				if (vehicle->owner != state.getPlayer())
				{
					continue;
				}
				for (auto &m : vehicle->missions)
				{
					if (m->type == VehicleMission::MissionType::AttackVehicle ||
					    m->type == VehicleMission::MissionType::RecoverVehicle)
					{
						if (m->targetVehicle)
						{
							vehiclesToDrawBrackets.insert(m->targetVehicle);
							vehiclesBracketsIndex[m->targetVehicle] = 2;
						}
					}
				}
			}

			for (int z = 0; z < maxZDraw; z++)
			{
				for (unsigned int layer = 0; layer < map.getLayerCount(); layer++)
				{
					for (int y = minY; y < maxY; y++)
					{
						for (int x = minX; x < maxX; x++)
						{
							auto tile = map.getTile(x, y, z);
							auto object_count = tile->drawnObjects[layer].size();
							for (size_t obj_id = 0; obj_id < object_count; obj_id++)
							{
								auto &obj = tile->drawnObjects[layer][obj_id];
								Vec2<float> pos = tileToOffsetScreenCoords(obj->getCenter());
								bool visible = true;

								switch (obj->getType())
								{
									case TileObject::Type::Vehicle:
									{
										auto v = std::static_pointer_cast<TileObjectVehicle>(obj)
										             ->getVehicle();

										if (!state.current_city->cityViewSelectedVehicles.empty())
										{
											auto selectedPos = std::find(
											    state.current_city->cityViewSelectedVehicles
											        .begin(),
											    state.current_city->cityViewSelectedVehicles.end(),
											    v);

											if (selectedPos ==
											    state.current_city->cityViewSelectedVehicles
											        .begin())
											{
												if (v->owner == state.getPlayer())
												{
													vehiclesToDrawBrackets.insert(v);
													vehiclesBracketsIndex[v] = 0;
												}
											}
											else if (selectedPos !=
											         state.current_city->cityViewSelectedVehicles
											             .end())
											{
												vehiclesToDrawBrackets.insert(v);
												vehiclesBracketsIndex[v] = 1;
											}
										}
									}
									break;
									case TileObject::Type::Scenery:
									{
										auto s = std::static_pointer_cast<TileObjectScenery>(obj)
										             ->getOwner();
										if (DEBUG_SHOW_WALK_TYPE)
										{
											switch (DEBUG_SHOW_WALK_TYPE)
											{
												case 5:
													visible = s->type->basement;
													break;
												case 4:
													visible = s->willCollapse() || s->falling;
													break;
												case 3:
												case 2:
												case 1:
												case 0:
													visible = (int)s->type->walk_mode ==
													          DEBUG_SHOW_WALK_TYPE - 1;
													break;
												default:
													LogError("Unhandled DEBUG_SHOW_WALK_TYPE %d",
													         DEBUG_SHOW_WALK_TYPE);
													DEBUG_SHOW_WALK_TYPE = 0;
													break;
											}
										}
										if (DEBUG_SHOW_SLOPES)
										{
											if (DEBUG_DIRECTION == -1)
											{
												visible = s->type->hill[0] || s->type->hill[1] ||
												          s->type->hill[2] || s->type->hill[3];
											}
											else
											{
												visible = s->type->hill[DEBUG_DIRECTION];
											}
											visible =
											    visible && (!DEBUG_ONLY_TYPE ||
											                s->type->tile_type ==
											                    SceneryTileType::TileType::Road);
										}
										if (DEBUG_SHOW_ROADS)
										{
											if (DEBUG_DIRECTION == -1)
											{
												visible = s->type->connection[0] ||
												          s->type->connection[1] ||
												          s->type->connection[2] ||
												          s->type->connection[3];
											}
											else
											{
												visible = s->type->connection[DEBUG_DIRECTION];
											}
											visible =
											    visible && (!DEBUG_ONLY_TYPE ||
											                s->type->tile_type ==
											                    SceneryTileType::TileType::Road);
										}
										if (DEBUG_SHOW_TUBE)
										{
											if (DEBUG_DIRECTION == -1)
											{
												visible = s->type->tube[0] || s->type->tube[1] ||
												          s->type->tube[2] || s->type->tube[3] ||
												          s->type->tube[4] || s->type->tube[5];
											}
											else
											{
												visible = s->type->tube[DEBUG_DIRECTION];
											}
											visible = visible || (!DEBUG_ONLY_TYPE && s->building);
										}
									}
									default:
										break;
								}

								obj->draw(r, *this, pos, this->viewMode, visible);
							}
							if (tile->pathfindingDebugFlag)
								r.draw(selectedTileImageFront,
								       tileToOffsetScreenCoords(Vec3<int>{x, y, z}) -
								           selectedTileImageOffset);
						}
					}
				}
			}

			// Draw agents
			for (auto &a : state.agents)
			{
				if (a.second->owner != state.getPlayer() || a.second->city != state.current_city ||
				    a.second->currentVehicle || a.second->isDead())
				{
					continue;
				}
				r.draw(state.city_common_image_list->agentIsometric,
				       (Vec2<int>)tileToOffsetScreenCoords(a.second->position) -
				           ((Vec2<int>)state.city_common_image_list->agentIsometric->size) / 2);
			}

			// Draw brackets
			for (auto &obj : vehiclesToDrawBrackets)
			{
				Vec3<float> size = obj->type->size.at(obj->type->getVoxelMapFacing(obj->facing));
				size /= 2;
				Vec2<float> pTop = tileToOffsetScreenCoords(obj->getPosition() +
				                                            Vec3<float>{-size.x, -size.y, size.z});
				Vec2<float> pLeft =
				    tileToOffsetScreenCoords(obj->getPosition() + Vec3<float>{-size.x, +size.y, 0});
				Vec2<float> pRight =
				    tileToOffsetScreenCoords(obj->getPosition() + Vec3<float>{size.x, -size.y, 0});
				Vec2<float> pBottom = tileToOffsetScreenCoords(
				    obj->getPosition() + Vec3<float>{size.x, size.y, -size.z});

				int idx = vehiclesBracketsIndex[obj];
				r.draw(selectionBrackets[idx][0], {pLeft.x - 2.0f, pTop.y - 2.0f});
				r.draw(selectionBrackets[idx][1], {pLeft.x - 2.0f, pBottom.y - 2.0f});
				r.draw(selectionBrackets[idx][2], {pRight.x - 2.0f, pTop.y - 2.0f});
				r.draw(selectionBrackets[idx][3], {pRight.x - 2.0f, pBottom.y - 2.0f});
			}
		}
		break;
		case TileViewMode::Strategy:
		{
			// Params are: friendly, hostile, selected (0 = not, 1 = small, 2 = large)
			std::list<std::tuple<sp<Vehicle>, bool, bool, int>> vehiclesToDraw;
			std::set<sp<Vehicle>> vehiclesUnderAttack;
			std::set<sp<Building>> buildingsSelected;
			// Lines to draw between unit and destination, bool is wether target x is drawn
			std::list<std::tuple<Vec3<float>, Vec3<float>, bool, bool>> targetLocationsToDraw;

			for (int z = 0; z < maxZDraw; z++)
			{
				for (unsigned int layer = 0; layer < map.getLayerCount(); layer++)
				{
					for (int y = minY; y < maxY; y++)
					{
						for (int x = minX; x < maxX; x++)
						{
							auto tile = map.getTile(x, y, z);
							auto object_count = tile->drawnObjects[layer].size();
							for (size_t obj_id = 0; obj_id < object_count; obj_id++)
							{
								bool friendly = false;
								bool hostile = false;
								auto &obj = tile->drawnObjects[layer][obj_id];
								Vec2<float> pos = tileToOffsetScreenCoords(obj->getCenter());

								switch (obj->getType())
								{
									case TileObject::Type::Vehicle:
									{
										auto v = std::static_pointer_cast<TileObjectVehicle>(obj)
										             ->getVehicle();
										friendly = v->owner == state.getPlayer();
										hostile = state.getPlayer()->isRelatedTo(v->owner) ==
										          Organisation::Relation::Hostile;
										bool selected =
										    std::find(
										        state.current_city->cityViewSelectedVehicles
										            .begin(),
										        state.current_city->cityViewSelectedVehicles.end(),
										        v) !=
										    state.current_city->cityViewSelectedVehicles.end();

										if (friendly)
										{
										}
										vehiclesToDraw.emplace_back(
										    v, friendly, hostile,
										    selected
										        ? (v->type->mapIconType ==
										                   VehicleType::MapIconType::LargeCircle
										               ? 2
										               : 1)
										        : 0);
										// Do not draw vehicle yet
										continue;
									}
									break;
									default:
										break;
								}

								obj->draw(r, *this, pos, this->viewMode, true, 0, friendly,
								          hostile);
							}
						}
					}
				}
			}

			// Compile list of agent destinations
			for (auto &a : state.agents)
			{
				if (a.second->owner != state.getPlayer() || a.second->city != state.current_city ||
				    a.second->currentVehicle || a.second->isDead())
				{
					continue;
				}
				bool selected =
				    std::find(state.current_city->cityViewSelectedAgents.begin(),
				              state.current_city->cityViewSelectedAgents.end(),
				              a.second) != state.current_city->cityViewSelectedAgents.end();

				for (auto &m : a.second->missions)
				{
					if (m->type == AgentMission::MissionType::GotoBuilding)
					{
						targetLocationsToDraw.emplace_back(m->targetBuilding->crewQuarters,
						                                   a.second->position, true, false);
						break;
					}
				}
			}
			// Compile list of vehicle destinations and add to draw for those that are in buildings
			for (auto &v : state.vehicles)
			{
				if (v.second->owner != state.getPlayer() || v.second->city != state.current_city)
				{
					continue;
				}
				bool selected =
				    std::find(state.current_city->cityViewSelectedVehicles.begin(),
				              state.current_city->cityViewSelectedVehicles.end(),
				              v.second) != state.current_city->cityViewSelectedVehicles.end();

				// Draw those in buildings
				if (v.second->currentBuilding)
				{
					vehiclesToDraw.emplace_back(
					    v.second, true, false,
					    selected
					        ? (v.second->type->mapIconType == VehicleType::MapIconType::LargeCircle
					               ? 2
					               : 1)
					        : 0);
				}

				// Destinations
				for (auto &m : v.second->missions)
				{
					switch (m->type)
					{
						case VehicleMission::MissionType::AttackVehicle:
						{
							vehiclesUnderAttack.insert(m->targetVehicle);
							targetLocationsToDraw.emplace_back(m->targetVehicle->position,
							                                   v.second->position, false, true);
							break;
						}
						case VehicleMission::MissionType::FollowVehicle:
						{
							targetLocationsToDraw.emplace_back(m->targetVehicle->position,
							                                   v.second->position, false, false);
							break;
						}
						case VehicleMission::MissionType::AttackBuilding:
						case VehicleMission::MissionType::GotoBuilding:
							buildingsSelected.insert(m->targetBuilding);
						// Intentional fall-through
						case VehicleMission::MissionType::Crash:
						{
							if (!m->currentPlannedPath.empty())
							{
								targetLocationsToDraw.emplace_back(
								    (Vec3<float>)m->currentPlannedPath.back() +
								        Vec3<float>{0.5f, 0.5f, 0.0f},
								    v.second->position, true, false);
							}
							break;
						}
						case VehicleMission::MissionType::GotoLocation:
						case VehicleMission::MissionType::InfiltrateSubvert:
						case VehicleMission::MissionType::Patrol:
						case VehicleMission::MissionType::GotoPortal:
						{
							targetLocationsToDraw.emplace_back((Vec3<float>)m->targetLocation +
							                                       Vec3<float>{0.5f, 0.5f, 0.0f},
							                                   v.second->position, true, false);
							break;
						}
					}
				}
			}

			// Draw lines to target
			for (auto &obj : targetLocationsToDraw)
			{
				static const auto offsetStrat = Vec2<float>{-4.0f, -4.0f};
				static const auto lineColorFriend = Colour(150, 250, 20, 255);
				static const auto lineColorEnemy = Colour(255, 0, 0, 255);

				bool targetDrawn = std::get<2>(obj);
				// Draw line from unit to target tile
				r.drawLine(tileToOffsetScreenCoords(std::get<0>(obj)),
				           tileToOffsetScreenCoords(std::get<1>(obj)),
				           std::get<3>(obj) ? lineColorEnemy : lineColorFriend);
				// Draw location image at target tile
				if (targetDrawn && selectionFrameTicksAccumulated / SELECTION_FRAME_ANIMATION_DELAY)
				{
					r.draw(targetTacticalThisLevel,
					       tileToOffsetScreenCoords(std::get<0>(obj)) + offsetStrat);
				}
			}
			// Draw agent icons
			for (auto &a : state.agents)
			{
				if (a.second->owner != state.getPlayer() || a.second->city != state.current_city ||
				    a.second->currentVehicle || a.second->isDead())
				{
					continue;
				}
				r.draw(state.city_common_image_list->agentStrategic,
				       tileToOffsetScreenCoords(a.second->position) - Vec2<float>{4, 4});
				// Draw unit selection brackets
				if (selectionFrameTicksAccumulated / SELECTION_FRAME_ANIMATION_DELAY)
				{
					bool selected =
					    !state.current_city->cityViewSelectedAgents.empty() &&
					    std::find(state.current_city->cityViewSelectedAgents.begin(),
					              state.current_city->cityViewSelectedAgents.end(),
					              a.second) != state.current_city->cityViewSelectedAgents.end();
					if (selected)
					{
						r.draw(selectionImageFriendlySmall,
						       tileToOffsetScreenCoords(a.second->position) -
						           Vec2<float>(selectionImageFriendlySmall->size / (unsigned)2));
					}
				}
			}
			// Draw vehicle icons
			for (auto &obj : vehiclesToDraw)
			{
				auto vehicle = std::get<0>(obj);
				Vec2<float> pos = tileToOffsetScreenCoords(vehicle->position);
				if (vehicle->tileObject)
				{
					vehicle->tileObject->draw(r, *this, pos, this->viewMode, true, 0,
					                          std::get<1>(obj), std::get<2>(obj));
				}
				else
				{
					TileObjectVehicle::drawStatic(r, vehicle, *this, pos, viewMode, true, 0,
					                              std::get<1>(obj), std::get<2>(obj));
				}
				// Draw unit selection brackets
				if (selectionFrameTicksAccumulated / SELECTION_FRAME_ANIMATION_DELAY)
				{
					auto selected = std::get<3>(obj);
					if (selected)
					{
						auto drawn = selected == 1 ? selectionImageFriendlySmall
						                           : selectionImageFriendlyLarge;
						r.draw(drawn, pos - Vec2<float>(drawn->size / (unsigned)2));
					}
					else
					{
						if (vehiclesUnderAttack.find(vehicle) != vehiclesUnderAttack.end())
						{
							auto drawn =
							    vehicle->type->mapIconType == VehicleType::MapIconType::LargeCircle
							        ? selectionImageHostileLarge
							        : selectionImageHostileSmall;
							r.draw(drawn, pos - Vec2<float>(drawn->size / (unsigned)2));
						}
					}
				}
			}

			// Alien debug display
			if (DEBUG_SHOW_ALIEN_CREW)
			{
				for (auto &b : state.current_city->buildings)
				{
					Vec2<float> pos = tileToOffsetScreenCoords(
					    Vec3<int>{b.second->bounds.p0.x, b.second->bounds.p0.y, 2});
					for (auto &a : b.second->current_crew)
					{
						for (int i = 0; i < a.second; i++)
						{
							auto icon = a.first->portraits.at(*a.first->possible_genders.begin())
							                .at(0)
							                .icon;
							r.draw(icon, pos);
							pos.x += icon->size.x / 2;
						}
					}
				}
			}

			// Building selection circles
			for (auto &b : state.current_city->buildings)
			{
				// Detection of aliens
				if (b.second->detected)
				{
					float initialRadius = std::max(alertImage->size.x, alertImage->size.y);
					// Eventually scale to 1/2 the size, but start with some bonus time of full
					// size,
					// so that it doesn't become distorted immediately, that's why we add extra 0.05
					float radius = std::min(initialRadius,
					                        initialRadius * (float)b.second->ticksDetectionTimeOut /
					                                (float)TICKS_DETECTION_TIMEOUT / 2.0f +
					                            0.55f);
					Vec2<float> pos = tileToOffsetScreenCoords(
					    Vec3<int>{(b.second->bounds.p0.x + b.second->bounds.p1.x) / 2,
					              (b.second->bounds.p0.y + b.second->bounds.p1.y) / 2, 2});
					pos -= Vec2<float>{radius / 2.0f, radius / 2.0f};

					if (radius == initialRadius)
					{
						r.draw(alertImage, pos);
					}
					else
					{
						r.drawScaled(alertImage, pos, {radius, radius});
					}
				}
				// Cargo for player
				bool hasCargo = false;
				uint64_t nearestExpiry = UINT64_MAX;
				for (auto &c : b.second->cargo)
				{
					if (c.count > 0 && c.destination->owner == state.getPlayer())
					{
						hasCargo = true;
						nearestExpiry = std::min(nearestExpiry, c.expirationDate);
					}
				}
				if (hasCargo)
				{
					nearestExpiry -= state.gameTime.getTicks();
					float initialRadius = std::max(cargoImage->size.x, cargoImage->size.y);
					// Eventually scale to 1/2 the size, but start with some bonus time of full
					// size,
					// so that it doesn't become distorted immediately, that's why we add extra 0.05
					float radius = std::min(initialRadius,
					                        initialRadius * (float)nearestExpiry /
					                                (float)TICKS_CARGO_TTL / 2.0f +
					                            0.55f);
					Vec2<float> pos = tileToOffsetScreenCoords(
					    Vec3<int>{(b.second->bounds.p0.x + b.second->bounds.p1.x) / 2,
					              (b.second->bounds.p0.y + b.second->bounds.p1.y) / 2, 2});
					pos -= Vec2<float>{radius / 2.0f, radius / 2.0f};

					if (radius == initialRadius)
					{
						r.draw(cargoImage, pos);
					}
					else
					{
						r.drawScaled(cargoImage, pos, {radius, radius});
					}
				}
			}

			// Building selection brackets
			for (auto &b : buildingsSelected)
			{
				std::vector<Vec3<int>> points = {
				    {b->bounds.p0.x, b->bounds.p0.y, 0}, {b->bounds.p0.x, b->bounds.p1.y, 0},
				    {b->bounds.p1.x, b->bounds.p1.y, 0}, {b->bounds.p1.x, b->bounds.p0.y, 0},
				    {b->bounds.p0.x, b->bounds.p0.y, 0},
				};
				static const auto lineColorBuilding = Colour(150, 210, 240, 255);
				for (int i = 0; i < points.size() - 1; i++)
				{
					r.drawLine(tileToOffsetScreenCoords(points[i]),
					           tileToOffsetScreenCoords(points[i + 1]), lineColorBuilding);
				}
			}

			renderStrategyOverlay(r);
		}
		break;
	}
}
}
