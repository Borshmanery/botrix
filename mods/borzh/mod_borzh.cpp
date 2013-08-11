#include "chat.h"
#include "event.h"
#include "mods/borzh/mod_borzh.h"
#include "players.h"
#include "source_engine.h"
#include "waypoint.h"


//----------------------------------------------------------------------------------------------------------------
/*const good::string CMod_Borzh::m_aChats[CHATS_COUNT] =
{
};*/

TChatVariable CMod_Borzh::iVarDoor;
TChatVariable CMod_Borzh::iVarDoorStatus;
TChatVariable CMod_Borzh::iVarButton;
TChatVariable CMod_Borzh::iVarWeapon;
TChatVariable CMod_Borzh::iVarArea;
TChatVariable CMod_Borzh::iVarPlayer;

TChatVariableValue CMod_Borzh::iVarValueDoorStatusOpened;
TChatVariableValue CMod_Borzh::iVarValueDoorStatusClosed;

TChatVariableValue CMod_Borzh::iVarValueWeaponPhyscannon;
TChatVariableValue CMod_Borzh::iVarValueWeaponCrossbow;

good::vector< good::vector<TWaypointId> > CMod_Borzh::m_aAreasWaypoints;       // Waypoints for areas.
good::vector< good::vector<TEntityIndex>  >CMod_Borzh::m_aAreasDoors;          // Doors for areas.
good::vector< good::vector<TEntityIndex> > CMod_Borzh::m_aAreasButtons;        // Buttons for areas.
good::vector< good::vector<TWaypointId> > CMod_Borzh::m_aShootButtonWaypoints; // Waypoints to shoot buttons.

//----------------------------------------------------------------------------------------------------------------
CMod_Borzh::CMod_Borzh()
{
	//CMod::AddEvent(new CPlayerActivateEvent()); // No need for this.
	CMod::AddEvent(new CPlayerTeamEvent());
	CMod::AddEvent(new CPlayerSpawnEvent());

	CMod::AddEvent(new CPlayerHurtEvent());
	CMod::AddEvent(new CPlayerDeathEvent());

	CMod::AddEvent(new CPlayerChatEvent());

	iVarDoor = CChat::AddVariable("$door");
	iVarDoorStatus = CChat::AddVariable("$door_status");
	iVarButton = CChat::AddVariable("$button");
	iVarWeapon = CChat::AddVariable("$weapon");
	iVarArea = CChat::AddVariable("$area");
	iVarPlayer = CChat::AddVariable("$player"); // TODO: move away.
}


//----------------------------------------------------------------------------------------------------------------
void CMod_Borzh::MapLoaded()
{
	// Add possible chat variable values for doors, buttons and weapons.
	iVarValueDoorStatusOpened = CChat::AddVariableValue(iVarDoorStatus, "opened");
	iVarValueDoorStatusClosed = CChat::AddVariableValue(iVarDoorStatus, "closed");

	iVarValueWeaponPhyscannon = CChat::AddVariableValue(iVarWeapon, "weapon_physcannon");
	iVarValueWeaponCrossbow = CChat::AddVariableValue(iVarWeapon, "weapon_crossbow");

	static char szInt[16];

	// Add possible doors values.
	const good::vector<CEntity>& aDoors = CItems::GetItems(EEntityTypeDoor);
	for ( TEntityIndex i=0; i < aDoors.size(); ++i )
	{
		sprintf(szInt, "%d", i+1);
		CChat::AddVariableValue( iVarDoor, good::string(szInt).duplicate() );
	}

	// Add possible buttons values.
	const good::vector<CEntity>& aButtons = CItems::GetItems(EEntityTypeButton);
	for ( TEntityIndex i=0; i < aButtons.size(); ++i )
	{
		sprintf(szInt, "%d", i+1);
		CChat::AddVariableValue( iVarButton, good::string(szInt).duplicate() );
	}

	// Add possible areas values.
	const StringVector& aAreas = CWaypoints::GetAreas();
	for ( TAreaId i=0; i < aAreas.size(); ++i )
		CChat::AddVariableValue( iVarArea, aAreas[i].duplicate() );

	// Add empty player names.
	for ( int i=0; i < CPlayers::Size(); ++i )
		CChat::AddVariableValue( iVarPlayer, "" );

	// Waypoints for areas.
	m_aAreasWaypoints.clear();
	m_aAreasWaypoints.resize( aAreas.size() );
	for ( TWaypointId iWaypoint = 0; iWaypoint < CWaypoints::Size(); ++iWaypoint )
		m_aAreasWaypoints[ CWaypoints::Get(iWaypoint).iAreaId ].push_back( iWaypoint );

	// Doors for areas.
	m_aAreasDoors.clear();
	m_aAreasDoors.resize( aAreas.size() );
	for ( TEntityIndex iDoor = 0; iDoor < aDoors.size(); ++iDoor )
	{
		const CEntity& cDoor = aDoors[iDoor];
		TWaypointId iDoorWaypoint1 = cDoor.iWaypoint;
		TWaypointId iDoorWaypoint2 = (TWaypointId)cDoor.pArguments;
		if ( iDoorWaypoint1 != EWaypointIdInvalid )
			m_aAreasDoors[ CWaypoints::Get(iDoorWaypoint1).iAreaId ].push_back( iDoor );
		if ( iDoorWaypoint2 != EWaypointIdInvalid )
			m_aAreasDoors[ CWaypoints::Get(iDoorWaypoint2).iAreaId ].push_back( iDoor );
	}

	// Buttons for areas.
	m_aAreasButtons.clear();
	m_aAreasButtons.resize( aAreas.size() );
	for ( TEntityIndex iButton = 0; iButton < aButtons.size(); ++iButton )
	{
		const CEntity& cButton = aButtons[iButton];
		if ( cButton.iWaypoint != EWaypointIdInvalid )
			m_aAreasButtons[ CWaypoints::Get(cButton.iWaypoint).iAreaId ].push_back( iButton );
	}

	// Shoot buttons waypoints.
	m_aShootButtonWaypoints.clear();
	m_aShootButtonWaypoints.resize( aButtons.size() );
	for ( TWaypointId iWaypoint = 0; iWaypoint < CWaypoints::Size(); ++iWaypoint )
	{
		const CWaypoint& cWaypoint = CWaypoints::Get(iWaypoint);
		if ( FLAG_SOME_SET(FWaypointSeeButton, cWaypoint.iFlags) )
			m_aShootButtonWaypoints[ CWaypoint::GetButton(cWaypoint.iArgument) - 1 ].push_back(iWaypoint);
	}
}

