#ifdef BOTRIX_TF2


#include <good/string_buffer.h>
#include <good/string_utils.h>

#include "clients.h"
#include "type2string.h"

#include "mods/tf2/bot_tf2.h"


extern char* szMainBuffer;
extern int iMainBufferSize;


//----------------------------------------------------------------------------------------------------------------
CBot_TF2::CBot_TF2( edict_t* pEdict, TBotIntelligence iIntelligence, int iTeam, int iClass ):
    CBot(pEdict, iIntelligence, iClass), m_aWaypoints(CWaypoints::Size()), m_cItemToSearch(-1, -1),
    m_cSkipWeapons( CWeapons::Size() ), m_iDesiredTeam(iTeam), m_pChasedEnemy(NULL)
{
    m_bShootAtHead = false;
}


//----------------------------------------------------------------------------------------------------------------
void CBot_TF2::Respawned()
{
    CBot::Respawned();
    m_bAlive = true;

    m_aWaypoints.reset();
    m_iFailWaypoint = EWaypointIdInvalid;

    m_iCurrentTask = EBotTaskInvalid;
    m_bNeedTaskCheck = true;
    m_bChasing = false;

    if ( m_iDesiredTeam == CMod::iUnassignedTeam )
        m_iDesiredTeam = ( CPlayers::GetTeamCount(2) > CPlayers::GetTeamCount(3) ) ? 3 : 2;
    if ( m_iDesiredTeam != GetTeam() )
        m_pPlayerInfo->ChangeTeam(m_iDesiredTeam);
}


//----------------------------------------------------------------------------------------------------------------
void CBot_TF2::ChangeTeam( TTeam iTeam )
{
    m_iDesiredTeam = iTeam;
    good::string_buffer sb(szMainBuffer, iMainBufferSize, false);
    const good::string& sClass = CTypeToString::ClassToString(m_iClass);
    sb << "joinclass " << sClass;
    BotMessage( "%s -> team %s, will joinclass %s.", GetName(),
                CTypeToString::TeamToString(iTeam).c_str(), sClass.c_str() );
    CBotrixPlugin::pServerPluginHelpers->ClientCommand( m_pEdict, sb.c_str() );
}


//----------------------------------------------------------------------------------------------------------------
void CBot_TF2::KilledEnemy( int iPlayerIndex, CPlayer* pVictim )
{
    CBot::KilledEnemy( iPlayerIndex, pVictim );
    if ( pVictim == m_pChasedEnemy )
    {
        m_pChasedEnemy = NULL;
        m_bChasing = false;
        m_iCurrentTask = EBotTaskInvalid;
        m_bNeedTaskCheck = true;
    }
}


//----------------------------------------------------------------------------------------------------------------
void CBot_TF2::HurtBy( int iPlayerIndex, CPlayer* pAttacker, int iHealthNow )
{
    if ( !m_bTest && !m_bDontAttack && (pAttacker != this) )
        CheckEnemy(iPlayerIndex, pAttacker, false);
    if ( iHealthNow < (CMod::iPlayerMaxHealth/2) )
        m_bNeedTaskCheck = true; // Check if need search for health.
}


//----------------------------------------------------------------------------------------------------------------
void CBot_TF2::Think()
{
    GoodAssert( !m_bTest );
    if ( !m_bAlive )
    {
        m_cCmd.buttons = rand() & IN_ATTACK; // Force bot to respawn by hitting randomly attack button.
        return;
    }

    if ( iCurrentWaypoint == EWaypointIdInvalid )
        return;

    bool bForceNewTask = false;

    // Check for move failure.
    if ( m_bMoveFailure || m_bStuck )
    {
        if ( m_bUseNavigatorToMove )
        {
            if ( iCurrentWaypoint == m_iFailWaypoint )
                m_iFailsCount++;
            else
            {
                m_iFailWaypoint = iCurrentWaypoint;
                m_iFailsCount = 1;
            }

            if ( m_iFailsCount >= 3 )
            {
                BLOG_W("Failed to follow path on same waypoint %d 3 times, marking task as finished.", iCurrentWaypoint);
                TaskFinished();
                m_bNeedTaskCheck = bForceNewTask = true;
                m_iFailsCount = 0;
                m_iFailWaypoint = -1;
            }
            else if ( m_bMoveFailure )
            {
                // Recalculate the route.
                m_iDestinationWaypoint = m_iTaskDestination;
                m_bNeedMove = m_bUseNavigatorToMove = m_bDestinationChanged = true;
            }
        }
        else
            m_bNeedMove = false; // As if we got to the needed destination.

        m_bMoveFailure = m_bStuck = false;
    }

    // Check if needs to add new tasks.
    if ( m_bNeedTaskCheck )
    {
        m_bNeedTaskCheck = false;
        CheckNewTasks(bForceNewTask);
    }

    if ( m_pCurrentEnemy && !m_bFlee && (m_iCurrentTask != EBotTaskEngageEnemy) )
    {
        m_iCurrentTask = EBotTaskEngageEnemy;
        m_pChasedEnemy = m_pCurrentEnemy;
        m_bNeedMove = false;
    }

    if ( m_iCurrentTask == EBotTaskEngageEnemy )
        CheckEngagedEnemy();
}


//----------------------------------------------------------------------------------------------------------------
void CBot_TF2::ReceiveChat( int /*iPlayerIndex*/, CPlayer* /*pPlayer*/, bool /*bTeamOnly*/, const char* /*szText*/ )
{
}


//----------------------------------------------------------------------------------------------------------------
bool CBot_TF2::DoWaypointAction()
{
    if ( m_bUseNavigatorToMove && (iCurrentWaypoint == m_iTaskDestination) )
    {
        m_iCurrentTask = EBotTaskInvalid;
        m_bNeedTaskCheck = true;
    }
    return CBot::DoWaypointAction();
}


//----------------------------------------------------------------------------------------------------------------
void CBot_TF2::CheckEngagedEnemy()
{
    GoodAssert( m_iCurrentTask == EBotTaskEngageEnemy );

    if ( m_pCurrentEnemy != m_pChasedEnemy ) // Start/stop seeing enemy or enemy change.
    {
        if ( m_pCurrentEnemy ) // Seeing new enemy.
        {
            m_pChasedEnemy = m_pCurrentEnemy;
            m_bChasing = CWeapon::IsValid(m_iWeapon) && m_aWeapons[m_iWeapon].IsMelee();
            m_bNeedMove = m_bUseNavigatorToMove = false; // Start moving on next tick.
        }
        else
        {
            // No current enemy is visible.
            if ( m_pChasedEnemy->IsAlive() )
            {
                if ( !m_bChasing || !m_bNeedMove ) // Lost sight of enemy, chase.
                {
                    if ( iCurrentWaypoint == m_pChasedEnemy->iCurrentWaypoint )
                        m_pCurrentEnemy = m_pChasedEnemy;
                    else
                        ChaseEnemy();
                }
            }
            else // Enemy dead, stop task.
            {
                m_bChasing = false;
                m_pChasedEnemy = NULL;
                m_iCurrentTask = EBotTaskInvalid;
                m_bNeedTaskCheck = true;
            }
        }
    }
    else if ( !m_bNeedMove || ( !m_bUseNavigatorToMove && !CWaypoint::IsValid(iNextWaypoint) ) )
    {
        // Bot arrived where enemy is or to next random waypoint.
        // Tip: current enemy == chased enemy.
        GoodAssert( m_pChasedEnemy );
        // Bot arrived at adyacent waypoint or is seeing enemy.
        m_bNeedMove = m_bDestinationChanged = true;
        m_bUseNavigatorToMove = m_bChasing = false;

        if ( CWeapon::IsValid(m_iWeapon) )
        {
            if ( m_aWeapons[m_iWeapon].IsMelee() ) // Rush toward enemy.
            {
                CWaypointPath* pPath;
                bool bNear = (m_pChasedEnemy->iCurrentWaypoint == iCurrentWaypoint) ||
                             ( (pPath = CWaypoints::GetPath(iCurrentWaypoint, m_pChasedEnemy->iCurrentWaypoint)) &&
                               !pPath->IsActionPath() );
                if ( bNear )
                {
                    // We are near enemy waypoint using melee weapon, rush to enemy head.
                    iNextWaypoint = EWaypointIdInvalid;
                    m_vDestination = m_pCurrentEnemy->GetHead();
                    m_bNeedMove = true;
                }
                else
                    ChaseEnemy();
                return;
            }
            else if ( CWaypoints::bValidVisibilityTable &&
                      CWaypoint::IsValid(m_pCurrentEnemy->iCurrentWaypoint) &&
                      (m_iIntelligence >= EBotNormal) )
            {
                if ( FLAG_SOME_SET(FFightStrategyRunAwayIfNear, CBot::iDefaultFightStrategy) &&
                     (m_fDistanceSqrToEnemy <= CBot::fNearDistanceSqr) )
                {
                    // Try to run away a little.
                    iNextWaypoint = CWaypoints::GetFarestNeighbour( iCurrentWaypoint, m_pCurrentEnemy->iCurrentWaypoint, true );
                    BotDebug( "%s -> Moving to far waypoint %d (current %d)", GetName(), iNextWaypoint, iCurrentWaypoint );
                    return;
                }
                else if ( FLAG_SOME_SET(FFightStrategyComeCloserIfFar, CBot::iDefaultFightStrategy) &&
                          m_fDistanceSqrToEnemy >= CBot::fFarDistanceSqr )
                {
                    // Try to come closer a little.
                    iNextWaypoint = CWaypoints::GetNearestNeighbour( iCurrentWaypoint, m_pCurrentEnemy->iCurrentWaypoint, true );
                    BotDebug( "%s -> Moving to near waypoint %d (current %d)", GetName(), iNextWaypoint, iCurrentWaypoint );
                    return;
                }
            }
        }

        iNextWaypoint = CWaypoints::GetRandomNeighbour(iCurrentWaypoint, m_pCurrentEnemy->iCurrentWaypoint, true);
        BotMessage( "%s -> Moving to random waypoint %d (current %d)", GetName(), iNextWaypoint, iCurrentWaypoint );
    }
    else if ( m_pCurrentEnemy && m_bUseNavigatorToMove &&
              CWeapon::IsValid(m_iWeapon) && !m_aWeapons[m_iWeapon].IsMelee() )
        m_bNeedMove = m_bUseNavigatorToMove = false;
    else if ( m_bChasing && (iCurrentWaypoint != m_pChasedEnemy->iCurrentWaypoint ) &&
              (CBotrixPlugin::fTime >= m_fChaseEnemyTime) )
        ChaseEnemy(); // Recalculate route to enemy every 3 seconds.
}


//----------------------------------------------------------------------------------------------------------------
void CBot_TF2::CheckNewTasks( bool bForceTaskChange )
{
    TBotTask iNewTask = EBotTaskInvalid;
    bool bForce = bForceTaskChange || (m_iCurrentTask == EBotTaskInvalid);

    const CWeapon* pWeapon = ( m_bFeatureWeaponCheck && CWeapon::IsValid(m_iBestWeapon) )
        ? m_aWeapons[m_iBestWeapon].GetBaseWeapon()
        : NULL;
    TBotIntelligence iWeaponPreference = m_iIntelligence;

    bool bNeedHealth = CMod::HasMapItems(EItemTypeHealth) && ( m_pPlayerInfo->GetHealth() < CMod::iPlayerMaxHealth );
    bool bNeedHealthBad = bNeedHealth && ( m_pPlayerInfo->GetHealth() < (CMod::iPlayerMaxHealth/2) );
    bool bAlmostDead = bNeedHealthBad && ( m_pPlayerInfo->GetHealth() < (CMod::iPlayerMaxHealth/5) );
    bool bNeedWeapon = pWeapon && CMod::HasMapItems(EItemTypeWeapon);
    bool bNeedAmmo = pWeapon && CMod::HasMapItems(EItemTypeAmmo);

    TWeaponId iWeapon = EWeaponIdInvalid;
    bool bSecondary = false;

    const CItemClass* pEntityClass = NULL; // Weapon or ammo class to search for.

    if ( bAlmostDead )
    {
        m_bDontAttack = (m_iIntelligence <= EBotNormal);
        m_bFlee = true;
    }

    int retries = 0;

restart_find_task: // TODO: remove gotos.
    retries++;
    if ( retries == 5 )
    {
        iNewTask = EBotTaskFindEnemy;
        pEntityClass = NULL;
        goto find_enemy;
    }

    if ( bNeedHealthBad ) // Need health pretty much.
    {
        iNewTask = EBotTaskFindHealth;
        bForce = true;
    }
    else if ( bNeedWeapon && (pWeapon->iBotPreference < iWeaponPreference) ) // Need some weapon with higher preference.
    {
        iNewTask = EBotTaskFindWeapon;
    }
    else if ( bNeedAmmo )
    {
        // Need ammunition.
        bool bNeedAmmo0 = (m_aWeapons[m_iBestWeapon].ExtraBullets(0) < pWeapon->iClipSize[0]); // Has less than 1 extra clip.
        bool bNeedAmmo1 = m_aWeapons[m_iBestWeapon].HasSecondary() && // Has secondary function, but no secondary bullets.
                          !m_aWeapons[m_iBestWeapon].HasAmmoInClip(1) && !m_aWeapons[m_iBestWeapon].HasAmmoExtra(1);

        if ( bNeedAmmo0 || bNeedAmmo1 )
        {
            iNewTask = EBotTaskFindAmmo;
            // Prefer search for secondary ammo only if has extra bullets for primary.
            bSecondary = bNeedAmmo1 && (m_aWeapons[m_iBestWeapon].ExtraBullets(0) > 0);
        }
        else if ( bNeedHealth ) // Need health (but has more than 50%).
            iNewTask = EBotTaskFindHealth;
        else if ( CMod::HasMapItems(EItemTypeArmor) && (m_pPlayerInfo->GetArmorValue() < CMod::iPlayerMaxArmor) ) // Need armor.
            iNewTask = EBotTaskFindArmor;
        else if ( bNeedWeapon && (pWeapon->iBotPreference < EBotPro) ) // Check if can find a better weapon.
        {
            iNewTask = EBotTaskFindWeapon;
            iWeaponPreference = pWeapon->iBotPreference+1;
        }
        else if ( !m_aWeapons[m_iBestWeapon].HasFullAmmo(1) ) // Check if weapon needs secondary ammo.
        {
            iNewTask = EBotTaskFindAmmo;
            bSecondary = true;
        }
        else if ( !m_aWeapons[m_iBestWeapon].HasFullAmmo(0) ) // Check if weapon needs primary ammo.
        {
            iNewTask = EBotTaskFindAmmo;
            bSecondary = false;
        }
        else
            iNewTask = EBotTaskFindEnemy;
    }
    else
        iNewTask = EBotTaskFindEnemy;

    switch(iNewTask)
    {
    case EBotTaskFindWeapon:
        pEntityClass = NULL;

        // Get weapon entity class to search for. Search for better weapons that actually have.
        for ( TBotIntelligence iPreference = iWeaponPreference; iPreference < EBotIntelligenceTotal; ++iPreference )
        {
            iWeapon = CWeapons::GetRandomWeapon(iPreference, m_cSkipWeapons);
            if ( iWeapon != EWeaponIdInvalid )
            {
                pEntityClass = CWeapons::Get(iWeapon).GetBaseWeapon()->pWeaponClass;
                break;
            }
        }
        if ( pEntityClass == NULL )
        {
            // None found, search for worst weapons.
            for ( TBotIntelligence iPreference = iWeaponPreference-1; iPreference >= 0; --iPreference )
            {
                iWeapon = CWeapons::GetRandomWeapon(iPreference, m_cSkipWeapons);
                if ( iWeapon != EWeaponIdInvalid )
                {
                    pEntityClass = CWeapons::Get(iWeapon).GetBaseWeapon()->pWeaponClass;
                    break;
                }
            }
        }
        if ( pEntityClass == NULL )
        {
            bNeedWeapon = false;
            goto restart_find_task;
        }
        break;

    case EBotTaskFindAmmo:
        // Get ammo entity class to search for.
        iWeapon = m_iBestWeapon;

        // Randomly search for weapon instead, as it gives same primary bullets.
        if ( !bSecondary && bNeedWeapon && CItems::ExistsOnMap(pWeapon->pWeaponClass) && (rand() & 1) )
        {
            iNewTask = EBotTaskFindWeapon;
            pEntityClass = pWeapon->pWeaponClass;
        }
        else
        {
            int iAmmoEntitiesSize = pWeapon->aAmmos[bSecondary].size();
            pEntityClass = (iAmmoEntitiesSize > 0) ? pWeapon->aAmmos[bSecondary][ rand() % iAmmoEntitiesSize ] : NULL;
        }

        if ( !pEntityClass || !CItems::ExistsOnMap(pEntityClass) ) // There are no such weapon/ammo on the map.
        {
            iNewTask = EBotTaskFindEnemy; // Just find enemy.
            pEntityClass = NULL;
        }
        break;

    case EBotTaskFindHealth:
    case EBotTaskFindArmor:
        pEntityClass = CItems::GetRandomItemClass(EItemTypeHealth + (iNewTask - EBotTaskFindHealth));
        break;
    }

    BASSERT( iNewTask != EBotTaskInvalid, return );

find_enemy:
    // Check if need task switch.
    if ( bForce || (m_iCurrentTask != iNewTask) )
    {
        m_iCurrentTask = iNewTask;
        if ( pEntityClass ) // Health, armor, weapon, ammo.
        {
            TItemType iType = EItemTypeHealth + (iNewTask - EBotTaskFindHealth);
            TItemIndex iItemToSearch = CItems::GetNearestItem( iType, GetHead(), m_aPickedItems, pEntityClass );

            if ( iItemToSearch == -1 )
            {
                if ( (m_iCurrentTask == EBotTaskFindWeapon) && (iWeapon != EWeaponIdInvalid) )
                    m_cSkipWeapons.set(iWeapon);
                m_iCurrentTask = EBotTaskInvalid;
                goto restart_find_task;
            }
            else
            {
                m_iTaskDestination = CItems::GetItems(iType)[iItemToSearch].iWaypoint;
                m_cItemToSearch.iType = iType;
                m_cItemToSearch.iIndex = iItemToSearch;
            }
        }
        else if (m_iCurrentTask == EBotTaskFindEnemy)
        {
            // Just go to some random waypoint.
            m_iTaskDestination = -1;
            GoodAssert( CWaypoints::Size() >= 2 );
            do {
                m_iTaskDestination = rand() % CWaypoints::Size();
            } while ( m_iTaskDestination == iCurrentWaypoint );
        }

        // Check if waypoint to go to is valid.
        if ( (m_iTaskDestination == EWaypointIdInvalid) || (m_iTaskDestination == iCurrentWaypoint) )
        {
            BLOG_W( "%s -> task %s, invalid destination waypoint %d (current %d), recalculate task.", GetName(),
                    CTypeToString::BotTaskToString(m_iCurrentTask).c_str(), m_iTaskDestination, iCurrentWaypoint );
            m_iCurrentTask = -1;
            m_bNeedTaskCheck = true; // Check new task in next frame.
            m_bNeedMove = m_bUseNavigatorToMove = m_bDestinationChanged = false;

            // Bot searched for item at current waypoint, but item was not there. Add it to array of picked items.
            TaskFinished();
        }
        else
        {
            BotMessage( "%s -> new task: %s (%s), waypoint %d (current %d).", GetName(), CTypeToString::BotTaskToString(m_iCurrentTask).c_str(),
                        pEntityClass ? pEntityClass->sClassName.c_str() : "", m_iTaskDestination, iCurrentWaypoint );

            m_iDestinationWaypoint = m_iTaskDestination;
            m_bNeedMove = m_bUseNavigatorToMove = m_bDestinationChanged = true;
        }
    }
    m_cSkipWeapons.reset();
}


//----------------------------------------------------------------------------------------------------------------
void CBot_TF2::TaskFinished()
{
    if ( (EBotTaskFindHealth <= m_cItemToSearch.iType) && (m_cItemToSearch.iType <= EBotTaskFindAmmo) )
    {
        const CItem& cItem = CItems::GetItems(m_cItemToSearch.iType)[m_cItemToSearch.iIndex];

        // If item is not respawnable (or just bad configuration), force to not to search for it again right away, but in 1 minute.
        m_cItemToSearch.fRemoveTime = CBotrixPlugin::fTime;
        m_cItemToSearch.fRemoveTime += FLAG_ALL_SET_OR_0(FEntityRespawnable, cItem.iFlags) ? cItem.pItemClass->GetArgument() : 60.0f;

        m_aPickedItems.push_back(m_cItemToSearch);
    }
    else // Bot was heading to waypoint, not item.
    {
        //m_aWaypoints.set(m_cItemToSearch.iIndex);

        m_cItemToSearch.fRemoveTime = CBotrixPlugin::fTime + 60.0f; // Do not go at that waypoint again at least for 1 minute.
        m_aPickedItems.push_back(m_cItemToSearch);
    }
}


#endif // BOTRIX_TF2
