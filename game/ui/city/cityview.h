#pragma once

#include "game/state/stateobject.h"
#include "game/ui/tileview/citytileview.h"
#include "library/sp.h"
#include <map>
#include <vector>

namespace OpenApoc
{

class Form;
class GameState;
class GraphicButton;
class Control;
class Vehicle;
class Sample;
class Base;
class Building;
class Projectile;
class Organisation;
class VehicleTileInfo;
class Agent;
class UfopaediaEntry;
class AgentInfo;

enum class CityUpdateSpeed
{
	Pause,
	Speed1,
	Speed2,
	Speed3,
	Speed4,
	Speed5,
};

enum class CitySelectionState
{
	Normal,
	GotoBuilding,
	GotoLocation,
	AttackVehicle,
	AttackBuilding,
	ManualControl
};

class CityView : public CityTileView
{
  private:
	sp<Form> activeTab, baseForm;
	std::vector<sp<Form>> uiTabs;
	sp<Form> overlayTab;
	std::vector<sp<GraphicButton>> miniViews;
	CityUpdateSpeed updateSpeed;
	CityUpdateSpeed lastSpeed;

	sp<GameState> state;

	std::map<sp<Vehicle>, std::pair<VehicleTileInfo, sp<Control>>> vehicleListControls;
	std::map<sp<Agent>, std::pair<AgentInfo, sp<Control>>> agentListControls;

	bool followVehicle;

	void updateSelectedUnits();

	CitySelectionState selectionState;
	bool modifierLShift = false;
	bool modifierRShift = false;
	bool modifierLAlt = false;
	bool modifierRAlt = false;
	bool modifierLCtrl = false;
	bool modifierRCtrl = false;

	bool vanillaControls = false;

	sp<Palette> day_palette;
	sp<Palette> twilight_palette;
	sp<Palette> night_palette;

	bool colorForward = true;
	int colorCurrent = 0;

	std::vector<sp<Palette>> mod_day_palette;
	std::vector<sp<Palette>> mod_twilight_palette;
	std::vector<sp<Palette>> mod_night_palette;

	bool drawCity = true;
	sp<Surface> surface;

	// Click handlers

	bool handleClickedBuilding(StateRef<Building> building, bool rightClick,
	                           CitySelectionState selState);
	bool handleClickedVehicle(StateRef<Vehicle> vehicle, bool rightClick,
	                          CitySelectionState selState);
	bool handleClickedAgent(StateRef<Agent> agent, bool rightClick, CitySelectionState selState);
	bool handleClickedProjectile(sp<Projectile> projectile, bool rightClick,
	                             CitySelectionState selState);

	void tryOpenUfopaediaEntry(StateRef<UfopaediaEntry> ufopaediaEntry);

	// Orders

	void orderGoToBase();
	void orderMove(Vec3<float> position, bool alternative);
	void orderMove(StateRef<Building> building, bool alternative);
	void orderSelect(StateRef<Vehicle> vehicle, bool inverse, bool additive);
	void orderSelect(StateRef<Agent> agent, bool inverse, bool additive);
	void orderFire(Vec3<float> position);
	void orderAttack(StateRef<Vehicle> vehicle, bool forced);
	void orderFollow(StateRef<Vehicle> vehicle);
	void orderAttack(StateRef<Building> building);

  public:
	CityView(sp<GameState> state);
	~CityView() override;

	void initiateUfoMission(StateRef<Vehicle> ufo, StateRef<Vehicle> playerCraft);

	void begin() override;
	void resume() override;
	void update() override;
	void render() override;
	void eventOccurred(Event *e) override;
	bool handleKeyDown(Event *e);
	bool handleKeyUp(Event *e);
	bool handleMouseDown(Event *e);
	bool handleGameStateEvent(Event *e);

	void setUpdateSpeed(CityUpdateSpeed updateSpeed);
	void zoomLastEvent();
	void setSelectionState(CitySelectionState selectionState);
};

}; // namespace OpenApoc
