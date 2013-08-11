#include "chat.h"
#include "clients.h"
#include "planner.h"
#include "type2string.h"

#include "mods/borzh/bot_borzh.h"
#include "mods/borzh/mod_borzh.h"


#define GET_TYPE(arg)                GET_1ST_BYTE(arg)
#define SET_TYPE(type, arg)          SET_1ST_BYTE(type, arg)

#define GET_INDEX(arg)               GET_2ND_BYTE(arg)
#define SET_INDEX(index, arg)        SET_2ND_BYTE(index, arg)

#define GET_AUX1(arg)                GET_3RD_BYTE(arg)
#define SET_AUX1(aux, arg)           SET_3RD_BYTE(aux, arg)

#define GET_AUX2(arg)                GET_4TH_BYTE(arg)
#define SET_AUX2(aux, arg)           SET_4TH_BYTE(aux, arg)


good::vector<TEntityIndex> CBot_BorzhMod::m_aLastPushedButtons;
CBot_BorzhMod::CBorzhTask CBot_BorzhMod::m_cCurrentProposedTask;

//----------------------------------------------------------------------------------------------------------------
CBot_BorzhMod::CBot_BorzhMod( edict_t* pEdict, TPlayerIndex iIndex, TBotIntelligence iIntelligence ):
	CBot(pEdict, iIndex, iIntelligence), m_bHasCrossbow(false), m_bHasPhyscannon(false), m_bStarted(false)
{
	CBotrixPlugin::pEngineServer->SetFakeClientConVarValue(pEdict, "cl_autowepswitch", "0");	
	CBotrixPlugin::pEngineServer->SetFakeClientConVarValue(pEdict, "cl_defaultweapon", "weapon_crowbar");	
}
		
//----------------------------------------------------------------------------------------------------------------
void CBot_BorzhMod::Activated()
{
	CBot::Activated();

	m_cAcceptedPlayers.resize( CPlayers::Size() );
	m_cAcceptedPlayers.reset();
	m_cCollaborativePlayers.resize( CPlayers::Size() );
	m_cCollaborativePlayers.reset();
	m_cCollaborativePlayers.set(m_iIndex); // Annotate self as collaborative player.
	m_cBusyPlayers.resize( CPlayers::Size() );
	m_cBusyPlayers.reset();
	m_cWaitingPlayers.resize( CPlayers::Size() );
	m_cWaitingPlayers.reset();

	m_aPlayersAreas.resize( m_cAcceptedPlayers.size() );
	m_cDesiredPlayersPositions.resize( m_cAcceptedPlayers.size() );
	memset(m_aPlayersAreas.data(), EAreaIdInvalid, m_aPlayersAreas.size() * sizeof(TAreaId));

	// Initialize doors and buttons.
	m_cSeenDoors.resize( CItems::GetItems(EEntityTypeDoor).size() );
	m_cSeenDoors.reset();

	m_cSeenButtons.resize( CItems::GetItems(EEntityTypeButton).size() );
	m_cSeenButtons.reset();

	m_cPushedButtons.resize( CItems::GetItems(EEntityTypeButton).size() );
	m_cPushedButtons.reset();

	m_cOpenedDoors.resize( m_cSeenDoors.size() );
	m_cOpenedDoors.reset();

	m_cFalseOpenedDoors.resize( m_cSeenDoors.size() );
	m_cFalseOpenedDoors.reset();

	m_cButtonTogglesDoor.resize( m_cSeenButtons.size()  );
	m_cButtonNoAffectDoor.resize( m_cSeenButtons.size() );
	m_cCheckedDoors.resize( m_cSeenButtons.size() );
	for ( int i=0; i < m_cButtonTogglesDoor.size(); ++i )
	{
		m_cButtonTogglesDoor[i].resize( m_cSeenDoors.size() );
		m_cButtonTogglesDoor[i].reset();

		m_cButtonNoAffectDoor[i].resize( m_cSeenDoors.size() );
		m_cButtonNoAffectDoor[i].reset();
	}

	m_aVisitedWaypoints.resize( CWaypoints::Size() );
	m_aVisitedWaypoints.reset();

	m_aVisitedAreas.resize( CWaypoints::GetAreas().size() );
	m_aVisitedAreas.reset();

	m_cVisitedAreasAfterPushButton.resize( m_aVisitedAreas.size() );

	m_cReachableAreas.resize( m_aVisitedAreas.size() );
}

//----------------------------------------------------------------------------------------------------------------
void CBot_BorzhMod::Respawned()
{
	CBot::Respawned();

	m_bDontAttack = m_bDomainChanged = true;
	m_cCurrentTask.iTask = EBorzhTaskInvalid;

	if ( iCurrentWaypoint != -1 )
		m_aVisitedWaypoints.set(iCurrentWaypoint);
	m_aPlayersAreas[m_iIndex] = CWaypoints::Get(iCurrentWaypoint).iAreaId;

	SetReachableAreas(m_aPlayersAreas[m_iIndex], m_cSeenDoors, m_cOpenedDoors, m_cReachableAreas);

	if ( !m_aVisitedAreas.test(m_aPlayersAreas[m_iIndex]) )
		PushSpeakTask(EBorzhChatNewArea, EEntityTypeInvalid, m_aPlayersAreas[m_iIndex]); // Say: i am in new area.
	else
		PushSpeakTask(EBorzhChatChangeArea, EEntityTypeInvalid, m_aPlayersAreas[m_iIndex]); // Say: i have changed area.

	if ( m_bFirstRespawn )
	{
		// Say hello to some other player.
		for ( TPlayerIndex iPlayer = 0; iPlayer < CPlayers::Size(); ++iPlayer )
		{
			CPlayer* pPlayer = CPlayers::Get(iPlayer);
			if ( pPlayer && (m_iIndex != iPlayer) && (pPlayer->GetTeam() == GetTeam()) )
				PushSpeakTask(EBotChatGreeting, EEntityTypeInvalid, iPlayer);
		}
	}
	m_bTaskFinished = true;
}

//----------------------------------------------------------------------------------------------------------------
void CBot_BorzhMod::HurtBy( int iPlayerIndex, CPlayer* pAttacker, int iHealthNow )
{
	m_cChat.iBotRequest = EBotChatDontHurt;
	m_cChat.iDirectedTo = iPlayerIndex;
	m_cChat.cMap.clear();

	CBot::Speak(false);
}

//----------------------------------------------------------------------------------------------------------------
void CBot_BorzhMod::ReceiveChat( int iPlayerIndex, CPlayer* pPlayer, bool bTeamOnly, const char* szText )
{
	if ( m_bStarted )
		return;

	good::string sText(szText, true, true);
	sText.lower_case();

	if ( sText == "start" )
		m_bStarted = true;
	else if ( sText == "stop" )
		m_bStarted = false;
	else if ( sText == "plan" )
	{
		if ( !CPlanner::IsRunning() )
			CPlanner::ExecutePlanner(*this, m_cDesiredPlayersPositions);
	}
}

//----------------------------------------------------------------------------------------------------------------
void CBot_BorzhMod::ReceiveChatRequest( const CBotChat& cRequest )
{
	// TODO: synchronize chat, receive in buffer at the Think().
	TChatVariableValue iVars[CMod_Borzh::iTotalVars];
	memset(&iVars, EChatVariableInvalid, CMod_Borzh::iTotalVars * sizeof(TChatVariableValue));

#define AREA        iVars[CMod_Borzh::iVarArea]
#define BUTTON      iVars[CMod_Borzh::iVarButton]
#define DOOR        iVars[CMod_Borzh::iVarDoor]
#define DOOR_STATUS iVars[CMod_Borzh::iVarDoorStatus]
#define WEAPON      iVars[CMod_Borzh::iVarWeapon]

	// Get needed chat variables.
	for ( int i=0; i < cRequest.cMap.size(); ++i )
	{
		const CChatVarValue& cVarValue = cRequest.cMap[i];
		iVars[cVarValue.iVar] = cVarValue.iValue;
	}

	// Respond to a request/chat. TODO: check arguments.
	switch (cRequest.iBotRequest)
	{
		case EBotChatAffirmative:
		case EBotChatAffirm:
		case EBorzhChatOk:
			m_cAcceptedPlayers.set(cRequest.iSpeaker);
			if ( m_cCurrentTask.iTask == EBorzhTaskWaitAnswer )
				CheckRemainingPlayersForBigTask();
			break;

		case EBorzhChatNoMoves:
		case EBorzhChatDone:
		case EBorzhChatFinishExplore:
			m_cBusyPlayers.reset(cRequest.iSpeaker);
			m_bAllPlayersIdle = !m_cBusyPlayers.any();
			if ( m_bAllPlayersIdle && (m_cCurrentTask.iTask == EBorzhTaskWaitAnswer) )
			{
				m_bTaskFinished = true;
				OfferCurrentBigTask();
			}
			break;

		case EBotChatBusy:
		case EBorzhChatWait:
			m_cBusyPlayers.set(cRequest.iSpeaker);
			break;

		case EBotChatStop:
		case EBotChatNegative:
		case EBotChatNegate:
			//TODO: process negative answer.
			/*
			CancelTask();
			m_cCurrentTask.iTask = EBorzhTaskWaitPlayer;
			m_cCurrentTask.iArgument = cRequest.iSpeaker;
			m_fEndWaitTime = CBotrixPlugin::fTime + m_iTimeToWaitPlayer / 1000.0f;
			*/
			break;

		case EBorzhChatThink:
		case EBorzhChatExplore:
		case EBorzhChatExploreNew:
			m_cBusyPlayers.set(cRequest.iSpeaker);
			break;

		case EBorzhChatNewArea:
		case EBorzhChatChangeArea:
			m_aPlayersAreas[cRequest.iSpeaker] = AREA;
			break;

		case EBorzhChatWeaponFound:
			break;

		case EBorzhChatDoorFound:
			DebugAssert( (DOOR != EChatVariableValueInvalid) && (DOOR_STATUS != EChatVariableValueInvalid) );
			DebugAssert( !m_cSeenDoors.test(DOOR) );
			m_cSeenDoors.set(DOOR);
			m_cOpenedDoors.set(DOOR, DOOR_STATUS == CMod_Borzh::iVarValueDoorStatusOpened);
			if ( (rand() & 3) == 0 )
				SwitchToSpeakTask(EBorzhChatOk, EEntityTypeInvalid, EEntityIndexInvalid);
			break;

		case EBorzhChatDoorChange:
			DebugAssert( (DOOR != EChatVariableValueInvalid) && (DOOR_STATUS != EChatVariableValueInvalid) );
			DebugAssert( m_cSeenDoors.test(DOOR) );
			m_cOpenedDoors.set(DOOR, DOOR_STATUS == CMod_Borzh::iVarValueDoorStatusOpened);
			if ( (m_cCurrentBigTask.iTask == EBorzhTaskButtonTry) && (GET_TYPE(m_cCurrentBigTask.iArgument) != 0) ) // We already pushed button.
				SetButtonTogglesDoor(m_aLastPushedButtons.back(), DOOR, true);
			if ( (rand() & 3) == 0 )
				SwitchToSpeakTask(EBorzhChatOk, EEntityTypeInvalid, EEntityIndexInvalid);
			break;

		case EBorzhChatDoorNoChange:
			DebugAssert( DOOR != EChatVariableValueInvalid );
			DebugAssert( m_cSeenDoors.test(DOOR) );
			DebugAssert( m_cOpenedDoors.test(DOOR) == (DOOR_STATUS == CMod_Borzh::iVarValueDoorStatusOpened) );
			if ( (m_cCurrentBigTask.iTask == EBorzhTaskButtonTry) && (GET_TYPE(m_cCurrentBigTask.iArgument) != 0) ) // We already pushed button.
				SetButtonTogglesDoor(m_aLastPushedButtons.back(), DOOR, false);
			if ( (rand() & 3) == 0 )
				SwitchToSpeakTask(EBorzhChatOk, EEntityTypeInvalid, EEntityIndexInvalid);
			break;

		case EBorzhChatSeeButton:
			DebugAssert( BUTTON != EChatVariableValueInvalid );
			m_cSeenButtons.set(BUTTON);
			if ( (rand() & 3) == 0 )
				SwitchToSpeakTask(EBorzhChatOk, EEntityTypeInvalid, EEntityIndexInvalid);
			break;

		case EBorzhChatButtonCanPush:
		case EBorzhChatButtonCantPush:
		case EBorzhChatButtonWeapon:
		case EBorzhChatButtonNoWeapon:
		case EBorzhChatDoorTry:
			break;

		case EBorzhChatButtonTry:
			ReceiveTaskOffer(EBorzhTaskButtonTry, BUTTON, cRequest.iSpeaker);
			break;

		case EBorzhChatButtonIPush:
		case EBorzhChatButtonIShoot:
			if ( m_cCurrentTask.iTask == EBorzhTaskWaitButton )
			{
				DebugAssert( m_cCurrentTask.iArgument == BUTTON );
				m_cPushedButtons.set(BUTTON);
				m_cCheckedDoors.reset();
				SET_TYPE(true, m_cCurrentBigTask.iArgument); // Button is pushed now.
				m_cTaskStack.push_back( CBorzhTask(EBorzhTaskWait, m_iTimeAfterSpeak) ); // Wait time after speak, so another bot can press button.
				m_bTaskFinished = true;
			}
			else
			{
				if ( m_cCurrentBigTask.iTask == EBorzhTaskInvalid )
				{
					m_cCurrentBigTask.iTask = EBorzhTaskButtonTry;
					m_cCurrentBigTask.iArgument = 0;
					SET_INDEX(BUTTON, m_cCurrentBigTask.iArgument);
					m_cTaskStack.push_back( CBorzhTask(EBorzhTaskWait, m_iTimeAfterSpeak) ); // Wait time after speak, so another bot can press button.
					m_bNothingToDo = false; // Wake up bot, if it has nothing to do.
				}
				else
					SwitchToSpeakTask(EBorzhChatWait, EEntityTypeInvalid, EEntityIndexInvalid); // Tell player to wait, so bot can finish his business first.
			}
			break;

		case EBorzhChatButtonYouPush:
		case EBorzhChatButtonYouShoot:
			break;

		case EBorzhChatAreaGo:
		case EBorzhChatAreaCantGo:
		case EBorzhChatDoorGo:
		case EBorzhChatButtonGo:
		case EBorzhChatFoundPlan:
			break;
	}

	// Wait some time, emulating processing of message.
	Wait(m_iTimeAfterSpeak - 1000, true);
}

//----------------------------------------------------------------------------------------------------------------
void CBot_BorzhMod::Think()
{
	if ( !m_bAlive )
	{
		m_cCmd.buttons = rand() & IN_ATTACK; // Force bot to respawn by hitting randomly attack button.
		return;
	}

	if ( !m_bStarted || (m_bNothingToDo && !m_bDomainChanged) )
		return;

	// Check if there are some new tasks.
	if ( m_bTaskFinished )
		TaskPop();

	if ( !m_bNewTask && (m_cCurrentTask.iTask == EBorzhTaskInvalid) )
	{
		CheckBigTask();
		if ( m_cCurrentTask.iTask == EBorzhTaskInvalid )
			CheckForNewTasks();
	}

	if ( m_bNewTask )
		InitNewTask();

	DebugAssert( !m_bNewTask );

	// Update current task.
	if ( !m_bTaskFinished )
	{
		switch (m_cCurrentTask.iTask)
		{
			case EBorzhTaskWait:
				if ( CBotrixPlugin::fTime >= m_fEndWaitTime ) // Bot finished waiting.
					m_bTaskFinished = true;
				break;

			case EBorzhTaskLook:
				if ( !m_bNeedAim ) // Bot finished aiming.
					m_bTaskFinished = true;
				break;

			case EBorzhTaskMove:
				if ( !m_bNeedMove ) // Bot finished moving.
					m_bTaskFinished = true;
				break;

			case EBorzhTaskSpeak:
				m_bTaskFinished = true;
				break;

			case EBorzhTaskWaitAnswer:
				if ( m_bAllPlayersIdle )
				{
					m_bTaskFinished = true;
					m_bAllPlayersIdle = false;
					OfferCurrentBigTask(); // Repeat task request.
				}
				break;

			case EBorzhTaskWaitButton:
				m_bTaskFinished = (GET_TYPE(m_cCurrentBigTask.iArgument) != 0);
				break;
		}
	}
}


//----------------------------------------------------------------------------------------------------------------
void CBot_BorzhMod::CurrentWaypointJustChanged()
{
	CBot::CurrentWaypointJustChanged();
	
	// Don't do processing of paths if bot is stucked.
	if ( iPrevWaypoint == iCurrentWaypoint )
		return;

	m_aVisitedWaypoints.set(iCurrentWaypoint);

	// Check neighbours of a new waypoint.
	const CWaypoints::WaypointNode& cNode = CWaypoints::GetNode(iCurrentWaypoint);
	const CWaypoints::WaypointNode::arcs_t& cNeighbours = cNode.neighbours;
	for ( int i=0; i < cNeighbours.size(); ++i)
	{
		const CWaypoints::WaypointArc& cArc = cNeighbours[i];
		TWaypointId iWaypoint = cArc.target;
		if ( iWaypoint != iPrevWaypoint ) // Bot is not coming from there.
		{
			const CWaypointPath& cPath = cArc.edge;
			if ( FLAG_SOME_SET(FPathDoor, cPath.iFlags) ) // Waypoint path is passing through a door.
			{
				TEntityIndex iDoor = cPath.iArgument;
				if ( iDoor > 0 )
				{
					--iDoor;

					bool bOpened = CItems::IsDoorOpened(iDoor);
					bool bNeedToPassThrough = (iWaypoint == m_iAfterNextWaypoint) && m_bNeedMove && m_bUseNavigatorToMove;
					CheckDoorStatus(iDoor, bOpened, bNeedToPassThrough);
				}
				else
					CUtil::Message(NULL, "Error, waypoint path from %d to %d has invalid door index.", iCurrentWaypoint, iWaypoint);
			}
		}
	}

	// Check if bot saw the button before.
	if ( FLAG_SOME_SET(FWaypointButton, cNode.vertex.iFlags) )
	{
		TEntityIndex iButton = CWaypoint::GetButton(cNode.vertex.iArgument);
		if (iButton > 0)
		{
			--iButton;
			if ( !m_cSeenButtons.test(iButton) ) // Bot sees button for the first time.
			{
				m_cSeenButtons.set(iButton);
				SwitchToSpeakTask( EBorzhChatSeeButton, EEntityTypeButton, iButton );
			}
		}
		else
			CUtil::Message(NULL, "Error, waypoint %d has invalid button index.", iCurrentWaypoint);
	}
}

//----------------------------------------------------------------------------------------------------------------
bool CBot_BorzhMod::DoWaypointAction()
{
	const CWaypoint& cWaypoint = CWaypoints::Get(iCurrentWaypoint);

	// Check if bot enters new area.
	if ( cWaypoint.iAreaId != m_aPlayersAreas[m_iIndex] )
	{
		m_aPlayersAreas[m_iIndex] = cWaypoint.iAreaId;
		if ( m_aVisitedAreas.test(m_aPlayersAreas[m_iIndex]) )
			SwitchToSpeakTask(EBorzhChatChangeArea, EEntityTypeInvalid, m_aPlayersAreas[m_iIndex]);
		else
			SwitchToSpeakTask(EBorzhChatNewArea, EEntityTypeInvalid, m_aPlayersAreas[m_iIndex]);

		m_aVisitedAreas.set(m_aPlayersAreas[m_iIndex]);
		m_cVisitedAreasAfterPushButton.set(m_aPlayersAreas[m_iIndex]);
	}

	// Check if bot saw the button to shoot before.
	if ( FLAG_SOME_SET(FWaypointSeeButton, cWaypoint.iFlags) )
	{
		TEntityIndex iButton = CWaypoint::GetButton(cWaypoint.iArgument);
		if (iButton > 0)
		{
			--iButton;
			if ( !m_cSeenButtons.test(iButton) ) // Bot sees button for the first time.
			{
				if ( m_bHasCrossbow )
					SwitchToSpeakTask( EBorzhChatButtonWeapon, EEntityTypeButton, iButton );
				else
					SwitchToSpeakTask( EBorzhChatButtonNoWeapon, EEntityTypeButton, iButton );
				SwitchToSpeakTask( EBorzhChatSeeButton, EEntityTypeButton, iButton );
			}
		}
		else
			CUtil::Message(NULL, "Error, waypoint %d has invalid button index.", iCurrentWaypoint);
	}

	return CBot::DoWaypointAction();
}

//----------------------------------------------------------------------------------------------------------------
void CBot_BorzhMod::ApplyPathFlags()
{
	CBot::ApplyPathFlags();
}

//----------------------------------------------------------------------------------------------------------------
void CBot_BorzhMod::DoPathAction()
{
	CBot::DoPathAction();
}

//----------------------------------------------------------------------------------------------------------------
void CBot_BorzhMod::PickItem( const CEntity& cItem, TEntityType iEntityType, TEntityIndex iIndex )
{
	CBot::PickItem( cItem, iEntityType, iIndex );
	switch( iEntityType )
	{
	case EEntityTypeWeapon:
		TEntityIndex iIdx;
		if ( cItem.pItemClass->sClassName == "weapon_crossbow" ) 
			iIdx = CMod_Borzh::iVarValueWeaponCrossbow;
		else if ( cItem.pItemClass->sClassName == "weapon_physcannon" ) 
			iIdx = CMod_Borzh::iVarValueWeaponPhyscannon;
		else
			break;
		SwitchToSpeakTask(EBorzhChatWeaponFound, EEntityTypeInvalid, iIdx);
		break;
	}
}

//----------------------------------------------------------------------------------------------------------------
void CBot_BorzhMod::SetReachableAreas( int iCurrentArea, const good::bitset& cSeenDoors, const good::bitset& cOpenedDoors, good::bitset& cReachableAreas )
{
	cReachableAreas.reset();

	good::vector<TAreaId> cToVisit( CWaypoints::GetAreas().size() );
	cToVisit.push_back( iCurrentArea );

	const good::vector<CEntity>& cDoorEntities = CItems::GetItems(EEntityTypeDoor);
	while ( !cToVisit.empty() )
	{
		int iArea = cToVisit.back();
		cToVisit.pop_back();

		cReachableAreas.set(iArea);

		const good::vector<TEntityIndex>& cDoors = CMod_Borzh::GetDoorsForArea(iArea);
		for ( TEntityIndex i = 0; i < cDoors.size(); ++i )
		{
			const CEntity& cDoor = cDoorEntities[ cDoors[i] ];
			if ( cSeenDoors.test( cDoors[i] ) && cOpenedDoors.test( cDoors[i] ) ) // Seen and opened door.
			{
				TWaypointId iWaypoint1 = cDoor.iWaypoint;
				TWaypointId iWaypoint2 = (TWaypointId)cDoor.pArguments;
				if ( CWaypoints::IsValid(iWaypoint1) && CWaypoints::IsValid(iWaypoint2) )
				{
					TAreaId iArea1 = CWaypoints::Get(iWaypoint1).iAreaId;
					TAreaId iArea2 = CWaypoints::Get(iWaypoint2).iAreaId;
					TAreaId iNewArea = ( iArea1 == iArea ) ? iArea2 : iArea1;
					if ( !cReachableAreas.test(iNewArea) )
						cToVisit.push_back(iNewArea);
				}
			}
		}
	}
}

//----------------------------------------------------------------------------------------------------------------
TWaypointId CBot_BorzhMod::GetButtonWaypoint( TEntityIndex iButton, const good::bitset& cReachableAreas )
{
	const good::vector<CEntity>& cButtonEntities = CItems::GetItems(EEntityTypeButton);
	const CEntity& cButton = cButtonEntities[ iButton ];

	if ( CWaypoints::IsValid(cButton.iWaypoint) )
		return cReachableAreas.test( CWaypoints::Get(cButton.iWaypoint).iAreaId ) ? cButton.iWaypoint : EWaypointIdInvalid;
	return false;
}

//----------------------------------------------------------------------------------------------------------------
TWaypointId CBot_BorzhMod::GetDoorWaypoint( TEntityIndex iDoor, const good::bitset& cReachableAreas )
{
	const good::vector<CEntity>& cDoorEntities = CItems::GetItems(EEntityTypeDoor);
	const CEntity& cDoor = cDoorEntities[ iDoor ];

	TWaypointId iWaypoint1 = cDoor.iWaypoint;
	TWaypointId iWaypoint2 = (TWaypointId)cDoor.pArguments;
	if ( CWaypoints::IsValid(iWaypoint1) && CWaypoints::IsValid(iWaypoint2) )
	{
		TAreaId iArea1 = CWaypoints::Get(iWaypoint1).iAreaId;
		TAreaId iArea2 = CWaypoints::Get(iWaypoint2).iAreaId;
		return cReachableAreas.test(iArea1) ? iWaypoint1 : (cReachableAreas.test(iArea2) ? iWaypoint2 : EWaypointIdInvalid);
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------
void CBot_BorzhMod::CheckDoorStatus( TEntityIndex iDoor, bool bOpened, bool bNeedToPassThrough )
{
	bool bCheckingDoors = (m_cCurrentBigTask.iTask == EBorzhTaskButtonTry) && (GET_TYPE(m_cCurrentBigTask.iArgument) != 0); // We already pushed button.

	if ( !m_cSeenDoors.test(iDoor) ) // Bot sees door for the first time.
	{
		DebugAssert( m_cCurrentBigTask.iTask == EBorzhTaskExplore ); // Should occur only when exploring new area.
		m_cSeenDoors.set(iDoor);
		m_cOpenedDoors.set(iDoor, bOpened);

		TChatVariableValue iDoorStatus = bOpened ? CMod_Borzh::iVarValueDoorStatusOpened : CMod_Borzh::iVarValueDoorStatusClosed;
		SwitchToSpeakTask(EBorzhChatDoorFound, EEntityTypeDoor, iDoor, iDoorStatus);
	}
	else if ( !bOpened && bNeedToPassThrough ) // Bot needs to pass through the door, but door is closed.
		ClosedDoorOnTheWay(iDoor, bCheckingDoors);
	else if ( m_cOpenedDoors.test(iDoor) != bOpened ) // Bot belief of opened/closed door is different.
		DifferentDoorStatus(iDoor, bOpened, bCheckingDoors);
	else 
		SameDoorStatus(iDoor, bOpened, bCheckingDoors);
}

//----------------------------------------------------------------------------------------------------------------
void CBot_BorzhMod::SameDoorStatus( TEntityIndex iDoor, bool bOpened, bool bCheckingDoors )
{
	if ( bCheckingDoors && !m_cCheckedDoors.test(iDoor) ) // Checking doors but this one is not checked.
	{
		m_cCheckedDoors.set(iDoor);
		// Button that we are checking doesn't affect iDoor.
		SetButtonTogglesDoor(GET_INDEX(m_cCurrentBigTask.iArgument), iDoor, false);

		// Say: door $door has not changed.
		TChatVariableValue iDoorStatus = bOpened ? CMod_Borzh::iVarValueDoorStatusOpened : CMod_Borzh::iVarValueDoorStatusClosed;
		SwitchToSpeakTask(EBorzhChatDoorNoChange, EEntityTypeDoor, iDoor, iDoorStatus);
	}
}

//----------------------------------------------------------------------------------------------------------------
void CBot_BorzhMod::DifferentDoorStatus( TEntityIndex iDoor, bool bOpened, bool bCheckingDoors )
{
	m_cOpenedDoors.set(iDoor, bOpened);
	if ( bCheckingDoors )
	{
		DebugAssert( !m_cCheckedDoors.test(iDoor) ); // Door should not be checked already.
		m_cCheckedDoors.set(iDoor);
		// Button that we are checking opens iDoor.
		SetButtonTogglesDoor(GET_INDEX(m_cCurrentBigTask.iArgument), iDoor, true);

		// If door is opened, don't check areas behind it, for now. Will check after finishing checking doors.
		if ( !bOpened )
		{
			// Bot was thinking that this door was opened. Recalculate reachable areas (closing this door).
			m_cFalseOpenedDoors.reset(iDoor);

			// Recalculate reachable areas using false opened doors.
			SetReachableAreas(m_aPlayersAreas[m_iIndex], m_cSeenDoors, m_cFalseOpenedDoors, m_cReachableAreas);
		}
	}
	// Say: door $door is now $door_status.
	TChatVariableValue iDoorStatus = bOpened ? CMod_Borzh::iVarValueDoorStatusOpened : CMod_Borzh::iVarValueDoorStatusClosed;
	SwitchToSpeakTask(EBorzhChatDoorChange, EEntityTypeDoor, iDoor, iDoorStatus);
}

//----------------------------------------------------------------------------------------------------------------
void CBot_BorzhMod::ClosedDoorOnTheWay( TEntityIndex iDoor, bool bCheckingDoors )
{
	DebugAssert( m_cOpenedDoors.test(iDoor) ); // Bot should think that this door was opened.
	m_cOpenedDoors.reset(iDoor);

	CancelTask();
	if ( bCheckingDoors )
	{
		m_cFalseOpenedDoors.reset(iDoor);

		// Recalculate reachable areas using false opened doors.
		SetReachableAreas(m_aPlayersAreas[m_iIndex], m_cSeenDoors, m_cFalseOpenedDoors, m_cReachableAreas);
	}
	else
	{
		// Say: I can't reach area $area.
		TAreaId iArea = CWaypoints::Get( m_iDestinationWaypoint ).iAreaId;
		SwitchToSpeakTask(EBorzhChatAreaCantGo, EEntityTypeInvalid, iArea);

		// Recalculate reachable areas using opened doors.
		SetReachableAreas(m_aPlayersAreas[m_iIndex], m_cSeenDoors, m_cOpenedDoors, m_cReachableAreas);
	}

	// Say: Door $door is closed now.
	SwitchToSpeakTask(EBorzhChatDoorChange, EEntityTypeDoor, iDoor, CMod_Borzh::iVarValueDoorStatusClosed);
}

//----------------------------------------------------------------------------------------------------------------
void CBot_BorzhMod::CheckBigTask()
{
	DebugAssert( m_cCurrentTask.iTask == EBorzhTaskInvalid );
	switch ( m_cCurrentBigTask.iTask )
	{
		case EBorzhTaskExplore:
		{
			TAreaId iArea = m_cCurrentBigTask.iArgument;

			const good::vector<TWaypointId>& cWaypoints = CMod_Borzh::GetWaypointsForArea(iArea);
			DebugAssert( cWaypoints.size() > 0 );

			TWaypointId iWaypoint = EWaypointIdInvalid;
			int iIndex = rand() % cWaypoints.size();

			// Search for some not visited waypoint in this area.
			for ( int i=iIndex; i >= 0; --i)
			{
				int iCurrent = cWaypoints[i];
				if ( (iCurrent != iCurrentWaypoint) && !m_aVisitedWaypoints.test(iCurrent) )
				{
					iWaypoint = iCurrent;
					break;
				}
			}
			if ( iWaypoint == EWaypointIdInvalid )
			{
				for ( int i=iIndex+1; i < cWaypoints.size(); ++i)
				{
					int iCurrent = cWaypoints[i];
					if ( (iCurrent != iCurrentWaypoint) && !m_aVisitedWaypoints.test(iCurrent) )
					{
						iWaypoint = iCurrent;
						break;
					}
				}
			}

			DebugAssert( iWaypoint != iCurrentWaypoint );
			if ( iWaypoint == EWaypointIdInvalid )
			{
				m_aVisitedAreas.set(iArea);
				m_cCurrentBigTask.iTask = EBorzhTaskInvalid;
				DoSpeakTask(EBorzhChatFinishExplore);
			}
			else
			{
				// Start task to move to given waypoint.
				m_cCurrentTask.iTask = EBorzhTaskMove;
				m_cCurrentTask.iArgument = iWaypoint;
				m_bNewTask = true;
			}
			break;
		}

		case EBorzhTaskButtonTry:
		{
			const good::vector<CEntity>& aDoors = CItems::GetItems(EEntityTypeDoor);
			for ( int iDoor = 0; iDoor < aDoors.size(); ++iDoor )
				if ( !m_cCheckedDoors.test(iDoor) )
				{
					TWaypointId iWaypoint = GetDoorWaypoint(iDoor, m_cReachableAreas);
					if ( iWaypoint != EWaypointIdInvalid )
					{
						m_cCurrentTask.iTask = EBorzhTaskMove;
						m_cCurrentTask.iArgument = iWaypoint;
						m_bNewTask = true;
						return;
					}
				}
			m_cCurrentBigTask.iTask = EBorzhTaskInvalid;

			// Say I finished for waiting players.
			if ( m_cWaitingPlayers.any() )
			{
				m_cWaitingPlayers.reset();
				DoSpeakTask(EBorzhChatDone);
			}

			// Recalculate reachable areas.
			SetReachableAreas(m_aPlayersAreas[m_iIndex], m_cSeenDoors, m_cOpenedDoors, m_cReachableAreas);
			break;
		}
	}
}

//----------------------------------------------------------------------------------------------------------------
bool CBot_BorzhMod::CheckForNewTasks( TBorzhTask iProposedTask )
{
	//DebugAssert( m_cCurrentBigTask.iTask == EBorzhTaskInvalid );

	// Check if there is some player waiting for this bot. TODO:
	/*if ( m_cAcceptedPlayers.any() )
	{
		for ( TPlayerIndex iPlayer = 0; iPlayer < m_cAcceptedPlayers.size(); ++iPlayer )
			if ( m_cAcceptedPlayers.test(iPlayer) )
			{
				m_cCurrentBigTask.iTask = EBorzhTaskHelping;
				m_cCurrentBigTask.iArgument = iPlayer;

				// Wait for answer to the question.
				m_cCurrentTask.iTask = EBorzhTaskWaitAnswer;
				m_cCurrentTask.iArgument = 0;
				//SET_TYPE(, m_cCurrentTask.iArgument);
				//SET_INDEX(iPlayer, m_cCurrentTask.iArgument);
				return;
			}
	}*/

	// TODO: Check if all bots can pass to goal area.

	// Check if there is some area to investigate.
	if ( iProposedTask > EBorzhTaskExplore )
		return true;

	const StringVector& aAreas = CWaypoints::GetAreas();
	for ( TAreaId iArea = 0; iArea < aAreas.size(); ++iArea )
	{
		if ( !m_aVisitedAreas.test(iArea) && m_cReachableAreas.test(iArea) )
		{
			m_cCurrentBigTask.iTask = EBorzhTaskExplore;
			m_cCurrentBigTask.iArgument = iArea;
			SwitchToSpeakTask(EBorzhChatExploreNew, EEntityTypeInvalid, m_aPlayersAreas[m_iIndex]);
			return false;
		}
	}

	// Check if there are new button to push.
	if ( iProposedTask > EBorzhTaskButtonTry )
		return true;

	const good::vector<CEntity>& aButtons = CItems::GetItems(EEntityTypeButton);
	for ( TEntityIndex iButton = 0; iButton < aButtons.size(); ++iButton )
	{
		if ( m_cSeenButtons.test(iButton) && !m_cPushedButtons.test(iButton) && 
		     GetButtonWaypoint(iButton, m_cReachableAreas) != EWaypointIdInvalid ) // Can reach button.
		{
			PushCheckButtonTask(iButton);
			return false;
		}
	}

	// Check if there are unknown button-door configuration to check (without pushing intermediate buttons).
	/*const good::vector<CEntity>& aDoors = CItems::GetItems(EEntityTypeDoor);
	good::bitset cReachableAreas( aAreas.size() );

	for ( TEntityIndex iButton = 0; iButton < aButtons.size(); ++iButton )
	{
		TWaypointId iGetButtonWaypoint = aButtons[iButton].iWaypoint;
		TAreaId iAreaButton = ( iGetButtonWaypoint == EWaypointIdInvalid ) ? EAreaIdInvalid : CWaypoints::Get(iGetButtonWaypoint).iAreaId;

		if ( iAreaButton == EAreaIdInvalid )
		{
			// TODO: check shoot button.
		}
		else
		{
			for ( TPlayerIndex iPlayer = 0; iPlayer < m_cCollaborativePlayers.size(); ++iPlayer )
			{
				if ( (iPlayer != m_iIndex) && m_cCollaborativePlayers.test(iPlayer) && (m_aPlayersAreas[iPlayer] != EAreaIdInvalid) )
				{
					SetReachableAreas(m_aPlayersAreas[iPlayer], m_cSeenDoors, m_cOpenedDoors, cReachableAreas);

					if ()
					for ( TEntityIndex iDoor = 0; iDoor < aDoors.size(); ++iDoor )
					{
						if (  )
					}
				}
			}
		}
	}*/

	if ( iProposedTask == EBorzhTaskInvalid )
	{
		// Bot has nothing to do, wait for domain change.
		DoSpeakTask(EBorzhChatNoMoves);
		m_bNothingToDo = true;
		m_bDomainChanged = false;
		return false;
	}
	else
		return true; // Accept any task, we have nothing to do
}

//----------------------------------------------------------------------------------------------------------------
void CBot_BorzhMod::InitNewTask()
{
	m_bNewTask = m_bTaskFinished = false;

	switch ( m_cCurrentTask.iTask )
	{
		case EBorzhTaskWaitAnswer:
			CheckRemainingPlayersForBigTask();
		case EBorzhTaskWait:
			m_fEndWaitTime = CBotrixPlugin::fTime + m_cCurrentTask.iArgument/1000.0f;
			break;

		case EBorzhTaskLook:
		{
			TEntityType iType = GET_TYPE(m_cCurrentTask.iArgument);
			TEntityIndex iIndex = GET_INDEX(m_cCurrentTask.iArgument);
			const CEntity& cEntity = CItems::GetItems(iType)[iIndex];
			m_vLook = cEntity.vOrigin;
			m_bNeedMove = false;
			m_bNeedAim = true;
			break;
		}

		case EBorzhTaskMove:
			if ( iCurrentWaypoint == m_cCurrentTask.iArgument )
			{
				m_bNeedMove = m_bUseNavigatorToMove = m_bDestinationChanged = false;
				m_bTaskFinished = true;
				CurrentWaypointJustChanged();
			}
			else
			{
				m_bNeedMove = m_bUseNavigatorToMove = m_bDestinationChanged = true;
				m_iDestinationWaypoint = m_cCurrentTask.iArgument;
			}
			break;

		case EBorzhTaskSpeak:
			m_bNeedMove = false;
			DoSpeakTask(m_cCurrentTask.iArgument);
			break;

		case EBorzhTaskPushButton:
			FLAG_SET(IN_USE, m_cCmd.buttons);
			m_cPushedButtons.set(m_cCurrentTask.iArgument);
			m_aLastPushedButtons.push_back(m_cCurrentTask.iArgument);
			SET_TYPE(true, m_cCurrentBigTask.iArgument); // Save that button was pushed.
			m_cVisitedAreasAfterPushButton.reset();
			m_cVisitedAreasAfterPushButton.set(m_aPlayersAreas[m_iIndex]);
			Wait(m_iTimeAfterPushingButton); // Wait 2 seconds after pushing button.
			break;

		case EBorzhTaskShootButton: // TODO: set current weapon & right-click & wait & left click.
			m_cPushedButtons.set(m_cCurrentTask.iArgument);
			m_aLastPushedButtons.push_back(m_cCurrentTask.iArgument);
			SET_TYPE(true, m_cCurrentBigTask.iArgument); // Save that button was pushed.
			m_cVisitedAreasAfterPushButton.reset();
			m_cVisitedAreasAfterPushButton.set(m_aPlayersAreas[m_iIndex]);
			Wait(m_iTimeAfterPushingButton); // Wait 2 seconds after shooting button.
			break;
	}
}

//----------------------------------------------------------------------------------------------------------------
void CBot_BorzhMod::PushSpeakTask( TBotChat iChat, TEntityType iType, TEntityIndex iIndex, int iArguments )
{
	// Speak.
	CBorzhTask task(EBorzhTaskSpeak);
	SET_TYPE(iChat, task.iArgument);
	SET_INDEX(iIndex, task.iArgument);
	SET_AUX1(iArguments, task.iArgument);
	m_cTaskStack.push_back( task );

	if ( iType != EEntityTypeInvalid ) // There is something to look at.
	{
		// Look at entity before speak (door, button, box, etc).
		task.iTask = EBorzhTaskLook;
		task.iArgument = 0;
		SET_TYPE(iType, task.iArgument);
		SET_INDEX(iIndex, task.iArgument);
		m_cTaskStack.push_back( task );
	}
}

//----------------------------------------------------------------------------------------------------------------
void CBot_BorzhMod::DoSpeakTask( int iArgument )
{
	m_cChat.iBotRequest = GET_TYPE(iArgument);
	m_cChat.cMap.clear();

	switch ( m_cChat.iBotRequest )
	{
	case EBotChatGreeting:
		m_cChat.iDirectedTo = GET_INDEX(iArgument);
		break;

	case EBorzhChatDoorFound:
	case EBorzhChatDoorChange:
	case EBorzhChatDoorNoChange:
		// Add door status.
		m_cChat.cMap.push_back( CChatVarValue(CMod_Borzh::iVarDoorStatus, 0, GET_AUX1(iArgument)) );
		// Don't break to add door index.

	case EBorzhChatDoorTry:
	case EBorzhChatDoorGo:
		m_cChat.cMap.push_back( CChatVarValue(CMod_Borzh::iVarDoor, 0, GET_INDEX(iArgument) ) );
		break;

	case EBorzhChatSeeButton:
	case EBorzhChatButtonIPush:
	case EBorzhChatButtonYouPush:
	case EBorzhChatButtonIShoot:
	case EBorzhChatButtonYouShoot:
	case EBorzhChatButtonTry:
	case EBorzhChatButtonGo:
		m_cChat.cMap.push_back( CChatVarValue(CMod_Borzh::iVarButton, 0, GET_INDEX(iArgument) ) );
		break;

	case EBorzhChatWeaponFound:
		m_cChat.cMap.push_back( CChatVarValue(CMod_Borzh::iVarWeapon, 0, GET_INDEX(iArgument)) );
		break;

	case EBorzhChatNewArea:
	case EBorzhChatChangeArea:
	case EBorzhChatAreaCantGo:
	case EBorzhChatAreaGo:
		m_cChat.cMap.push_back( CChatVarValue(CMod_Borzh::iVarArea, 0, GET_INDEX(iArgument)) );
		break;
	}

	Speak(false);
	Wait(m_iTimeAfterSpeak);
}

//----------------------------------------------------------------------------------------------------------------
void CBot_BorzhMod::PushCheckButtonTask( TEntityIndex iButton, bool bShoot )
{
	DebugAssert( !bShoot || m_bHasCrossbow );

	// Mark all doors as unchecked.
	m_cCheckedDoors.reset();

	m_cCurrentBigTask.iTask = EBorzhTaskButtonTry;
	m_cCurrentBigTask.iArgument = 0;
	SET_INDEX(iButton, m_cCurrentBigTask.iArgument);

	//DebugAssert( m_cCurrentProposedTask.iTask == EBorzhTaskInvalid );
	m_cCurrentProposedTask = m_cCurrentBigTask;

	// Push button.
	m_cCurrentTask.iTask = bShoot ? EBorzhTaskShootButton : EBorzhTaskPushButton;
	m_cCurrentTask.iArgument = iButton;
	m_cTaskStack.push_back(m_cCurrentTask);

	// Say: I will push/shoot button $button now.
	m_cCurrentTask.iTask = EBorzhTaskSpeak;
	m_cCurrentTask.iArgument = 0;
	SET_TYPE(bShoot ? EBorzhChatButtonIShoot : EBorzhChatButtonIPush, m_cCurrentTask.iArgument);
	SET_INDEX(iButton, m_cCurrentTask.iArgument);
	m_cTaskStack.push_back(m_cCurrentTask);

	// Look at button.
	m_cCurrentTask.iTask = EBorzhTaskLook;
	m_cCurrentTask.iArgument = 0;
	SET_TYPE(EEntityTypeButton, m_cCurrentTask.iArgument);
	SET_INDEX(iButton, m_cCurrentTask.iArgument);
	m_cTaskStack.push_back(m_cCurrentTask);

	// Go to button waypoint.
	m_cCurrentTask.iTask = EBorzhTaskMove;
	const CEntity& cButton = CItems::GetItems(EEntityTypeButton)[iButton];
	if ( bShoot )
		m_cCurrentTask.iArgument = CMod_Borzh::GetWaypointToShootButton(iButton);
	else
		m_cCurrentTask.iArgument = cButton.iWaypoint;

	OfferCurrentBigTask();
}

//----------------------------------------------------------------------------------------------------------------
void CBot_BorzhMod::CheckRemainingPlayersForBigTask()
{
	DebugAssert( (m_cCurrentBigTask.iTask == EBorzhTaskButtonTry) || (m_cCurrentBigTask.iTask == EBorzhTaskButtonDoorConfig) );
	if ( m_cBusyPlayers.any() )
		return;

	for ( TPlayerIndex iPlayer = 0; iPlayer < CPlayers::Size(); ++iPlayer )
	{
		if ( iPlayer == m_iIndex )
			continue;

		CPlayer* pPlayer = CPlayers::Get(iPlayer);
		if ( pPlayer && (GetTeam() == pPlayer->GetTeam()) && !m_cAcceptedPlayers.test(iPlayer) )
			return;
	}
	m_bTaskFinished = true;
}

//----------------------------------------------------------------------------------------------------------------
void CBot_BorzhMod::OfferCurrentBigTask()
{
	m_cAcceptedPlayers.reset();
	m_bAllPlayersIdle = false;

	SaveCurrentTask();

	// Wait for answer after speak.
	m_cTaskStack.push_back( CBorzhTask(EBorzhTaskWaitAnswer, m_iTimeToWaitPlayer) );

	// Speak.
	if ( m_cCurrentBigTask.iTask == EBorzhTaskButtonTry )
	{
		int iButton = GET_INDEX(m_cCurrentBigTask.iArgument);

		// Say: Let's try to find which doors opens button $button.
		int iArgument = 0;
		SET_TYPE(EBorzhChatButtonTry, iArgument);
		SET_INDEX(iButton, iArgument);
		m_cTaskStack.push_back( CBorzhTask(EBorzhTaskSpeak, iArgument) );
		m_bTaskFinished = true;
	}
}

//----------------------------------------------------------------------------------------------------------------
void CBot_BorzhMod::ReceiveTaskOffer( TBorzhTask iProposedTask, int iArgument, TPlayerIndex iSpeaker )
{
	DebugAssert( m_cCurrentBigTask.iTask != EBorzhTaskButtonTry || GET_INDEX(m_cCurrentBigTask.iArgument) == iArgument );
	bool bAcceptTask = (m_cCurrentBigTask.iTask <= iProposedTask)/* || CheckForNewTasks(iProposedTask)*/;
	if ( bAcceptTask )
	{
		CancelTask();
		m_bNothingToDo = false;
		m_cCurrentBigTask.iTask = iProposedTask;
		switch ( iProposedTask )
		{
			case EBorzhTaskButtonTry:
				// iArgument is button index.
				m_cCurrentBigTask.iArgument = 0;
				SET_INDEX(iArgument, m_cCurrentBigTask.iArgument);
				m_cTaskStack.push_back( CBorzhTask(EBorzhTaskWaitButton, iArgument) ); // Wait for button to be pushed.
				SwitchToSpeakTask(EBorzhChatOk, EEntityTypeInvalid, EEntityIndexInvalid); // Accept the task to check doors after pushing button.
				break;

			default:
				DebugAssert(false);
		}
	}
	else
	{
		m_cWaitingPlayers.set(iSpeaker);
		SwitchToSpeakTask(EBorzhChatWait, EEntityTypeInvalid, EEntityIndexInvalid); // Tell player to wait, so bot can finish his business first.
	}
}
