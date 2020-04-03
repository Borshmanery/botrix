#include <good/string_buffer.h>
#include <good/string_utils.h>
#include <good/log.h>

#include "bot.h"
#include "clients.h"
#include "config.h"
#include "console_commands.h"
#include "item.h"
#include "waypoint.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define MAIN_COMMAND "botrix"

good::unique_ptr<CBotrixCommand> CBotrixCommand::instance;

const good::string sHelp( "help" ); // TODO: all commands.

const good::string sAll( "all" );
const good::string sNone( "none" );
const good::string sNext( "next" );

const good::string sRandom( "random" );

const good::string sWeapon = "weapon";
const good::string sAmmo = "ammo";
const good::string sHealth = "health";
const good::string sArmor = "armor";
const good::string sButton = "button";
const good::string sFirstAngle = "angle1";
const good::string sSecondAngle = "angle2";

const good::string sCurrent = "current";
const good::string sDestination = "destination";

const good::string sUnlock = "unlock";

extern char* szMainBuffer;
extern int iMainBufferSize;

StringVector aBoolsCompletion(2);
StringVector aWaypointCompletion(2);

//----------------------------------------------------------------------------------------------------------------
// Singleton to access console variables.
//----------------------------------------------------------------------------------------------------------------
//CPluginConVarAccessor CPluginConVarAccessor::instance;
//
//bool CPluginConVarAccessor::RegisterConCommandBase( ConCommandBase *pCommand )
//{
//	// Link to engine's list.
//	CBotrixPlugin::pCvar->RegisterConCommand( pCommand );
//	return true;
//}


//----------------------------------------------------------------------------------------------------------------
// CConsoleCommand.
//----------------------------------------------------------------------------------------------------------------
#if defined(BOTRIX_NO_COMMAND_COMPLETION)
#elif defined(BOTRIX_OLD_COMMAND_COMPLETION)

int CConsoleCommand::AutoComplete( const char* partial, int partialLength,
                                   char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ],
                                   int strIndex, int charIndex )
{
    if (charIndex + partialLength >= COMMAND_COMPLETION_ITEM_LENGTH ||
        strIndex >= COMMAND_COMPLETION_ITEM_LENGTH-1)
        return 0; // Check bounds.

    int result = 0;
    int maxLength = COMMAND_COMPLETION_ITEM_LENGTH - charIndex - 1; // Save one space for trailing 0.

    if ( partialLength <= m_sCommand.size() )
    {
        if ( strncmp( m_sCommand.c_str(), partial, partialLength ) == 0 )
        {
            // Autocomplete only command name.
            strncpy( &commands[strIndex][charIndex], m_sCommand.c_str(), MIN2(maxLength, m_sCommand.size()+1) );
            commands[strIndex+result][COMMAND_COMPLETION_ITEM_LENGTH-1] = 0;
            result++;
        }
    }
    else
    {
        if ( m_cAutoCompleteArguments.size() > 0 &&
             strncmp( m_sCommand.c_str(), partial, m_sCommand.size() ) == 0 )
        {
            // Autocomplete command name with arguments.
            good::string part(partial, false, false, partialLength);

            int start = m_sCommand.size();
            if ( part[start] == ' ' )
            {
                int lastSpace = part.rfind(' ');
                if ( lastSpace != good::string::npos )
                {
                    if ( !m_bAutoCompleteOnlyOneArgument || (lastSpace == start) )
                    {
                        lastSpace++;
                        good::string partArg(&partial[lastSpace], false, false, partialLength - lastSpace);

                        maxLength = COMMAND_COMPLETION_ITEM_LENGTH - (charIndex + lastSpace) - 1; // Save one space for trailing 0.
                        if ( maxLength > 0 ) // There is still space in autocomplete field.
                        {
                            for ( int i = 0; i < m_cAutoCompleteArguments.size(); ++i )
                            {
                                const good::string& arg = m_cAutoCompleteArguments[i];
                                if ( good::starts_with(arg, partArg) )
                                {
                                    strncpy( &commands[strIndex+result][charIndex], partial, lastSpace );
                                    strncpy( &commands[strIndex+result][charIndex+lastSpace], arg.c_str(), MIN2(maxLength, arg.size()+1) );
                                    commands[strIndex+result][COMMAND_COMPLETION_ITEM_LENGTH-1] = 0;
                                    result++;
                                    if ( strIndex+result >= COMMAND_COMPLETION_ITEM_LENGTH-1 )
                                        return result; // Bound check.
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return result;
}

#else // BOTRIX_OLD_COMMAND_COMPLETION

int CConsoleCommand::AutoComplete( good::string& partial, CUtlVector<CUtlString>& cCommands, int charIndex )
{
    int result = 0;

    const char* szSubPartial = &partial[charIndex];
	int iLen = partial.size() - charIndex;

    if ( iLen <= m_sCommand.size() )
    {
        if ( strncmp( m_sCommand.c_str(), szSubPartial, iLen ) == 0 )
        {
            // Autocomplete only command name.
            CUtlString sStr( partial.c_str(), charIndex );
            sStr.Append( m_sCommand.c_str() );
            cCommands.AddToTail( sStr );
            result++;
        }
    }
    else
    {
		BASSERT(m_cAutoCompleteArguments.size() == m_cAutoCompleteValues.size(), return result);

		char last = szSubPartial[m_sCommand.size()]; // Can't be 0, because iLen > command length
        if ( (m_cAutoCompleteArguments.size() > 0) &&
             (strncmp( m_sCommand.c_str(), szSubPartial, m_sCommand.size() ) == 0) &&
			 (last == ' ') )
        {
            szSubPartial += m_sCommand.size() + 1;
            iLen -= m_sCommand.size() + 1;

			while ( iLen > 0 && *szSubPartial == ' ')
            {
                ++szSubPartial;
                --iLen;
            }

            // Autocomplete command name with arguments.
            good::string sArg(szSubPartial, false, false, iLen);
			int currentArg = 0, lastSpace = 0;
			bool hadQuote = false;
			for (int i = 0; i < iLen; ++i) {
				if ( sArg[i] == '"' )
					hadQuote = !hadQuote;
				else if ( (sArg[i] == ' ') && !hadQuote )
				{
					while ( sArg[++i] == ' ' ); // Skip spaces.
					lastSpace = i;
					i--; // Will increment i again.
					++currentArg;
				}
			}

			// Get current argument type.
			TConsoleAutoCompleteArg argType = currentArg < m_cAutoCompleteArguments.size() ? m_cAutoCompleteArguments[currentArg] : EConsoleAutoCompleteArgInvalid;
			if ( argType == EConsoleAutoCompleteArgInvalid )
			{
				switch ( m_cAutoCompleteArguments.back() )
				{
				case EConsoleAutoCompleteArgValuesForever:
					argType = EConsoleAutoCompleteArgValues;
					currentArg = m_cAutoCompleteValues.size() - 1;
					break;

				case EConsoleAutoCompleteArgPlayersForever:
					argType = EConsoleAutoCompleteArgPlayers;
					break;

				case EConsoleAutoCompleteArgUsersForever:
					argType = EConsoleAutoCompleteArgUsers;
					break;

				case EConsoleAutoCompleteArgBotsForever:
					argType = EConsoleAutoCompleteArgBots;
					break;

				case EConsoleAutoCompleteArgWaypointForever:
					argType = EConsoleAutoCompleteArgWaypoint;
					break;

				default:
					return result;
				}
			}

			// Get completion values.
			StringVector completionValues;
			StringVector* completion = NULL;
			switch (argType) {
			case EConsoleAutoCompleteArgBool:
				completion = &aBoolsCompletion;
				break;

			case EConsoleAutoCompleteArgValues:
			case EConsoleAutoCompleteArgValuesForever:
				completion = &m_cAutoCompleteValues[currentArg];
				break;

			case EConsoleAutoCompleteArgWaypoint:
			case EConsoleAutoCompleteArgWaypointForever:
				completion = &aWaypointCompletion;
				break;

			case EConsoleAutoCompleteArgBots:
			case EConsoleAutoCompleteArgBotsForever:
			case EConsoleAutoCompleteArgUsers:
			case EConsoleAutoCompleteArgUsersForever:
			case EConsoleAutoCompleteArgPlayers:
			case EConsoleAutoCompleteArgPlayersForever:
				completionValues.push_back( sAll );
				CPlayers::GetNames( 
					completionValues, 
					argType != EConsoleAutoCompleteArgUsers && argType != EConsoleAutoCompleteArgUsersForever,
					argType != EConsoleAutoCompleteArgBots && argType != EConsoleAutoCompleteArgBotsForever
				);
				completion = &completionValues;
				for ( int i = 1; i < completionValues.size(); ++i )
				{
					if ( completionValues[i].find(' ') != good::string::npos )
					{
						good::string_buffer sBuff( completionValues[i].size() + 2 );
						sBuff.append('"');
						sBuff.append(completionValues[i]);
						sBuff.append('"');
						completionValues[i] = sBuff;
					}
				}
				break;

			default:
				return result;
			}
			
			if ( completion->empty() )
				return result;
            
			good::string sPartArg(&sArg[lastSpace], true, true, iLen - lastSpace);
			good::string sCmd(partial.c_str(), true, true, partial.size() - sPartArg.size());

			for ( int i = 0; i < completion->size(); ++i )
            {
				const good::string& arg = (*completion)[i];
                if ( good::starts_with(arg, sPartArg) )
                {
                    CUtlString sStr( sCmd.c_str() );
                    sStr.Append( arg.c_str() );
                    cCommands.AddToTail( sStr );
                    result++;
                }
            }
        }
    }
    return result;
}

#endif // BOTRIX_OLD_COMMAND_COMPLETION

TCommandResult CConsoleCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( argc == 1 && sHelp == argv[0] )
	{
		PrintCommand( pClient ? pClient->GetEdict() : NULL, 0 );
		return ECommandPerformed;
	}
	return ECommandNotFound;
}

void CConsoleCommand::PrintCommand( edict_t* pPrintTo, int indent )
{
    bool bHasAccess = true;

    if ( pPrintTo )
    {
        CPlayer* pPlayer = CPlayers::Get( pPrintTo );
        BASSERT( pPlayer && !pPlayer->IsBot(), return );
        CClient* pClient = (CClient*)pPlayer;
        bHasAccess = HasAccess(pClient);
    }

    int i;
    for ( i = 0; i < indent*2; i ++ )
        szMainBuffer[i] = ' ';
    szMainBuffer[i]=0;

    const char* sCantUse = bHasAccess ? "" : "[can't use]";
    BULOG_I( pPrintTo, "%s[%s]%s: %s", szMainBuffer, m_sCommand.c_str(), sCantUse, m_sHelp.c_str() );
    if ( m_sDescription.length() > 0 )
        BULOG_I( pPrintTo, "%s  %s", szMainBuffer, m_sDescription.c_str() );
}


//----------------------------------------------------------------------------------------------------------------
// CConsoleCommandContainer.
//----------------------------------------------------------------------------------------------------------------
#if defined(BOTRIX_NO_COMMAND_COMPLETION)
#elif defined(BOTRIX_OLD_COMMAND_COMPLETION)

int CConsoleCommandContainer::AutoComplete( const char* partial, int partialLength, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ], int strIndex, int charIndex )
{
    int result = 0;
    int command_size = m_sCommand.size();

    if ( command_size >= partialLength ) // only Add command to commands array
    {
        if ( strncmp( m_sCommand.c_str(), partial, partialLength ) == 0 )
        {
            strcpy( &commands[strIndex][charIndex], m_sCommand.c_str() ); // e.g. "way" -> "waypoint"
            result++;
        }
    }
    else
    {
        if ( strncmp( m_sCommand.c_str(), partial, command_size ) == 0 )
        {
            partial += command_size;
            partialLength -= command_size;
            while ( *partial == ' ' )
            {
                partial++; // remove root command from partial command(e.g. "botrix way" -> "way")
                partialLength--;
            }

            int charIdx = charIndex + command_size + 1; // 1 is for space

            for ( int i = 0; i < m_aCommands.size(); i ++ )
            {
                int count = m_aCommands[i]->AutoComplete(partial, partialLength, commands, strIndex, charIdx);
                for ( int j = 0; j < count; j ++ )
                {
                    strncpy(&commands[strIndex][charIndex], m_sCommand.c_str(), command_size);
                    commands[strIndex][charIndex+command_size] = ' ';
                    strIndex ++;
                    result ++;
                }
            }
        }
    }

    return result;
}

#else // BOTRIX_OLD_COMMAND_COMPLETION

int CConsoleCommandContainer::AutoComplete( good::string& partial, CUtlVector< CUtlString > &cCommands, int charIndex )
{
    int result = 0;
    int command_size = m_sCommand.size();

    if ( command_size >= partial.size() - charIndex ) // Only add this command to commands array.
    {
        if ( strncmp(m_sCommand.c_str(), &partial[charIndex], partial.size() - charIndex) == 0 )
        {
            // Autocomplete only command name.
            CUtlString sStr( partial.c_str(), charIndex );
            sStr.Append( m_sCommand.c_str() );
            cCommands.AddToTail( sStr );
            result++;
        }
    }
    else
    {
		if (strncmp(m_sCommand.c_str(), &partial[charIndex], command_size) == 0)
        {
			int partialLength = partial.size();
            int iLen = partialLength - charIndex - command_size - 1;
            const char* szSubPartial = &partial[partialLength - iLen];
            while ( *szSubPartial == ' ' )
            {
                szSubPartial++;
                iLen--;
            }

            for ( int i = 0; i < m_aCommands.size(); i++ )
                result += m_aCommands[i]->AutoComplete(partial, cCommands, partialLength - iLen);
        }
    }

    return result;
}

#endif // BOTRIX_OLD_COMMAND_COMPLETION

TCommandResult CConsoleCommandContainer::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

    if ( argc > 0 )
    {
        for ( int i = 0; i < m_aCommands.size(); i ++ )
        {
            CConsoleCommand *pCommand = m_aCommands[i].get();

            if ( pCommand->IsCommand(argv[0]) )
            {
                if ( pCommand->HasAccess( pClient ) )
                    return pCommand->Execute( pClient, argc-1, &argv[1] );
                else
                    return ECommandRequireAccess;
            }
        }
    }

    PrintCommand( pClient ? pClient->GetEdict() : NULL );
    return ECommandNotFound;
}

void CConsoleCommandContainer::PrintCommand( edict_t* pPrintTo, int indent )
{
    int i;
    for ( i = 0; i < indent*2; i ++ )
        szMainBuffer[i] = ' ';
    szMainBuffer[i]=0;

    BULOG_I( pPrintTo, "%s[%s]", szMainBuffer, m_sCommand.c_str() );
    for ( int i = 0; i < m_aCommands.size(); i ++ )
        m_aCommands[i]->PrintCommand( pPrintTo, indent+1 );
}



//----------------------------------------------------------------------------------------------------------------
// Userful functions.
//----------------------------------------------------------------------------------------------------------------
TWaypointId GetWaypointId( int iCurrentIndex, int argc, const char **argv, CClient *pClient, int iDefaultId )
{
	TWaypointId id = -1;
	if ( iCurrentIndex == argc )
		id = iDefaultId;
	else if ( iCurrentIndex < argc )
	{
		if ( sCurrent == argv[iCurrentIndex] )
			id = pClient->iCurrentWaypoint;
		else if ( sDestination == argv[iCurrentIndex] )
			id = pClient->iDestinationWaypoint;
		else
			sscanf( argv[iCurrentIndex], "%d", &id );
	}
	return id;
}


//----------------------------------------------------------------------------------------------------------------
// Waypoints commands.
//----------------------------------------------------------------------------------------------------------------
CWaypointDrawFlagCommand::CWaypointDrawFlagCommand()
{
    m_sCommand = "drawtype";
    m_sHelp = "defines how to draw waypoint";
    m_sDescription = good::string("Can be 'none' / 'all' / 'next' or mix of: ") + CTypeToString::WaypointDrawFlagsToString(FWaypointDrawAll);
    m_iAccessLevel = FCommandAccessWaypoint;

	StringVector args;
	args.push_back(sNone);
	args.push_back(sAll);
	args.push_back(sNext);
	for (int i = 0; i < EWaypointDrawFlagTotal; ++i)
		args.push_back(CTypeToString::WaypointDrawFlagsToString(1 << i).duplicate());

	m_cAutoCompleteValues.push_back(args);
	m_cAutoCompleteArguments.push_back(EConsoleAutoCompleteArgValuesForever);
}

TCommandResult CWaypointDrawFlagCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

    if ( argc == 0 )
    {
        const good::string& sType = CTypeToString::WaypointDrawFlagsToString(pClient->iWaypointDrawFlags);
        BULOG_I( pClient->GetEdict(), "Waypoint draw flags: %s.", (sType.size() != 0) ? sType.c_str() : sNone.c_str() );
        return ECommandPerformed;
    }

    // Retrieve flags from string arguments.
    bool bFinished = false;
    TWaypointDrawFlags iFlags = FWaypointDrawNone;

    if ( argc == 1 )
    {
        if ( sNone == argv[0] )
            bFinished = true;
        else if ( sAll == argv[0] )
        {
            iFlags = FWaypointDrawAll;
            bFinished = true;
        }
        else if ( sNext == argv[0] )
        {
            int iNew = (pClient->iWaypointDrawFlags)  ?  pClient->iWaypointDrawFlags<< 1  :  1;
            iFlags = (iNew > FWaypointDrawAll) ? 0 : iNew;
            bFinished = true;
        }
    }

    if ( !bFinished )
    {
        for ( int i=0; i < argc; ++i )
        {
            int iAddFlag = CTypeToString::WaypointDrawFlagsFromString(argv[i]);
            if ( iAddFlag == -1 )
            {
                BULOG_E( pClient->GetEdict(), "Error, invalid draw flag(s). Can be 'none' / 'all' / 'next' or mix of: %s", CTypeToString::WaypointDrawFlagsToString(FWaypointDrawAll).c_str() );
                return ECommandError;
            }
            FLAG_SET(iAddFlag, iFlags);
        }
    }

    pClient->iWaypointDrawFlags = iFlags;
    BULOG_I(pClient->GetEdict(), "Waypoints drawing is %s.", iFlags ? "on" : "off");
    return ECommandPerformed;
}

TCommandResult CWaypointResetCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

    if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

    Vector vOrigin( pClient->GetHead() );
    pClient->iCurrentWaypoint = CWaypoints::GetNearestWaypoint( vOrigin );
    BULOG_I(pClient->GetEdict(), "Current waypoint %d.", pClient->iCurrentWaypoint);

    return ECommandPerformed;
}

TCommandResult CWaypointCreateCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;
	
	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

    if ( !pClient->IsAlive() )
    {
        BULOG_W(pClient->GetEdict(), "Error, you need to be alive to create waypoints (bots can't just fly around you know).");
        return ECommandError;
    }

    TWaypointId id = CWaypoints::Add( pClient->GetHead(), FWaypointNone );
    pClient->iCurrentWaypoint = id;

    // Check if player is crouched.
    float fHeight = pClient->GetPlayerInfo()->GetPlayerMaxs().z - pClient->GetPlayerInfo()->GetPlayerMins().z + 1;
    bool bIsCrouched = ( fHeight < CMod::iPlayerHeight );

    if (pClient->bAutoCreatePaths)
        CWaypoints::CreateAutoPaths(id, bIsCrouched);
    else if ( CWaypoint::IsValid( pClient->iDestinationWaypoint ) )
        CWaypoints::CreatePathsWithAutoFlags( pClient->iDestinationWaypoint, pClient->iCurrentWaypoint, bIsCrouched );

    BULOG_I(pClient->GetEdict(), "Waypoint %d added.", id);

	CItems::MapUnloaded();
	CItems::MapLoaded(false);

    return ECommandPerformed;
}

CWaypointRemoveCommand::CWaypointRemoveCommand()
{
	m_sCommand = "remove";
	m_sHelp = "delete waypoints";
	m_sDescription = "Arguments can be: current / destination / other waypoint id(s)";
	m_iAccessLevel = FCommandAccessWaypoint;

	m_cAutoCompleteArguments.push_back(EConsoleAutoCompleteArgWaypoint);
	m_cAutoCompleteValues.push_back(StringVector());
}

TCommandResult CWaypointRemoveCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

	TWaypointId id = -1;
	if (argc == 0)
		id = pClient->iCurrentWaypoint;
	else if (argc == 1)
	{
		if (sCurrent == argv[0])
			id = pClient->iCurrentWaypoint;
		else if (sDestination == argv[0])
			id = pClient->iDestinationWaypoint;
		else
			sscanf(argv[0], "%d", &id);
	}

    if ( !CWaypoints::IsValid(id) )
    {
        BULOG_W(pClient->GetEdict(), "Error, invalid given or current waypoint (move closer to some waypoint).");
        return ECommandError;
    }

    CWaypoints::Remove(id);
    BULOG_I(pClient->GetEdict(), "Waypoint %d deleted.", id);

    // Invalidate current / destination waypoints for all players.
    for (int i=0; i < CPlayers::Size(); ++i)
    {
        CPlayer* pPlayer = CPlayers::Get(i);
        if ( pPlayer )
        {
            if ( pPlayer->IsBot() )
            {
                pPlayer->iCurrentWaypoint = -1; // Force bot move failure, because path can contain removed waypoint.
                pPlayer->iNextWaypoint = -1;
            }
            else
            {
                CClient* pClient = (CClient*)pPlayer;
                if ( pClient->iCurrentWaypoint == id )
                    pClient->iCurrentWaypoint = -1;
                else if ( pClient->iCurrentWaypoint > id )
                    pClient->iCurrentWaypoint--;
                if ( pClient->iDestinationWaypoint == id )
                    pClient->iDestinationWaypoint = -1;
                else if ( pClient->iDestinationWaypoint > id )
                    pClient->iDestinationWaypoint--;
            }
        }
    }

	CItems::MapUnloaded();
	CItems::MapLoaded(false);

    return ECommandPerformed;
}

TCommandResult CWaypointMoveCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }
	if ( argc > 1 )
	{
		BLOG_W( "Invalid parameters count." );
		return ECommandError;
	}

	TWaypointId id = GetWaypointId( 0, argc, argv, pClient, pClient->iCurrentWaypoint );	
    if ( !CWaypoints::IsValid(id) )
    {
        BULOG_W(pClient->GetEdict(), "Error, invalid waypoint %s (move closer to some waypoint).", argc == 0 ? "current" : argv[0]);
        return ECommandError;
    }

	Vector vOrigin( pClient->GetHead() );
    CWaypoints::Move(id, vOrigin);
    BULOG_I(pClient->GetEdict(), "Set new position for waypoint %d (%d, %d, %d).", id, (int)vOrigin.x, (int)vOrigin.y, (int)vOrigin.z);

	CItems::MapUnloaded();
	CItems::MapLoaded(false);

    return ECommandPerformed;
}

TCommandResult CWaypointAutoCreateCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

    if ( argc == 0 )
    {
        BULOG_I( pClient->GetEdict(), pClient->bAutoCreateWaypoints ? "Auto create waypoints is on." : "Auto create waypoints is off." );
        return ECommandPerformed;
    }

    int iValue = -1;
    if ( argc == 1 )
        iValue = CTypeToString::BoolFromString(argv[0]);

    if ( iValue == -1 )
    {
        BULOG_W(pClient->GetEdict(), "Error, invalid argument (must be 'on' or 'off').");
        return ECommandError;
    }

    pClient->bAutoCreateWaypoints = iValue != 0;
    BULOG_I(pClient->GetEdict(), iValue ? "Auto create waypoints is on." : "Auto create waypoints is off.");
    return ECommandPerformed;
}


TCommandResult CWaypointClearCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

    // Invalidate current / next / destination waypoints for all players.
    for ( int i = 0; i < CPlayers::Size(); ++i )
    {
        CPlayer* pPlayer = CPlayers::Get(i);
        if ( pPlayer )
        {
            pPlayer->iCurrentWaypoint = pPlayer->iNextWaypoint = -1;
            if ( !pPlayer->IsBot() )
                ((CClient*)pPlayer)->iDestinationWaypoint = -1;
        }
    }

    CWaypoints::Clear();
    BULOG_I(pClient->GetEdict(), "All waypoints deleted.");

	CItems::MapUnloaded();
	CItems::MapLoaded(false);

    return ECommandPerformed;
}

TCommandResult CWaypointAddTypeCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

    if ( !CWaypoint::IsValid(pClient->iCurrentWaypoint) )
    {
        BULOG_W(pClient->GetEdict(), "Error, no waypoint nearby to add type (move closer to waypoint).");
        return ECommandError;
    }

    // Retrieve flags from string arguments.
    TWaypointFlags iFlags = FWaypointNone;
    for ( int i=0; i < argc; ++i )
    {
        int iAddFlag = CTypeToString::WaypointFlagsFromString(argv[i]);
        if ( iAddFlag == -1 )
        {
            BULOG_E( pClient->GetEdict(), "Error, invalid waypoint flag: %s. Can be one of: %s", argv[i], CTypeToString::WaypointFlagsToString(FWaypointAll).c_str() );
            return ECommandError;
        }
        FLAG_SET(iAddFlag, iFlags);
    }

    if ( iFlags == FWaypointNone )
    {
        BULOG_E(pClient->GetEdict(), "Error, specify at least one waypoint type. Can be one of: %s.", CTypeToString::WaypointFlagsToString(FWaypointAll).c_str());
        return ECommandError;
    }
    else
    {
        CWaypoint& w = CWaypoints::Get(pClient->iCurrentWaypoint);

        bool bAngle1 = FLAG_SOME_SET(FWaypointCamper | FWaypointSniper | FWaypointArmorMachine | FWaypointHealthMachine | FWaypointButton, w.iFlags);
        bool bAngle2 = FLAG_SOME_SET(FWaypointCamper | FWaypointSniper, w.iFlags);

        bool bWeapon = FLAG_SOME_SET(FWaypointAmmo | FWaypointWeapon, w.iFlags);

        bool bArmor = FLAG_SOME_SET(FWaypointArmor, w.iFlags);
        bool bHealth = FLAG_SOME_SET(FWaypointHealth, w.iFlags);

        if ( (bAngle1 && bWeapon) || ( bAngle2 && (bWeapon || bArmor || bHealth) ) )
        {
            BULOG_W(pClient->GetEdict(), "Error, you can't mix these waypoint types.");
            return ECommandError;
        }

        FLAG_SET(iFlags, w.iFlags);
        BULOG_I(pClient->GetEdict(), "Types %s (%d) added to waypoint %d.", CTypeToString::WaypointFlagsToString(iFlags).c_str(), iFlags, pClient->iCurrentWaypoint);
        return ECommandPerformed;
    }
}

TCommandResult CWaypointRemoveTypeCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

	TWaypointId id = GetWaypointId( 0, argc, argv, pClient, pClient->iCurrentWaypoint );
    if ( CWaypoints::IsValid(id) )
    {
        CWaypoint& w = CWaypoints::Get(id);
        w.iFlags = FWaypointNone;
        BULOG_I(pClient->GetEdict(), "Waypoint %d has no type now.", id);
        return ECommandPerformed;
    }
    else
    {
        BULOG_W(pClient->GetEdict(), "Error, no waypoint nearby to remove type (move closer to waypoint).");
        return ECommandError;
    }
}

CWaypointArgumentCommand::CWaypointArgumentCommand()
{
    m_sCommand = "argument";
	m_sHelp = "set waypoint argument";
	m_sDescription = "Parameters: (waypoint) (key) (value), where key can be angle1 / angle2 / weapon / ammo / health /";
	m_iAccessLevel = FCommandAccessWaypoint;

	StringVector args0, args1, args2;

	args0.push_back(sFirstAngle);
	args0.push_back(sSecondAngle);
	args0.push_back(sButton);

#define NO_ITEMS_AVAILABLE
#ifdef NO_ITEMS_AVAILABLE
	args0.push_back(sWeapon);
	args0.push_back(sAmmo);
	args0.push_back(sHealth);
	args0.push_back(sArmor);
#endif

	for ( TWeaponId weapon = 0; weapon < CWeapons::Size(); ++weapon )
	{
		const CWeaponWithAmmo& cWeapon = *CWeapons::Get(weapon);
		args1.push_back( cWeapon.GetName().duplicate() );
		
		auto ammos = cWeapon.GetBaseWeapon()->aAmmos;
		for (int ammo = 0; ammo < ammos->size(); ++ammo)
			args2.push_back( ammos->at(ammo)->sClassName.duplicate() );
	}

	m_cAutoCompleteArguments.push_back(EConsoleAutoCompleteArgValues);
	m_cAutoCompleteValues.push_back(args0);

	m_cAutoCompleteArguments.push_back(EConsoleAutoCompleteArgValues);
	m_cAutoCompleteValues.push_back(args1);

	m_cAutoCompleteArguments.push_back(EConsoleAutoCompleteArgValues);
	m_cAutoCompleteValues.push_back(args2);
}

TCommandResult CWaypointArgumentCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

    TWaypointId id = pClient->iCurrentWaypoint;
    if ( !CWaypoints::IsValid(id) )
    {
        BULOG_W(pClient->GetEdict(), "Error, no waypoint nearby (move closer to waypoint).");
        return ECommandError;
    }

    CWaypoint& w = CWaypoints::Get(pClient->iCurrentWaypoint);

    bool bAngle1 = FLAG_SOME_SET(FWaypointCamper | FWaypointSniper | FWaypointArmorMachine | FWaypointHealthMachine | FWaypointButton | FWaypointSeeButton, w.iFlags);
    bool bAngle2 = FLAG_SOME_SET(FWaypointCamper | FWaypointSniper, w.iFlags);

    bool bWeapon = FLAG_SOME_SET(FWaypointAmmo | FWaypointWeapon, w.iFlags);

    bool bArmor = FLAG_SOME_SET(FWaypointArmor, w.iFlags);
    bool bHealth = FLAG_SOME_SET(FWaypointHealth, w.iFlags);

    bool bButton = FLAG_SOME_SET(FWaypointButton | FWaypointSeeButton, w.iFlags);

    if ( argc == 0 )
    {
        if ( bWeapon )
		{
			TWeaponId iWeaponId = CWaypoint::GetWeaponId(w.iArgument);
			const CWeaponWithAmmo *pWeapon = CWeapons::Get(iWeaponId);
			BULOG_I( pClient->GetEdict(), "Weapon: %s.", pWeapon == NULL ? "invalid" : pWeapon->GetName() );
			if ( pWeapon && FLAG_SOME_SET(FWaypointAmmo, w.iFlags) )
			{
				bool bIsSecondary; int iAmmo = CWaypoint::GetAmmo(bIsSecondary, w.iArgument);
				auto aAmmos = pWeapon->GetBaseWeapon()->aAmmos[ bIsSecondary ? CWeapon::SECONDARY : CWeapon::PRIMARY ];

				if ( iAmmo < aAmmos.size() )
					BULOG_I(pClient->GetEdict(), "Weapon ammo %s, secondary %s.", aAmmos[iAmmo]->szEngineName, bIsSecondary ? "yes" : "no");
				else
					BULOG_I(pClient->GetEdict(), "Weapon ammo invalid.");
			}
		}
		
        if ( bHealth )
            BULOG_I(pClient->GetEdict(), "Health %d.", CWaypoint::GetHealth(w.iArgument));
        if ( bArmor )
            BULOG_I(pClient->GetEdict(), "Armor %d.", CWaypoint::GetArmor(w.iArgument));
        if ( bButton )
            BULOG_I(pClient->GetEdict(), "Button %d.", CWaypoint::GetButton(w.iArgument));
        if ( bAngle1 )
        {
            QAngle a1; CWaypoint::GetFirstAngle(a1, w.iArgument);
            BULOG_I(pClient->GetEdict(), "Angle 1 (pitch, yaw): (%.0f, %.0f).", a1.x, a1.y);
        }
        if ( bAngle2 )
        {
            QAngle a2; CWaypoint::GetSecondAngle(a2, w.iArgument);
            BULOG_I(pClient->GetEdict(), "Angle 2 (pitch, yaw): (%.0f, %.0f).", a2.x, a2.y);
        }
        return ECommandPerformed;
    }

    if ( sHelp == argv[0] )
    {
        BULOG_I(pClient->GetEdict(), "You can add next arguments:");
        BULOG_I(pClient->GetEdict(), " - 'angle1': set your current angles as waypoint first angles (camper, sniper, machines)");
		BULOG_I(pClient->GetEdict(), " - 'angle2': set your current angles as second angles (camper, sniper)");
#ifdef NO_ITEMS_AVAILABLE
		BULOG_I(pClient->GetEdict(), " - 'weapon': set waypoint's weapon");
		BULOG_I(pClient->GetEdict(), " - 'ammo': set waypoint's ammo");
		BULOG_I(pClient->GetEdict(), " - 'health': set waypoint's health amount");
		BULOG_I(pClient->GetEdict(), " - 'armor': set waypoint's armor amount");
#endif
		BULOG_I(pClient->GetEdict(), " - 'button': set waypoint's button");
		return ECommandPerformed;
    }

    int iArgument = w.iArgument;
    QAngle angClient;
    pClient->GetEyeAngles(angClient);
    CUtil::DeNormalizeAngle(angClient.x);
    CUtil::DeNormalizeAngle(angClient.y);
    BASSERT( -90.0f <= angClient.x && angClient.x <= 90.0f, return ECommandError );

	bool bIsWeapon = sWeapon == argv[0];
	int iWeaponId = EWeaponIdInvalid;

#ifdef NO_ITEMS_AVAILABLE
	if ( bIsWeapon || sAmmo == argv[0] )
    {
		if ( argc < 2 )
        {
            BULOG_W(pClient->GetEdict(), "Error, you must provide weapon name.");
            return ECommandError;
        }
		else if ( bIsWeapon && (argc != 2) )
		{
			BULOG_W(pClient->GetEdict(), "Error, invalid parameters count.");
			return ECommandError;
		}
        if ( !bWeapon )
        {
            BULOG_W(pClient->GetEdict(), "Error, first you need to set waypoint type accordingly (weapon/ammo).");
            return ECommandError;
        }

        if ( bAngle1 || bButton )
        {
            BULOG_W(pClient->GetEdict(), "Error, you can't mix weapon/ammo with angles/buttons.");
            return ECommandError;
        }
        iWeaponId = CWeapons::GetIdFromWeaponName(argv[1]);
        if ( !CWeapons::IsValid(iWeaponId) )
        {
            BULOG_W(pClient->GetEdict(), "Error, invalid weapon name.");
            return ECommandError;
        }
		CWaypoint::SetWeaponId(iWeaponId, iArgument);

		if ( !bIsWeapon ) // Is ammo.
		{
			if ( argc != 3 )
			{
				BULOG_W(pClient->GetEdict(), "Error, you must provide ammo name or invalid parameters count.");
				return ECommandError;
			}
			if ( !FLAG_SOME_SET(FWaypointAmmo, w.iFlags) )
			{
				BULOG_W(pClient->GetEdict(), "Error, first you need to set waypoint type accordingly (ammo).");
				return ECommandError;
			}
			if ( bAngle1 || bButton )
			{
				BULOG_W(pClient->GetEdict(), "Error, you can't mix weapon/ammo with angles/buttons.");
				return ECommandError;
			}

			const CWeapon *pWeapon = CWeapons::Get(iWeaponId)->GetBaseWeapon();
			BASSERT(pWeapon, return ECommandError);

			bool bIsSecondary;
			int iAmmo = pWeapon->GetAmmoIndexFromName(argv[2], bIsSecondary);

			if ( iAmmo < 0 )
			{
				BULOG_W(pClient->GetEdict(), "Error, invalid ammo name.");
				return ECommandError;
			}
			CWaypoint::SetAmmo(iAmmo, bIsSecondary, iArgument);
		}
	}

    else if ( sHealth == argv[0] )
    {
        if ( argc != 2 )
        {
            BULOG_W(pClient->GetEdict(), "Error, you must provide health amount or invalid parameters count.");
            return ECommandError;
        }
		if ( !bHealth )
		{
			BULOG_W(pClient->GetEdict(), "Error, first you need to set waypoint type accordingly (health/health_charger).");
			return ECommandError;
		}
        if ( bAngle2 )
        {
            BULOG_W(pClient->GetEdict(), "Error, you can't mix health with 2 angles.");
            return ECommandError;
        }

        int i1 = -1;
        sscanf(argv[1], "%d", &i1);

        if ( (i1 < 0) || (i1 > 100) )
        {
            BULOG_W(pClient->GetEdict(), "Error, invalid health argument (must be from 0 to 100).");
            return ECommandError;
        }
        CWaypoint::SetHealth(i1, iArgument);
    }

    else if ( sArmor == argv[0] )
    {
        if ( argc != 2 )
        {
            BULOG_W(pClient->GetEdict(), "Error, you must provide armor amount or invalid parameters count.");
            return ECommandError;
        }
		if ( !bArmor )
		{
			BULOG_W(pClient->GetEdict(), "Error, first you need to set waypoint type accordingly (armor/armor_charger).");
			return ECommandError;
		}
        if ( bAngle2 )
        {
            BULOG_W(pClient->GetEdict(), "Error, you can't mix armor with 2 angles.");
            return ECommandError;
        }

        int i1 = -1;
        sscanf(argv[1], "%d", &i1);

        if ( (i1 < 0) || (i1 > 100) )
        {
            BULOG_W(pClient->GetEdict(), "Error, invalid armor argument (must be from 0 to 100).");
            return ECommandError;
        }
        CWaypoint::SetArmor(i1, iArgument);
    }

    else 
#endif

	if ( sButton == argv[0] )
    {
        if ( argc != 2 )
        {
            BULOG_W(pClient->GetEdict(), "Error, you must provide button index  or invalid parameters count.");
            return ECommandError;
        }
        if ( bAngle2 )
        {
            BULOG_W(pClient->GetEdict(), "Error, you can't mix button with 2 angles.");
            return ECommandError;
        }
        if ( !bButton )
        {
            BULOG_W(pClient->GetEdict(), "Error, first you need to set waypoint type accordingly (button/see_button).");
            return ECommandError;
        }

        int i1 = -1;
        sscanf(argv[1], "%d", &i1);

        int iButtonsCount = CItems::GetItems(EItemTypeButton).size();
        if ( (i1 < 0) || (i1 >= iButtonsCount) )
        {
            BULOG_W(pClient->GetEdict(), "Error, invalid button argument (must be from 0 to %d).", iButtonsCount-1);
            return ECommandError;
        }
        CWaypoint::SetButton(i1, iArgument);
    }

    else if ( sFirstAngle == argv[0] )
    {
		if ( argc != 1 && argc != 3 )
		{
			BULOG_W(pClient->GetEdict(), "Error, invalid parameters count.");
			return ECommandError;
		}
        if ( bWeapon )
        {
            BULOG_W(pClient->GetEdict(), "Error, you can't mix weapon/ammo with angles.");
            return ECommandError;
        }
        if ( !bAngle1 )
        {
            BULOG_W(pClient->GetEdict(), "Error, first you need to set waypoint type accordingly (camper/sniper/armor_charger/health_charger).");
            return ECommandError;
        }

        int iPitch = angClient.x;
		int iYaw = angClient.y;

		if ( (CWaypoint::MIN_ANGLE_PITCH <= iPitch) && (iPitch <= CWaypoint::MAX_ANGLE_PITCH) &&
			 (CWaypoint::MIN_ANGLE_YAW <= iYaw) && (iYaw <= CWaypoint::MAX_ANGLE_YAW) )
		{
			CWaypoint::SetFirstAngle(iPitch, iYaw, iArgument);
		}
		else
        {
            BULOG_W(pClient->GetEdict(), "Error, up/down angle (pitch/yaw) must be from -64 to 63 / -128 to 128.");
            return ECommandError;
        }
    }

    else if ( sSecondAngle == argv[0] )
    {
		if ( argc != 1 && argc != 3 )
		{
			BULOG_W(pClient->GetEdict(), "Error, invalid parameters count.");
			return ECommandError;
		}
        if ( bWeapon || bArmor || bHealth )
        {
            BULOG_W(pClient->GetEdict(), "Error, you can't mix weapon/ammo/health/armor with two angles.");
            return ECommandError;
        }
        if ( !bAngle2 )
        {
            BULOG_W(pClient->GetEdict(), "Error, first you need to set waypoint type accordingly (camper/sniper).");
            return ECommandError;
        }

        int iPitch = angClient.x;
        int iYaw = angClient.y;
        if ( (CWaypoint::MIN_ANGLE_PITCH <= iPitch) && (iPitch <= CWaypoint::MAX_ANGLE_PITCH) &&
			 (CWaypoint::MIN_ANGLE_YAW <= iYaw) && (iYaw <= CWaypoint::MAX_ANGLE_YAW) )
		{
			CWaypoint::SetSecondAngle(iPitch, iYaw, iArgument);
        }
		else
        {
			BULOG_W(pClient->GetEdict(), "Error, up/down angle (pitch/yaw) must be from -64 to 63 / -128 to 128.");
			return ECommandError;
        }
    }
	
    else
    {
        BULOG_W(pClient->GetEdict(), "Error, invalid argument %s.", argv[0]);
        return ECommandError;
    }

    w.iArgument = iArgument;
    return ECommandPerformed;
}

TCommandResult CWaypointInfoCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

	for ( int arg = 0; arg < ( argc > 0 ? argc : 1 ); ++arg )
	{
		TWaypointId id = GetWaypointId( arg, argc, argv, pClient, pClient->iCurrentWaypoint );
		if ( CWaypoints::IsValid( id ) )
		{
			CWaypoint& w = CWaypoints::Get( id );
			const good::string& sFlags = CTypeToString::WaypointFlagsToString( w.iFlags );
			BULOG_I( pClient->GetEdict(), "Waypoint id %d: flags %s", id, ( sFlags.size() > 0 ) ? sFlags.c_str() : sNone.c_str() );
			BULOG_I( pClient->GetEdict(), "Origin: %.0f %.0f %.0f", w.vOrigin.x, w.vOrigin.y, w.vOrigin.z );
		}
		else
		{
			BULOG_W( pClient->GetEdict(), "Error, invalid waypoint (try to move closer to some waypoint)." );
			return ECommandError;
		}
	}
	return ECommandPerformed;
}

CWaypointDestinationCommand::CWaypointDestinationCommand()
{
	m_sCommand = "destination";
	m_sHelp = "lock / unlock path 'destination'";
	m_sDescription = "Parameter: (waypoint / unlock). Current waypoint locked as 'destination' if omitted.";

	StringVector args;
	args.push_back( sUnlock );
	args.push_back( sCurrent );
	args.push_back( sDestination );

	m_cAutoCompleteArguments.push_back( EConsoleAutoCompleteArgValues );
	m_cAutoCompleteValues.push_back( args );
}

TCommandResult CWaypointDestinationCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

	TWaypointId id = GetWaypointId( 0, argc, argv, pClient, pClient->iCurrentWaypoint );
    if ( CWaypoint::IsValid(id) )
    {
        pClient->bLockDestinationWaypoint = true;
        pClient->iDestinationWaypoint = id;
        BULOG_I(pClient->GetEdict(), "Lock path 'destination': waypoint %d.", id );
    }
    else
    {
        pClient->bLockDestinationWaypoint = false;
        BULOG_I(pClient->GetEdict(), "Path 'destination' is unlocked, aim at waypoint to change it.");
    }
    return ECommandPerformed;
}

TCommandResult CWaypointSaveCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

	bool bResult = CWaypoints::Save();
    if ( bResult )
        BULOG_I(pClient->GetEdict(), "%d waypoints saved.", CWaypoints::Size());
    else
        BULOG_W(pClient->GetEdict(), "Error, could not save waypoints.");

	CItems::MapUnloaded();
	CItems::MapLoaded(true);

	return bResult ? ECommandPerformed : ECommandError;
}

TCommandResult CWaypointLoadCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

	bool result = CWaypoints::Load();
	if (result)
        BULOG_I(pClient->GetEdict(), "%d waypoints loaded for map %s.", CWaypoints::Size(), CBotrixPlugin::instance->sMapName.c_str() );
    else
        BULOG_E( pClient->GetEdict(), "Error, could not load waypoints for %s.", CBotrixPlugin::instance->sMapName.c_str() );

	CItems::MapUnloaded();
	CItems::MapLoaded(true);

	return result ? ECommandPerformed : ECommandError;
}


CWaypointVisibilityCommand::CWaypointVisibilityCommand()
{
    m_sCommand = "visibility";
    m_sHelp = "defines how to draw visible waypoints";
    m_sDescription = good::string("Can be 'none' / 'all' / 'next' or mix of: ") + CTypeToString::PathDrawFlagsToString(FPathDrawAll);
    m_iAccessLevel = FCommandAccessWaypoint;

	StringVector args;
	args.push_back(sNone);
	args.push_back(sAll);
	args.push_back(sNext);
	for ( int i = 0; i < EPathDrawFlagTotal; ++i )
		args.push_back( CTypeToString::PathDrawFlagsToString( 1 << i ).duplicate() );

	m_cAutoCompleteArguments.push_back(EConsoleAutoCompleteArgValuesForever);
	m_cAutoCompleteValues.push_back(args);
}

TCommandResult CWaypointVisibilityCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

    if ( argc == 0 )
    {
        const good::string& sType = CTypeToString::PathDrawFlagsToString(pClient->iPathDrawFlags);
        BULOG_I( pClient->GetEdict(), "Path draw flags: %s.", (sType.size() > 0) ? sType.c_str() : sNone.c_str());
        return ECommandPerformed;
    }

    // Retrieve flags from string arguments.
    bool bFinished = false;
    TPathDrawFlags iFlags = FPathDrawNone;

    if ( argc == 1 )
    {
        if ( sHelp == argv[0] )
        {
            PrintCommand( pClient->GetEdict() );
            return ECommandPerformed;
        }
        else if ( sNone == argv[0] )
            bFinished = true;
        else if ( sAll == argv[0] )
        {
            iFlags = FPathDrawAll;
            bFinished = true;
        }
        else if ( sNext == argv[0] )
        {
            int iNew = (pClient->iPathDrawFlags)  ?  pClient->iPathDrawFlags << 1  :  1;
            iFlags = (iNew > FPathDrawAll) ? 0 : iNew;
            bFinished = true;
        }
    }

    if ( !bFinished )
    {
        for ( int i=0; i < argc; ++i )
        {
            int iAddFlag = CTypeToString::PathDrawFlagsFromString(argv[i]);
            if ( iAddFlag == -1 )
            {
                BULOG_W( pClient->GetEdict(), "Error, invalid draw flag(s). Can be 'none' / 'all' / 'next' or mix of: %s.", CTypeToString::PathDrawFlagsToString(FWaypointDrawAll).c_str() );
                return ECommandError;
            }
            FLAG_SET(iAddFlag, iFlags);
        }
    }

    pClient->iVisiblesDrawFlags = iFlags;
    BULOG_I(pClient->GetEdict(), "Visible waypoints drawing is %s.", iFlags ? "on" : "off");
    return ECommandPerformed;
}


//----------------------------------------------------------------------------------------------------------------
// Waypoint area commands.
//----------------------------------------------------------------------------------------------------------------
TCommandResult CWaypointAreaRemoveCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

    good::string_buffer sbBuffer(szMainBuffer, iMainBufferSize, false); // Don't deallocate after use.

    if ( argc != 1 )
    {
        BULOG_W(pClient->GetEdict(), "Error, 1 argument for area's name needed.");
        return ECommandError;
    }
	
	good::string sArea(argv[0]);
    StringVector& cAreas = CWaypoints::GetAreas();
    for ( TAreaId iArea = 1; iArea < cAreas.size(); ++iArea ) // Do not take default area in account.
    {
        if ( cAreas[iArea] != sArea )
        {
            for ( TWaypointId iWaypoint = 0; iWaypoint < CWaypoints::Size(); ++iWaypoint )
            {
                CWaypoint& cWaypoint = CWaypoints::Get(iWaypoint);
                if ( cWaypoint.iAreaId > iArea )
                    --cWaypoint.iAreaId;
                else if ( cWaypoint.iAreaId == iArea )
                    cWaypoint.iAreaId = 0; // Set to default.
            }

            cAreas.erase(cAreas.begin() + iArea);
            BULOG_I( pClient->GetEdict(), "Deleted area '%s'.", sbBuffer.c_str() );
            return ECommandPerformed;
        }
    }

    BULOG_W(pClient->GetEdict(), "Error, no area with such name.");
    return ECommandError;
}

TCommandResult CWaypointAreaRenameCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

    good::string_buffer sbBuffer(szMainBuffer, iMainBufferSize, false); // Don't deallocate after use.

    if ( argc < 2 )
    {
        BULOG_W(pClient->GetEdict(), "Error, 2 arguments needed.");
        return ECommandError;
    }

    for ( int i=0; i < argc; ++i )
        sbBuffer << argv[i] << ' ';
    sbBuffer.erase( sbBuffer.size()-1, 1 ); // Erase last space.

    StringVector& cAreas = CWaypoints::GetAreas();
    for ( int i=1; i < cAreas.size(); ++i ) // Do not take default area in account.
    {
        StringVector::value_type& sArea = cAreas[i];
        if ( good::starts_with((good::string)sbBuffer, sArea) )
        {
            sbBuffer.erase(0, sArea.size());
            good::trim(sbBuffer);
            if ( sbBuffer.size() > 0 )
            {
                BULOG_I( pClient->GetEdict(), "Renamed '%s' to '%s'.", sArea.c_str(), sbBuffer.c_str() );
                sArea = sbBuffer.duplicate();
                return ECommandPerformed;
            }
            else
            {
                BULOG_W( pClient->GetEdict(), "Error, can't rename '%s' to '%s'.", cAreas[i].c_str(), sbBuffer.c_str() );
                return ECommandError;
            }
        }
    }

    BULOG_W(pClient->GetEdict(), "Error, no area with such name.");
    return ECommandError;
}

TCommandResult CWaypointAreaSetCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

    TWaypointId iWaypoint = pClient->iCurrentWaypoint;
    int index = 0;

    if ( argc > 0 ) // Check if first argument is a waypoint number.
    {
        char c = argv[0][0];
        bool isNumber = '0' <= c && c <= '9';

        if (isNumber)
            iWaypoint = atoi(argv[index++]);
    }

    if ( index == argc ) // Only waypoint given, show waypoint area.
    {
        const CWaypoint& w = CWaypoints::Get(iWaypoint);
        BULOG_I(pClient->GetEdict(), "Waypoint %d is at area %s (%d).", iWaypoint, CWaypoints::GetAreas()[w.iAreaId].c_str(), w.iAreaId);
        return ECommandPerformed;
    }

    if ( !CWaypoints::IsValid(iWaypoint) )
    {
        BULOG_W(pClient->GetEdict(), "Error, invalid waypoint: %d.", iWaypoint);
        return ECommandError;
    }

    // Concatenate all arguments.
    good::string_buffer sbBuffer(szMainBuffer, iMainBufferSize, false); // Don't deallocate after use.
    for ( ; index < argc; ++index )
        sbBuffer << argv[index] << ' ';
    good::trim(sbBuffer);

    // Check if that area id already exists.
    TAreaId iAreaId = CWaypoints::GetAreaId(sbBuffer);
    if ( iAreaId == EAreaIdInvalid ) // If not, add it.
        iAreaId = CWaypoints::AddAreaName(sbBuffer.duplicate());

    CWaypoints::Get(iWaypoint).iAreaId = iAreaId;

    return ECommandPerformed;
}

TCommandResult CWaypointAreaShowCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	edict_t* pEdict = ( pClient ) ? pClient->GetEdict() : NULL;

    good::string_buffer sbBuffer(szMainBuffer, iMainBufferSize, false); // Don't deallocate after use.

    for ( int i=0; i < CWaypoints::GetAreas().size(); ++i )
        sbBuffer << "   - " <<  CWaypoints::GetAreas()[i] << '\n';
    sbBuffer.erase( sbBuffer.size()-1, 1 ); // Erase last \n.

    BULOG_I( pEdict, "Area names:\n%s", sbBuffer.c_str() );
    return ECommandPerformed;
}

//----------------------------------------------------------------------------------------------------------------
// Paths commands.
//----------------------------------------------------------------------------------------------------------------
/*
TCommandResult CPathSwapCommand::Execute( CClient* pClient, int argc, const char** argv )
{
    if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

    if ( argc > 0 && argv[0] )
    {
        int iValue = pClient->iDestinationWaypoint;
        sscanf(argv[0], "%d", &iValue);
        if ( !CWaypoints::IsValid(iValue) )
        {
            BULOG_W(pClient->GetEdict(), "Error, invalid parameter.");
            return ECommandError;
        }
        pClient->iDestinationWaypoint = iValue;
    }

    if ( !CWaypoint::IsValid(pClient->iDestinationWaypoint) )
    {
        BULOG_W(pClient->GetEdict(), "Error, you need to set 'destination' waypoint first.");
        return ECommandError;
    }

    if ( CWaypoint::IsValid(pClient->iCurrentWaypoint) )
        good::swap(pClient->iDestinationWaypoint, pClient->iCurrentWaypoint);
    else
        pClient->iCurrentWaypoint = pClient->iDestinationWaypoint;

    if ( pClient->iDestinationWaypoint ) // Set angles to aim there.
    {
        CWaypoint& wFrom = CWaypoints::Get(pClient->iCurrentWaypoint);
    }

    CWaypoint& wTo = CWaypoints::Get(pClient->iCurrentWaypoint);
    pClient->GetPlayerInfo()->Set

    BULOG_I(pClient->GetEdict(), "Teleported to %d. Path destination now is %d", pClient->iCurrentWaypoint, pClient->iDestinationWaypoint);

    return ECommandPerformed;
}
*/

TCommandResult CPathDistanceCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	edict_t* pEdict = pClient ? pClient->GetEdict() : NULL;

    if ( argc == 0 )
        BULOG_I( pEdict, "Default path distance: %d.", CWaypoint::iDefaultDistance );
    else if ( argc == 1 )
    {
        if ( pClient == NULL )
        {
            BLOG_W( "Please login to server to execute this command." );
            return ECommandError;
        }

        int i = -1;
        if ( (sscanf(argv[0], "%d", &i) != 1) || (i < 0) || (i > CWaypoint::MAX_RANGE) )
        {
            BULOG_W( pClient->GetEdict(), "Error, number from 0 to %d expected.", CWaypoint::MAX_RANGE );
            return ECommandError;
        }
        CWaypoint::iDefaultDistance = i;
    }
    else
    {
        BULOG_W( pEdict, "Error, invalid parameters count." );
        return ECommandError;
    }

    return ECommandPerformed;
}

CPathDrawCommand::CPathDrawCommand()
{
    m_sCommand = "drawtype";
    m_sHelp = "defines how to draw path";
    m_sDescription = good::string("Can be 'none' / 'all' / 'next' or mix of: ") + CTypeToString::PathDrawFlagsToString(FPathDrawAll);
    m_iAccessLevel = FCommandAccessWaypoint;

	StringVector args;
	args.push_back(sNone);
	args.push_back(sAll);
	args.push_back(sNext);
	for (int i = 0; i < EPathDrawFlagTotal; ++i)
		args.push_back(CTypeToString::PathDrawFlagsToString(1 << i).duplicate());

	m_cAutoCompleteArguments.push_back(EConsoleAutoCompleteArgValuesForever);
	m_cAutoCompleteValues.push_back(args);
}

TCommandResult CPathDrawCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

    if ( argc == 0 )
    {
        const good::string& sType = CTypeToString::PathDrawFlagsToString(pClient->iPathDrawFlags);
        BULOG_I( pClient->GetEdict(), "Path draw flags: %s.", (sType.size() > 0) ? sType.c_str() : sNone.c_str());
        return ECommandPerformed;
    }

    // Retrieve flags from string arguments.
    bool bFinished = false;
    TPathDrawFlags iFlags = FPathDrawNone;

    if ( argc == 1 )
    {
        if ( sHelp == argv[0] )
        {
            PrintCommand( pClient->GetEdict() );
            return ECommandPerformed;
        }
        else if ( sNone == argv[0] )
            bFinished = true;
        else if ( sAll == argv[0] )
        {
            iFlags = FPathDrawAll;
            bFinished = true;
        }
        else if ( sNext == argv[0] )
        {
            int iNew = (pClient->iPathDrawFlags)  ?  pClient->iPathDrawFlags << 1  :  1;
            iFlags = (iNew > FPathDrawAll) ? 0 : iNew;
            bFinished = true;
        }
    }

    if ( !bFinished )
    {
        for ( int i=0; i < argc; ++i )
        {
            int iAddFlag = CTypeToString::PathDrawFlagsFromString(argv[i]);
            if ( iAddFlag == -1 )
            {
                BULOG_W( pClient->GetEdict(), "Error, invalid draw flag(s). Can be 'none' / 'all' / 'next' or mix of: %s.", CTypeToString::PathDrawFlagsToString(FWaypointDrawAll).c_str() );
                return ECommandError;
            }
            FLAG_SET(iAddFlag, iFlags);
        }
    }

    pClient->iPathDrawFlags = iFlags;
    BULOG_I(pClient->GetEdict(), "Path drawing is %s.", iFlags ? "on" : "off");
    return ECommandPerformed;
}

TCommandResult CPathCreateCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

    TWaypointId iPathFrom = -1, iPathTo = -1;
    if (argc == 0)
    {
        iPathFrom = pClient->iCurrentWaypoint;
        iPathTo = pClient->iDestinationWaypoint;
    }
    else if (argc == 1)
    {
        iPathFrom = pClient->iCurrentWaypoint;
		iPathTo = GetWaypointId( 0, argc, argv, pClient, pClient->iCurrentWaypoint );
	}
    else if (argc == 2)
    {
		iPathTo = GetWaypointId( 0, argc, argv, pClient, EWaypointIdInvalid );
		iPathTo = GetWaypointId( 1, argc, argv, pClient, EWaypointIdInvalid );
    }

    if ( !CWaypoints::IsValid(iPathFrom) || !CWaypoints::IsValid(iPathTo) )
    {
        BULOG_W(pClient->GetEdict(), "Error, invalid parameters, 'current' or 'destination' waypoints.");
        return ECommandError;
    }

    TPathFlags iFlags = FPathNone;
    if ( pClient->IsAlive() )
    {
        float fHeight = pClient->GetPlayerInfo()->GetPlayerMaxs().z - pClient->GetPlayerInfo()->GetPlayerMins().z + 1;
        if (fHeight < CMod::iPlayerHeight)
            iFlags = FPathCrouch;
    }

    if ( CWaypoints::AddPath(iPathFrom, iPathTo, 0, iFlags) )
    {
        BULOG_I(pClient->GetEdict(), "Path created: from %d to %d.", iPathFrom, iPathTo);
        return ECommandPerformed;
    }
    else
    {
        BULOG_W(pClient->GetEdict(), "Error creating path from %d to %d (exists or too big distance?).", iPathFrom, iPathTo);
        return ECommandError;
    }
}

TCommandResult CPathRemoveCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

	TWaypointId iPathFrom = -1, iPathTo = -1;
	if ( argc == 0 )
	{
		iPathFrom = pClient->iCurrentWaypoint;
		iPathTo = pClient->iDestinationWaypoint;
	}
	else if ( argc == 1 )
	{
		iPathFrom = pClient->iCurrentWaypoint;
		iPathTo = GetWaypointId( 0, argc, argv, pClient, pClient->iCurrentWaypoint );
	}
	else if ( argc == 2 )
	{
		iPathTo = GetWaypointId( 0, argc, argv, pClient, EWaypointIdInvalid );
		iPathTo = GetWaypointId( 1, argc, argv, pClient, EWaypointIdInvalid );
	}

    if ( !CWaypoints::IsValid(iPathFrom) || !CWaypoints::IsValid(iPathTo) )
    {
        BULOG_W(pClient->GetEdict(), "Error, invalid parameters, 'current' or 'destination' waypoints.");
        return ECommandError;
    }

    if ( CWaypoints::RemovePath(iPathFrom, iPathTo) )
    {
        BULOG_I(pClient->GetEdict(), "Path removed: from %d to %d.", iPathFrom, iPathTo);
        return ECommandPerformed;
    }
    else
    {
        BULOG_W(pClient->GetEdict(), "Error, there is no path from %d to %d.", iPathFrom, iPathTo);
        return ECommandError;
    }
}

TCommandResult CPathAutoCreateCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

    if ( argc == 0 )
    {
        BULOG_I(pClient->GetEdict(), "Waypoint's auto paths creation is: %s.", CTypeToString::BoolToString(pClient->bAutoCreatePaths).c_str());
        return ECommandPerformed;
    }

    int iValue = -1;
    if ( argc == 1 )
        iValue = CTypeToString::BoolFromString(argv[0]);

    if ( iValue == -1 )
    {
        BULOG_W(pClient->GetEdict(), "Error, invalid argument (must be 'on' or 'off').");
        return ECommandError;
    }

    pClient->bAutoCreatePaths = iValue != 0;
    BULOG_I(pClient->GetEdict(), "Waypoint's auto paths creation is: %s.", CTypeToString::BoolToString(pClient->bAutoCreatePaths).c_str());
    return ECommandPerformed;
}

TCommandResult CPathAddTypeCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

    // Get 'destination' and current waypoints IDs.
    TWaypointId iPathFrom = pClient->iCurrentWaypoint;
    TWaypointId iPathTo = pClient->iDestinationWaypoint;

    if ( !CWaypoints::IsValid(iPathFrom) )
    {
        BULOG_W(pClient->GetEdict(), "Error, no waypoint nearby (move closer to waypoint).");
        return ECommandError;
    }
    if ( !CWaypoints::IsValid(iPathTo) )
    {
        BULOG_W(pClient->GetEdict(), "Error, you need to set 'destination' waypoint first.");
        return ECommandError;
    }

    // Retrieve flags from string arguments.
    TPathFlags iFlags = FPathNone;
    for ( int i=0; i < argc; ++i )
    {
        int iAddFlag = CTypeToString::PathFlagsFromString(argv[i]);
        if ( iAddFlag == -1 )
        {
            BULOG_W( pClient->GetEdict(), "Error, invalid path flag: %s. Can be one of: %s", argv[i], CTypeToString::PathFlagsToString(FPathAll).c_str() );
            return ECommandError;
        }
        FLAG_SET(iAddFlag, iFlags);
    }

    if ( iFlags == FPathNone )
    {
        BULOG_W( pClient->GetEdict(), "Error, at least one path flag is needed. Can be one of: %s", CTypeToString::PathFlagsToString(FPathAll).c_str() );
        return ECommandError;
    }

    CWaypointPath* pPath = CWaypoints::GetPath(iPathFrom, pClient->iDestinationWaypoint);
    if ( pPath )
    {
        FLAG_SET(iFlags, pPath->iFlags);
        BULOG_I(pClient->GetEdict(), "Added path types %s (path from %d to %d).", CTypeToString::PathFlagsToString(iFlags).c_str(), iPathFrom, iPathTo);
        return ECommandPerformed;
    }
    else
    {
        BULOG_W(pClient->GetEdict(), "Error, no path from %d to %d.", iPathFrom, iPathTo);
        return ECommandError;
    }
}

TCommandResult CPathRemoveTypeCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

	TWaypointId iPathFrom = -1, iPathTo = -1;
	if ( argc == 0 )
	{
		iPathFrom = pClient->iCurrentWaypoint;
		iPathTo = pClient->iDestinationWaypoint;
	}
	else if ( argc == 1 )
	{
		iPathFrom = pClient->iCurrentWaypoint;
		iPathTo = GetWaypointId( 0, argc, argv, pClient, pClient->iCurrentWaypoint );
	}
	else if ( argc == 2 )
	{
		iPathTo = GetWaypointId( 0, argc, argv, pClient, EWaypointIdInvalid );
		iPathTo = GetWaypointId( 1, argc, argv, pClient, EWaypointIdInvalid );
	}

    if ( !CWaypoints::IsValid(iPathFrom) || !CWaypoints::IsValid(iPathTo) )
    {
        BULOG_W(pClient->GetEdict(), "Error, invalid parameters, 'current' or 'destination' waypoints.");
        return ECommandError;
    }

    CWaypointPath* pPath = CWaypoints::GetPath(iPathFrom, iPathTo);
    if ( pPath )
    {
        pPath->iFlags = FPathNone;
        BULOG_I(pClient->GetEdict(), "Removed all types for path from %d to %d.", iPathFrom, pClient->iDestinationWaypoint);
        return ECommandPerformed;
    }
    else
    {
        BULOG_W(pClient->GetEdict(), "Error, no path from %d to %d.", iPathFrom, pClient->iDestinationWaypoint);
        return ECommandError;
    }
}

TCommandResult CPathArgumentCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

    if ( !CWaypoints::IsValid(pClient->iCurrentWaypoint) || !CWaypoints::IsValid(pClient->iDestinationWaypoint) )
    {
        BULOG_W(pClient->GetEdict(), "Error, invalid 'current' or 'destination' waypoints.");
        return ECommandError;
    }

    CWaypointPath* pPath = CWaypoints::GetPath(pClient->iCurrentWaypoint, pClient->iDestinationWaypoint);
    if ( pPath == NULL )
    {
        BULOG_W(pClient->GetEdict(), "Error, no path from %d to %d.", pClient->iCurrentWaypoint, pClient->iDestinationWaypoint);
        return ECommandError;
    }

    if ( argc == 0 )
    {
        BULOG_I( pClient->GetEdict(), "Path (from %d to %d) action time %d, action duration %d. Time in deciseconds.",
                        pClient->iCurrentWaypoint, pClient->iDestinationWaypoint, GET_1ST_BYTE(pPath->iArgument), GET_2ND_BYTE(pPath->iArgument) );
        return ECommandPerformed;
    }
    else
    {
        TWaypointId iFirst = -1, iSecond = -1;
        if ( argc == 2 )
        {
            sscanf(argv[0], "%d", &iFirst);
            sscanf(argv[1], "%d", &iSecond);
        }

        if ( iFirst < 0 || iSecond < 0 || (iFirst & ~0xFF) || (iSecond & ~0xFF) )
        {
            BULOG_W(pClient->GetEdict(), "Error, invalid parameters, time must be from 0 to 256.");
            return ECommandError;
        }

        pPath->iArgument = iFirst | (iSecond << 8);
        BULOG_I( pClient->GetEdict(), "Set path (from %d to %d) action time %d, action duration %d. Time in deciseconds.",
                        pClient->iCurrentWaypoint, pClient->iDestinationWaypoint, GET_1ST_BYTE(pPath->iArgument), GET_2ND_BYTE(pPath->iArgument) );
        return ECommandPerformed;
    }
}

TCommandResult CPathInfoCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

    TWaypointId iPathFrom = -1, iPathTo = -1;
    if (argc == 0)
    {
        iPathFrom = pClient->iCurrentWaypoint;
        iPathTo = pClient->iDestinationWaypoint;
    }
    else if (argc == 1)
    {
        iPathFrom = pClient->iCurrentWaypoint;
        sscanf(argv[0], "%d", &iPathTo);
    }
    else if (argc == 2)
    {
        sscanf(argv[0], "%d", &iPathFrom);
        sscanf(argv[1], "%d", &iPathTo);
    }

    if ( !CWaypoints::IsValid(iPathFrom) || !CWaypoints::IsValid(iPathTo) )
    {
        BULOG_W(pClient->GetEdict(), "Error, invalid parameters, 'current' or 'destination' waypoints.");
        return ECommandError;
    }

    CWaypointPath* pPath = CWaypoints::GetPath(iPathFrom, iPathTo);
    if ( pPath )
    {
        if ( pPath->HasDemo() )
            BULOG_I(pClient->GetEdict(), "Path (from %d to %d) has demo: id %d.", iPathFrom, iPathTo, pPath->DemoNumber());
        else
        {
            const good::string& sFlags = CTypeToString::PathFlagsToString(pPath->iFlags);
            BULOG_I( pClient->GetEdict(), "Path (from %d to %d) has flags: %s.", iPathFrom, iPathTo,
                            (sFlags.size() > 0) ? sFlags.c_str() : sNone.c_str() );
            if ( FLAG_SOME_SET(FPathDoor, pPath->iFlags) )
            {
                if ( pPath->iArgument )
                    BULOG_I( pClient->GetEdict(), "Door %d.", pPath->iArgument );
                else
                    BULOG_I( pClient->GetEdict(), "Door not set." );
            }
            else if ( FLAG_SOME_SET(FPathJump | FPathCrouch | FPathBreak, pPath->iFlags) )
                BULOG_I( pClient->GetEdict(), "Path action time %d, action duration %d. Time in deciseconds.", GET_1ST_BYTE(pPath->iArgument), GET_2ND_BYTE(pPath->iArgument) );
        }
        return ECommandPerformed;
    }
    else
    {
        BULOG_W(pClient->GetEdict(), "Error, no path from %d to %d.", iPathFrom, pClient->iDestinationWaypoint);
        return ECommandError;
    }
}



//----------------------------------------------------------------------------------------------------------------
// Bots commands.
//----------------------------------------------------------------------------------------------------------------
TCommandResult AllowOrForbid( bool bForbid, CClient* pClient, int argc, const char** argv )
{
    edict_t* pEdict = pClient ? pClient->GetEdict() : NULL;

    if ( argc == 0 ) // Print all weapons.
    {
        for ( TWeaponId i=0; i < CWeapons::Size(); ++i )
        {
            const CWeapon* pWeapon = CWeapons::Get(i)->GetBaseWeapon();
            BULOG_I(pEdict, "%s is %s.", pWeapon->pWeaponClass->sClassName.c_str(), pWeapon->bForbidden ? "forbidden" : "allowed" );
        }
    }
    else if ( (argc == 1) && (sAll == argv[0]) ) // Apply to all weapons.
    {
        for ( TWeaponId i=0; i < CWeapons::Size(); ++i )
        {
            const CWeapon* pWeapon = CWeapons::Get(i)->GetBaseWeapon();
            ((CWeapon*)pWeapon)->bForbidden = bForbid;
            BULOG_I(pEdict, "%s is %s.", pWeapon->pWeaponClass->sClassName.c_str(), pWeapon->bForbidden ? "forbidden" : "allowed" );
        }
    }
    else
    {
        for ( int i=0; i < argc; ++i )
        {
            TWeaponId iWeaponId = CWeapons::GetIdFromWeaponName( argv[i] );
            if ( CWeapons::IsValid(iWeaponId) )
            {
                const CWeapon* pWeapon = CWeapons::Get(iWeaponId)->GetBaseWeapon();
                ((CWeapon*)pWeapon)->bForbidden = bForbid;
                BULOG_I(pEdict, "%s is %s.", argv[i], bForbid ? "forbidden" : "allowed" );
            }
            else
                BULOG_W(pEdict, "Warning, no such weapon: %s, skipping.", argv[i]);
        }
    }
    return ECommandPerformed;
}

TCommandResult CBotWeaponAddCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	edict_t* pEdict = ( pClient ) ? pClient->GetEdict() : NULL;

    if ( argc < 2 )
    {
        BULOG_W( pEdict, "Invalid parameters count." );
        return ECommandError;
    }

    good::string sName( argv[0] );
    bool bAll = ( sName == "all" );

    for ( TPlayerIndex i = 0; i < CPlayers::Size(); ++i )
    {
        CPlayer* pPlayer = CPlayers::Get(i);
        if ( pPlayer && pPlayer->IsBot() )
        {
            good::string sBotName = pPlayer->GetName();
            if ( bAll || good::starts_with(sBotName, sName) )
                for ( int iWeapon = 1; iWeapon < argc; ++iWeapon )
                    ((CBot*)pPlayer)->AddWeapon(argv[iWeapon]);
        }
    }

    return ECommandPerformed;
}

TCommandResult CBotWeaponAllowCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	return AllowOrForbid( false, pClient, argc, argv );
}

TCommandResult CBotWeaponForbidCommand::Execute( CClient* pClient, int argc, const char** argv )
{
    return AllowOrForbid(true, pClient, argc, argv);
}

TCommandResult CBotWeaponRemoveCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	edict_t* pEdict = ( pClient ) ? pClient->GetEdict() : NULL;

    if ( argc != 1 )
    {
        BULOG_W( pEdict, "Invalid parameters count." );
        return ECommandError;
    }

    good::string sName( argv[0] );
    bool bAll = ( sName == "all" );

    for ( TPlayerIndex i = 0; i < CPlayers::Size(); ++i )
    {
        CPlayer* pPlayer = CPlayers::Get(i);
        if ( pPlayer && pPlayer->IsBot() )
        {
            good::string sBotName = pPlayer->GetName();
            if ( bAll || good::starts_with(sBotName, sName) )
                ((CBot*)pPlayer)->RemoveWeapons();
        }
    }

    return ECommandPerformed;
}

TCommandResult CBotWeaponUnknownCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	edict_t* pEdict = ( pClient ) ? pClient->GetEdict() : NULL;

    bool bAssume;

    if ( argc == 1 )
    {
        good::string sArg( argv[0] );
        if ( sArg == "melee" )
            bAssume = true;
        else if ( sArg == "ranged" )
            bAssume = false;
        else
        {
            BULOG_W( pEdict, "Invalid parameter: %s. Should be 'melee' or 'ranged'", argv[0] );
            return ECommandError;
        }
    }
    else
    {
        BULOG_W( pEdict, "Invalid parameters count." );
        return ECommandError;
    }

    CBot::bAssumeUnknownWeaponManual = bAssume;
    return ECommandPerformed;
}

CBotAddCommand::CBotAddCommand()
{
	m_sCommand = "add";
	m_sHelp = "add bot";
	if (CMod::aClassNames.size())
		m_sDescription = "Optional parameters: (intelligence) (team) (class) (bot-name).";
	else
		m_sDescription = "Optional parameters: (intelligence) (team) (bot-name).";
	m_iAccessLevel = FCommandAccessBot;

	StringVector args1;
	args1.push_back( sRandom );
	for ( TBotIntelligence i = 0; i < EBotIntelligenceTotal; ++i )
		args1.push_back( CTypeToString::IntelligenceToString( i ).duplicate() );
	m_cAutoCompleteArguments.push_back( EConsoleAutoCompleteArgValues );
	m_cAutoCompleteValues.push_back( args1 );

	StringVector args2;
	for ( TBotIntelligence i = 0; i < CMod::aTeamsNames.size(); ++i )
		args2.push_back( CMod::aTeamsNames[ i ].duplicate() );
	m_cAutoCompleteArguments.push_back( EConsoleAutoCompleteArgValues );
	m_cAutoCompleteValues.push_back( args2 );

	if ( CMod::aClassNames.size() > 0 )
	{
		StringVector args3;
		for ( TBotIntelligence i = 0; i < CMod::aClassNames.size(); ++i )
			args3.push_back( CMod::aClassNames[i].duplicate() );
		m_cAutoCompleteArguments.push_back( EConsoleAutoCompleteArgValues );
		m_cAutoCompleteValues.push_back( args3 );
	}

	StringVector args0;
	args0.push_back( "bot-name" );
	m_cAutoCompleteArguments.push_back( EConsoleAutoCompleteArgValues );
	m_cAutoCompleteValues.push_back( args0 );
}

TCommandResult CBotAddCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	edict_t* pEdict = ( pClient ) ? pClient->GetEdict() : NULL;

	int iArg = 0;
	
    // 1st argument: intelligence.
    TBotIntelligence iIntelligence = -1;
    if ( argc > iArg )
    {
        good::string sIntelligence( argv[iArg++] );
        iIntelligence = CTypeToString::IntelligenceFromString(sIntelligence);
        if ( (iIntelligence == -1) && (sIntelligence != sRandom) )
        {
			BULOG_W( pEdict, "Invalid bot intelligence: %s.", sIntelligence.c_str() );
            BULOG_W( pEdict, "  Must be one of: fool stupied normal smart pro." );
            // TODO: CTypeToString::AllIntelligences()
            return ECommandError;
        }
    }

    // 2nd argument: team.
    TTeam iTeam = CBot::iDefaultTeam;
    if ( argc > iArg )
    {
        iTeam = CTypeToString::TeamFromString(argv[iArg]);
        if ( iTeam == -1 )
        {
            BULOG_W(pEdict, "Invalid team: %s.", argv[iArg]);
            BULOG_W( pEdict, "  Must be one of: %s.", CTypeToString::TeamFlagsToString(-1).c_str() );
            return ECommandError;
        }
        iArg++;
    }

    // 3rd argument: class, but mod can have no classes.
    TClass iClass = CBot::iDefaultClass;
    if ( CMod::aClassNames.size() && (argc > iArg) )
    {
        good::string sClass( argv[iArg] );
        iClass = CTypeToString::ClassFromString(sClass);
        if ( (iClass == -1) && (sClass != "random") )
        {
            BULOG_W(pEdict, "Invalid class: %s.", argv[iArg]);
            BULOG_W( pEdict, "  Must be one of: %s.", CTypeToString::ClassFlagsToString(-1).c_str() );
            return ECommandError;
        }
        iArg++;
    }

	// 4th argument: name.
	const char* szName = ( argc > iArg ) ? argv[iArg++] : NULL;

    CPlayer* pBot = CPlayers::AddBot(szName, iTeam, iClass, iIntelligence, argc-iArg, &argv[iArg]);
    if ( pBot )
    {
        BULOG_I( pEdict, "Bot added: %s.", pBot->GetName() );
        return ECommandPerformed;
    }
    else
    {
        const good::string& sLastError = CPlayers::GetLastError();
        if ( sLastError.size() )
            BULOG_W( pEdict, sLastError.c_str() );
        return ECommandError;
    }
}

TCommandResult CBotCommandCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	edict_t* pEdict = ( pClient ) ? pClient->GetEdict() : NULL;

    if ( argc < 2 )
    {
        BULOG_W( pEdict,"Error, invalid parameters count." );
        return ECommandError;
    }
    
	const char *szCmd = argv[0];
	for ( int iArg = 1; iArg < argc; ++iArg )
	{
		good::string sName = argv[iArg];
		bool bAll = ( sName == sAll );

		for ( TPlayerIndex i = 0; i < CPlayers::Size(); ++i )
		{
			CPlayer* pPlayer = CPlayers::Get( i );
			if ( pPlayer && pPlayer->IsBot() && 
				( bAll || good::starts_with( good::string( pPlayer->GetName() ), sName ) ) )
			{
				CBot* pBot = (CBot*)pPlayer;
				pBot->ConsoleCommand( szCmd );
			}
		}

		if ( bAll )
			break;
    }
    return ECommandPerformed;
}

TCommandResult CBotKickCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	edict_t* pEdict = ( pClient ) ? pClient->GetEdict() : NULL;

    if ( argc == 0 )
    {
        if ( !CPlayers::KickRandomBot() )
            BULOG_W(pEdict,"Error, no bots to kick.");
    }
    else
    {
		for ( int iArg = 0; iArg < argc; ++iArg )
		{
			good::string sName( argv[iArg] );
			bool bAll = ( sName == sAll );

			for ( TPlayerIndex i = 0; i < CPlayers::Size(); ++i )
			{
				CPlayer* pPlayer = CPlayers::Get( i );
				if ( pPlayer && pPlayer->IsBot() &&
					( bAll || good::starts_with( good::string( pPlayer->GetName() ), sName ) ) )
					CPlayers::KickBot( pPlayer );
			}
			if ( bAll )
				break;
		}
    }
    return ECommandPerformed;
}

TCommandResult CBotDebugCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	edict_t* pEdict = ( pClient ) ? pClient->GetEdict() : NULL;

	if ( argc < 2 )
	{
		BULOG_W( pEdict, "Error, invalid parameters count." );
		return ECommandError;
	}

	int bDebug = CTypeToString::BoolFromString( argv[0] );
	if ( bDebug == -1 )
	{
		BULOG_W( pEdict, "Error, invalid parameter %s, should be 'on' or 'off'.", argv[0] );
		return ECommandError;
	}

	bool bSomeone = false;
	for ( int iArg = 1; iArg < argc; ++iArg )
	{
		good::string sName = argv[iArg];
		bool bAll = ( sName == sAll );

        for ( TPlayerIndex i = 0; i < CPlayers::Size(); ++i )
        {
            CPlayer* pPlayer = CPlayers::Get(i);
            if ( pPlayer && pPlayer->IsBot() &&
                 (bAll || good::starts_with(good::string(pPlayer->GetName()), sName)) )
            {
                bSomeone = true;
                bool bDebuging = (bDebug == -1) ? !((CBot*)pPlayer)->IsDebugging() : (bDebug != 0);
                ((CBot*)pPlayer)->SetDebugging( bDebuging );
                BULOG_I( pEdict, "%s bot %s.", bDebuging ? "Debugging" : "Not debugging", pPlayer->GetName() );
            }
        }
    }
    if ( !bSomeone )
    {
        BULOG_W( pEdict, "Error, no matched bots." );
        return ECommandError;
    }
    return ECommandPerformed;
}

TCommandResult CBotConfigQuotaCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	edict_t* pEdict = ( pClient ) ? pClient->GetEdict() : NULL;

    if ( argc == 0 )
    {
        if ( CPlayers::fPlayerBotRatio > 0.0f )
            BULOG_I( pEdict, "Player-Bot ratio is %.1f.", CPlayers::fPlayerBotRatio );
        else
            BULOG_I( pEdict, "Bots+Players amount: %d.", CPlayers::iBotsPlayersCount );
    }
    else if ( argc == 1 )
    {
        StringVector aSplit(2);
        good::split( good::string(argv[0]), aSplit, '-' );
        int iArg[2] = {-1, -1};
        if ( aSplit.size() > 2 )
        {
            BULOG_W( pEdict, "Error, invalid argument: %s.", argv[0] );
            return ECommandError;
        }
        for ( int i=0; i < aSplit.size(); ++i )
            if ( (sscanf(aSplit[i].c_str(), "%d", &iArg[i]) != 1) || (iArg[i] <= 0) ||
                 (CBotrixPlugin::instance->bMapRunning && (iArg[i] > CPlayers::Size())) )
            {
                BULOG_W( pEdict, "Error, invalid argument: %s.", aSplit[i].c_str() );
                BULOG_W( pEdict, "  Should be a number from 0 to %d.", CPlayers::Size() );
                return ECommandError;
            }

        if ( aSplit.size() == 2 )
        {
            if ( iArg[0] )
                CPlayers::fPlayerBotRatio = (float)iArg[1]/(float)iArg[0];
            BULOG_I( pEdict, "Player-Bot ratio is %.1f.", CPlayers::fPlayerBotRatio );
        }
        else
        {
            CPlayers::fPlayerBotRatio = 0.0f;
            CPlayers::iBotsPlayersCount = iArg[0];
            BULOG_I( pEdict, "Bots+Players amount: %d.", CPlayers::iBotsPlayersCount );
        }

        CPlayers::CheckBotsCount();
    }
    else
    {
        BULOG_W( pEdict, "Error, invalid parameters count." );
        return ECommandError;
    }
    return ECommandPerformed;
}

TCommandResult CBotConfigIntelligenceCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	edict_t* pEdict = ( pClient ) ? pClient->GetEdict() : NULL;

    TCommandResult iResult = ECommandPerformed;
    if ( argc == 0 )
    {
    }
    else if ( argc < 3 )
    {
        TBotIntelligence iIntelligenceMin = CTypeToString::IntelligenceFromString(argv[0]);
        TBotIntelligence iIntelligenceMax = (argc == 2) ? CTypeToString::IntelligenceFromString(argv[1]) : EBotPro;
        if ( (iIntelligenceMin == -1) || (iIntelligenceMax == -1) )
        {
            BULOG_W( pEdict, "Error, invalid intelligence: %s.", (iIntelligenceMin == -1) ? argv[0] : argv[1] );
            BULOG_W( pEdict, "Can be one of: random fool stupied normal smart pro" );
            return ECommandError;
        }
        else if ( iIntelligenceMin > iIntelligenceMax )
        {
            BULOG_W( pEdict, "Error, min should be <= max." );
            return ECommandError;
        }
        else 
        {
            CBot::iMinIntelligence = iIntelligenceMin;
            CBot::iMaxIntelligence = iIntelligenceMax;
        }
    }
    else
    {
        BULOG_W( pEdict, "Error, invalid parameters count." );
        return ECommandError;
    }
    BULOG_I( pEdict, "Bot's intelligence: min %s, max %s.", CTypeToString::IntelligenceToString(CBot::iMinIntelligence).c_str(),
                CTypeToString::IntelligenceToString(CBot::iMaxIntelligence).c_str() );
    return iResult;
}

TCommandResult CBotConfigTeamCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	edict_t* pEdict = ( pClient ) ? pClient->GetEdict() : NULL;

    if ( argc == 1 )
    {
        TTeam iTeam = CTypeToString::TeamFromString(argv[0]);
        if ( iTeam == -1 && sRandom != argv[0] )
        {
            BULOG_W( pEdict, "Error, invalid team: %s.", argv[0] );
            BULOG_W( pEdict, "Can be one of: %s", CTypeToString::TeamFlagsToString(-1).c_str() );
            return ECommandError;
        }
        else
            CBot::iDefaultTeam = iTeam;
    }
    else
    {
        BULOG_W( pEdict, "Error, invalid parameters count." );
        return ECommandError;
    }

    BULOG_I( pEdict, "Bot's default team: %s.", CTypeToString::TeamToString(CBot::iDefaultTeam).c_str() );
    return ECommandPerformed;
}

TCommandResult CBotConfigChangeClassCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	edict_t* pEdict = ( pClient ) ? pClient->GetEdict() : NULL;

    TCommandResult iResult = ECommandPerformed;
    if ( argc == 0 )
    {
    }
    else if ( argc == 1 )
    {
        int i = -1;
        if ( (sscanf(argv[0], "%d", &i) != 1) || (i < 0) )
        {
            BULOG_W( pEdict, "Error, invalid number: %s.", argv[0] );
            return ECommandError;
        }
        CBot::iChangeClassRound = i;
    }
    else
    {
        BULOG_W( pEdict, "Error, invalid parameters count." );
        iResult = ECommandError;
    }

    if ( CBot::iChangeClassRound )
        BULOG_I( pEdict, "Bots will change their class every %d rounds.", CBot::iChangeClassRound );
    else
        BULOG_I( pEdict, "Bots won't change their class." );
    return iResult;
}

TCommandResult CBotConfigClassCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	edict_t* pEdict = ( pClient ) ? pClient->GetEdict() : NULL;

    TCommandResult iResult = ECommandPerformed;
    if ( argc == 0 )
    {
        const char* szClass = (CBot::iDefaultClass == -1)
            ? "random"
            : CTypeToString::ClassToString(CBot::iDefaultClass).c_str();
        BULOG_I( pEdict, "Bot's default class: %s.", szClass );
    }
    else if ( argc == 1 )
    {
        good::string sClass( argv[0] );
        TClass iClass = CTypeToString::ClassFromString(sClass);
        if ( (iClass == -1) && (sClass != "random") )
        {
            BULOG_W( pEdict, "Error, invalid class: %s.", argv[0] );
            BULOG_W( pEdict, "Can be one of: random %s.", CTypeToString::ClassFlagsToString(-1).c_str() );
            iResult = ECommandError;
        }
        else
        {
            BULOG_I( pEdict, "Bot's default class: %s.", argv[0] );
            CBot::iDefaultClass = iClass;
        }
    }
    else
    {
        BULOG_W( pEdict, "Error, invalid parameters count." );
        iResult = ECommandError;
    }
    return iResult;
}

TCommandResult CBotConfigSuicideCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	edict_t* pEdict = ( pClient ) ? pClient->GetEdict() : NULL;

    if ( argc == 0 )
    {
    }
    else if ( argc == 1 )
    {
        float f = -1.0f;
        if ( (sscanf(argv[0], "%f", &f) != 1) || (f < 0) )
        {
            BULOG_W( pEdict, "Error, invalid argument: %s.", argv[0] );
            return ECommandError;
        }

        CBot::fInvalidWaypointSuicideTime = f;
    }
    else
    {
        BULOG_W( pEdict, "Error, invalid parameters count." );
        return ECommandError;
    }

    if ( CBot::fInvalidWaypointSuicideTime )
        BULOG_I( pEdict, "Bot's suicide time: %.1f.", CBot::fInvalidWaypointSuicideTime );
    else
        BULOG_I( pEdict, "Bot will not commit suicide." );
    return ECommandPerformed;
}

TCommandResult CBotConfigStrategyFlagsCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	edict_t* pEdict = ( pClient ) ? pClient->GetEdict() : NULL;

    if ( argc != 0 )
    {
        TFightStrategyFlags iFlags = 0;
        for ( int i = 0; i < argc; ++i )
        {
            TFightStrategyFlags iFlag = CTypeToString::StrategyFlagsFromString(argv[i]);
            if ( iFlag == -1 )
            {
                BULOG_W( pEdict, "Error, invalid strategy flag: %s.", argv[i] );
                BULOG_W( pEdict, "Can be one of: %s.",
                         CTypeToString::StrategyFlagsToString(FFightStrategyAll).c_str() );
                return ECommandError;
            }
            FLAG_SET(iFlag, iFlags);
        }
        CBot::iDefaultFightStrategy = iFlags;
    }

    BULOG_I( pEdict, "Bot's strategy flags: %s.",
             CTypeToString::StrategyFlagsToString(CBot::iDefaultFightStrategy).c_str() );
    return ECommandPerformed;
}

TCommandResult CBotConfigStrategySetCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	edict_t* pEdict = ( pClient ) ? pClient->GetEdict() : NULL;

    if ( argc == 0 )
    {
    }
    else if ( argc == 2 )
    {
        TFightStrategyArg iArg = CTypeToString::StrategyArgFromString(argv[0]);
        if ( iArg == -1 )
        {
            BULOG_W( pEdict, "Error, invalid strategy argument: %s.", argv[0] );
            BULOG_W( pEdict, "Can be one of: %s.", CTypeToString::StrategyArgs().c_str() );
            return ECommandError;
        }
        int iArgValue;
        int iCount = sscanf(argv[1], "%d", &iArgValue);
        if ( (iCount != 1) || (iArgValue < 0) )
        {
            BULOG_W( pEdict, "Error, invalid number: %s.", argv[1] );
            return ECommandError;
        }
        if ( iArgValue > CUtil::iMaxMapSize )
            iArgValue = CUtil::iMaxMapSize;
        switch ( iArg )
        {
        case EFightStrategyArgNearDistance:
            CBot::fNearDistanceSqr = SQR(iArgValue);
            break;
        case EFightStrategyArgFarDistance:
            CBot::fFarDistanceSqr = SQR(iArgValue);
            break;
        default:
            GoodAssert(false);
        }
    }
    else
    {
        BULOG_W( pEdict, "Error, invalid parameters count." );
        return ECommandError;
    }

    BULOG_I( pEdict, "Bot's strategy arguments:" );
    BULOG_I( pEdict, "  %s = %d", CTypeToString::StrategyArgToString(EFightStrategyArgNearDistance).c_str(),
             (int)FastSqrt(CBot::fNearDistanceSqr) );
    BULOG_I( pEdict, "  %s = %d", CTypeToString::StrategyArgToString(EFightStrategyArgFarDistance).c_str(),
             (int)FastSqrt(CBot::fFarDistanceSqr) );
    return ECommandPerformed;
}

CBotDrawPathCommand::CBotDrawPathCommand()
{
	m_sCommand = "drawpath";
	m_sHelp = "defines how to draw bot's path";
	m_sDescription = good::string("Can be 'none' / 'all' / 'next' or mix of: ") + CTypeToString::PathDrawFlagsToString(FPathDrawAll);
	m_iAccessLevel = FCommandAccessBot;

	StringVector args;
	args.push_back(sNone);
	args.push_back(sAll);
	args.push_back(sNext);
	for (int i = 0; i < EPathDrawFlagTotal; ++i)
		args.push_back(CTypeToString::PathDrawFlagsToString(1 << i).duplicate());
	m_cAutoCompleteArguments.push_back(EConsoleAutoCompleteArgValuesForever);
	m_cAutoCompleteValues.push_back(args);
}

TCommandResult CBotDrawPathCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	edict_t* pEdict = ( pClient ) ? pClient->GetEdict() : NULL;

    if ( argc == 0 )
    {
        const good::string& sTypes = CTypeToString::PathDrawFlagsToString(CWaypointNavigator::iPathDrawFlags);
        BULOG_I( pEdict, "Bot's path draw flags: %s.", (sTypes.size() > 0) ? sTypes.c_str() : sNone.c_str() );
        return ECommandPerformed;
    }

    // Retrieve flags from string arguments.
    bool bFinished = false;
    TPathDrawFlags iFlags = FPathDrawNone;

    if ( argc == 1 )
    {
        if ( sHelp == argv[0] )
        {
            PrintCommand( pClient->GetEdict() );
            return ECommandPerformed;
        }
        else if ( sNone == argv[0] )
            bFinished = true;
        else if ( sAll == argv[0] )
        {
            iFlags = FPathDrawAll;
            bFinished = true;
        }
        else if ( sNext == argv[0] )
        {
            int iNew = (CWaypointNavigator::iPathDrawFlags)  ?  CWaypointNavigator::iPathDrawFlags << 1  :  1;
            iFlags = (iNew > FPathDrawAll) ? 0 : iNew;
            bFinished = true;
        }
    }

    if ( !bFinished )
    {
        for ( int i=0; i < argc; ++i )
        {
            int iAddFlag = CTypeToString::PathDrawFlagsFromString(argv[i]);
            if ( iAddFlag == -1 )
            {
                BULOG_W( pEdict, "Error, invalid draw type(s). Can be 'none' / 'all' / 'next' or mix of: %s", CTypeToString::PathDrawFlagsToString(FPathDrawAll).c_str() );
                return ECommandError;
            }
            FLAG_SET(iAddFlag, iFlags);
        }
    }

    CWaypointNavigator::iPathDrawFlags = iFlags;
    BULOG_I(pEdict, "Bot's path drawing is %s.", iFlags ? "on" : "off");
    return ECommandPerformed;
}

TCommandResult CBotAllyCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	edict_t* pEdict = ( pClient ) ? pClient->GetEdict() : NULL;

	if ( argc < 3 )
	{
		BULOG_W( pEdict, "Error, invalid parameter count." );
		return ECommandError;
	}
    good::string sPlayer(argv[0]);
    bool bAllPlayers = (sPlayer == sAll);
    int iAllyOnOff = CTypeToString::BoolFromString(argv[1]);
    if ( iAllyOnOff == -1 )
    {
        BULOG_W( pEdict, "Error, invalid parameter %s, should be 'on' or 'off'.", argv[1] );
        return ECommandError;
    }

	for ( int arg = 2; arg < argc; ++arg )
	{
		good::string sBot( argv[arg] );
		bool bAllBots = ( sBot == sAll );

		for ( TPlayerIndex i = 0; i < CPlayers::Size(); ++i )
		{
			CPlayer* pBot = CPlayers::Get( i );
			if ( pBot && pBot->IsBot() &&
				( bAllBots || good::starts_with( good::string( pBot->GetName() ), sBot ) ) )
			{
				for ( TPlayerIndex j = 0; j < CPlayers::Size(); ++j )
				{
					CPlayer* pPlayer = CPlayers::Get( j );
					if ( pPlayer && ( i != j ) &&
						( bAllPlayers || good::starts_with( good::string( pPlayer->GetName() ), sPlayer ) ) )
					{
						bool bAlly = ( iAllyOnOff == -1 ) ? !( (CBot*)pBot )->IsAlly( j ) : ( iAllyOnOff != 0 );
						( (CBot*)pBot )->SetAlly( j, bAlly );
						BULOG_I( pEdict, "%s is thinking that %s is its %s.",
							pBot->GetName(), pPlayer->GetName(), bAlly ? "ally" : "enemy" );
					}
				}
			}

			if ( bAllBots )
				break;
		}
	}
    
    return ECommandPerformed;
}

TCommandResult CBotAttackCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	edict_t* pEdict = ( pClient ) ? pClient->GetEdict() : NULL;

    if ( argc < 2 )
	{
		BULOG_W( pEdict, "Error, invalid parameters count." );
		return ECommandError;
	}
	int bAttack = CTypeToString::BoolFromString(argv[0]);
    if ( bAttack == -1 )
    {
        BULOG_W( pEdict, "Error, invalid parameter %s, should be 'on' or 'off'.", argv[0] );
        return ECommandError;
    }

	bool bSomeone = false;
	for ( int arg = 1; arg < argc; ++arg )
	{
		good::string sName( argv[arg] );
		bool bAll = sName == sAll;

		for ( TPlayerIndex i = 0; i < CPlayers::Size(); ++i )
		{
			CPlayer* pPlayer = CPlayers::Get( i );
			if ( pPlayer && pPlayer->IsBot() &&
				( bAll || good::starts_with( good::string( pPlayer->GetName() ), sName ) ) )
			{
				bSomeone = true;
				bool bAttacking = ( bAttack == -1 ) ? !( (CBot*)pPlayer )->IsAttacking() : ( bAttack != 0 );
				( (CBot*)pPlayer )->SetAttack( bAttacking );
				BULOG_I( pEdict, "Bot %s %s.", pPlayer->GetName(), bAttacking ? "attacks" : "doesn't attack" );
			}
		}

		if ( bAll )
			break;
	}
    
    if ( !bSomeone )
    {
        BULOG_W( pEdict, "Error, no matched bots." );
        return ECommandError;
    }
    return ECommandPerformed;
}

TCommandResult CBotMoveCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	edict_t* pEdict = ( pClient ) ? pClient->GetEdict() : NULL;

	if ( argc < 2 )
	{
		BULOG_W( pEdict, "Error, invalid parameters count." );
		return ECommandError;
	}
	int bMove = CTypeToString::BoolFromString( argv[0] );
	if ( bMove == -1 )
	{
		BULOG_W( pEdict, "Error, invalid parameter %s, should be 'on' or 'off'.", argv[0] );
		return ECommandError;
	}

	bool bSomeone = false;
	for ( int arg = 1; arg < argc; ++arg )
	{
		good::string sName( argv[arg] );
		bool bAll = sName == sAll;

		for ( TPlayerIndex i = 0; i < CPlayers::Size(); ++i )
		{
			CPlayer* pPlayer = CPlayers::Get( i );
			if ( pPlayer && pPlayer->IsBot() &&
				( bAll || good::starts_with( good::string( pPlayer->GetName() ), sName ) ) )
			{
				bSomeone = true;
				bool bStop = ( bMove == -1 ) ? !( (CBot*)pPlayer )->IsStopped() : ( bMove == 0 );
				( (CBot*)pPlayer )->SetStopped( bStop );
				BULOG_I( pEdict, "Bot %s %s move.", pPlayer->GetName(), bStop ? "can't" : "can" );
			}
		}
	}

    if ( !bSomeone )
    {
        BULOG_W( pEdict, "Error, no matched bots." );
        return ECommandError;
    }
    return ECommandPerformed;
}

TCommandResult CBotPauseCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	edict_t* pEdict = ( pClient ) ? pClient->GetEdict() : NULL;

	if ( argc < 2 )
	{
		BULOG_W( pEdict, "Error, invalid parameters count." );
		return ECommandError;
	}
	int bPaused = CTypeToString::BoolFromString( argv[0] );
	if ( bPaused == -1 )
	{
		BULOG_W( pEdict, "Error, invalid parameter %s, should be 'on' or 'off'.", argv[0] );
		return ECommandError;
	}

	bool bSomeone = false;
	for ( int arg = 1; arg < argc; ++arg )
	{
		good::string sName( argv[arg] );
		bool bAll = sName == sAll;

        for ( TPlayerIndex i = 0; i < CPlayers::Size(); ++i )
        {
            CPlayer* pPlayer = CPlayers::Get(i);
            if ( pPlayer && pPlayer->IsBot() &&
                 (bAll || good::starts_with(good::string(pPlayer->GetName()), sName)) )
            {
                bSomeone = true;
                bool bPausing = (bPaused == -1) ? !((CBot*)pPlayer)->IsPaused() : (bPaused != 0);
                ((CBot*)pPlayer)->SetPaused( bPausing );
                BULOG_I( pEdict, "Bot %s is %s.", pPlayer->GetName(), bPausing ? "paused" : "not paused" );
            }
        }
    }

    if ( !bSomeone )
    {
        BULOG_W( pEdict, "Error, no matched bots." );
        return ECommandError;
    }
    return ECommandPerformed;
}

good::string sForever("forever");
good::string sOff("off");

CBotProtectCommand::CBotProtectCommand()
{
	m_sCommand = "protect";
	m_sHelp = "protect given players";
	m_sDescription = "Parameters: <forever/off/time-in-seconds> <player-name> ...";
	m_iAccessLevel = FCommandAccessBot;

	StringVector args;
	args.push_back(sForever);
	args.push_back(sOff);

	m_cAutoCompleteArguments.push_back(EConsoleAutoCompleteArgValues);
	m_cAutoCompleteValues.push_back(args);

	m_cAutoCompleteArguments.push_back(EConsoleAutoCompleteArgPlayersForever);
	m_cAutoCompleteValues.push_back(StringVector());
}

TCommandResult CBotProtectCommand::Execute(CClient* pClient, int argc, const char** argv)
{
	if (pClient == NULL)
	{
		BLOG_W("Please login to server to execute this command.");
		return ECommandError;
	}

	if (argc < 2)
	{
		BLOG_W("Invalid parameters count.");
		return ECommandError;
	}
	
	float time = -2;
	if ( sForever == argv[0] )
		time = -1;
	else if ( sOff == argv[0] )
		time = 0;
	else
		sscanf( argv[0], "%f", &time );

	if ( time < 0.0 && time != -1.0 )
	{
		BULOG_W( pClient->GetEdict(), "Error, invalid parameter %s, should be 'forever' / 'off' / time-in-seconds.", argv[0] );
		return ECommandError;
	}

	bool bSomeone = false;
	for ( int i = 1; i < argc;  ++i )
	{
		good::string sName(argv[i]);
		bool bAll = sAll == sName;

		for (TPlayerIndex i = 0; i < CPlayers::Size(); ++i)
		{
			CPlayer* pPlayer = CPlayers::Get(i);
			if ( pPlayer && (bAll || sName == pPlayer->GetName()) )
			{
				bSomeone = true;
				pPlayer->Protect(time);
				BULOG_I(pClient->GetEdict(), "Player %s is now %s.", pPlayer->GetName(), time == 0.0 ? "unprotected" : "protected");
			}
		}
	}
	if (!bSomeone)
	{
		BULOG_W(pClient->GetEdict(), "Error, no matched players.");
		return ECommandError;
	}
	return ECommandPerformed;
}

TCommandResult CBotTestPathCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

    TWaypointId iPathFrom = -1, iPathTo = -1;
    if (argc == 0)
    {
        iPathFrom = pClient->iCurrentWaypoint;
        iPathTo = pClient->iDestinationWaypoint;
    }
    else if (argc == 1)
    {
        iPathFrom = pClient->iCurrentWaypoint;
		if ( sDestination == argv[0] )
			iPathTo = pClient->iDestinationWaypoint;
		else
			sscanf(argv[0], "%d", &iPathTo);
    }
    else if (argc == 2)
    {
        sscanf(argv[0], "%d", &iPathFrom);
        sscanf(argv[1], "%d", &iPathTo);
    }

    if ( !CWaypoints::IsValid(iPathFrom) || !CWaypoints::IsValid(iPathTo) || (iPathFrom == iPathTo) )
    {
        BULOG_W(pClient->GetEdict(), "Error, invalid parameters, 'current' or 'destination' waypoints.");
        return ECommandError;
    }

    CPlayer* pPlayer = CPlayers::AddBot();
    if ( pPlayer )
    {
        ((CBot*)pPlayer)->TestWaypoints(iPathFrom, iPathTo);
        BULOG_I( pClient->GetEdict(), "Bot added: %s. Testing path from %d to %d.",
                 pPlayer->GetName(), iPathFrom, iPathTo );
        return ECommandPerformed;
    }
    else
    {
        BULOG_W( pClient->GetEdict(), CMod::pCurrentMod->GetLastError().c_str() );
        return ECommandError;
    }
}

//----------------------------------------------------------------------------------------------------------------
// Item commands.
//----------------------------------------------------------------------------------------------------------------
CItemDrawCommand::CItemDrawCommand()
{
	m_sCommand = "draw";
	m_sHelp = "defines which items to draw";
	m_sDescription = good::string("Can be 'none' / 'all' / 'next' or mix of: ") + CTypeToString::EntityTypeFlagsToString(EItemTypeAll);
	m_iAccessLevel = FCommandAccessWaypoint; // User doesn't have control over items, he only can draw them.

	StringVector args;
	args.push_back(sNone);
	args.push_back(sAll);
	args.push_back(sNext);
	for (int i = 0; i < EItemTypeOther + 1; ++i)
		args.push_back(CTypeToString::EntityTypeFlagsToString(1 << i).duplicate());

	m_cAutoCompleteArguments.push_back(EConsoleAutoCompleteArgValuesForever);
	m_cAutoCompleteValues.push_back(args);
}

TCommandResult CItemDrawCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

    if ( argc == 0 )
    {
        const good::string& sFlags = CTypeToString::EntityTypeFlagsToString(pClient->iItemTypeFlags);
        BULOG_I(pClient->GetEdict(), "Item types to draw: %s.", sFlags.size() ? sFlags.c_str(): "none");
        return ECommandPerformed;
    }

    // Retrieve flags from string arguments.
    bool bFinished = false;
    TItemTypeFlags iFlags = 0;

    if ( argc == 1 )
    {
        if ( sHelp == argv[0] )
        {
            PrintCommand( pClient->GetEdict() );
            return ECommandPerformed;
        }
        else if ( sNone == argv[0] )
            bFinished = true;
        else if ( sAll == argv[0] )
        {
            iFlags = EItemTypeAll;
            bFinished = true;
        }
        else if ( sNext == argv[0] )
        {
            int iNew = (pClient->iItemTypeFlags)  ?  pClient->iItemTypeFlags << 1  :  1;
            iFlags = (iNew > EItemTypeAll) ? 0 : iNew;
            bFinished = true;
        }
    }

    if ( !bFinished )
    {
        for ( int i=0; i < argc; ++i )
        {
            int iAddFlag = CTypeToString::EntityTypeFlagsFromString(argv[i]);
            if ( iAddFlag == -1 )
            {
                BULOG_W( pClient->GetEdict(), "Error, invalid item type(s). Can be 'none' / 'all' / 'next' or mix of: %s", CTypeToString::EntityTypeFlagsToString(EItemTypeAll).c_str() );
                return ECommandError;
            }
            FLAG_SET(iAddFlag, iFlags);
        }
    }

    pClient->iItemTypeFlags = iFlags;
    const good::string& sFlags = CTypeToString::EntityTypeFlagsToString(pClient->iItemTypeFlags);
    BULOG_I(pClient->GetEdict(), "Item types to draw: %s.", sFlags.size() ? sFlags.c_str(): "none");
    return ECommandPerformed;
}

CItemDrawTypeCommand::CItemDrawTypeCommand()
{
	m_sCommand = "drawtype";
	m_sHelp = "defines how to draw items";
	m_sDescription = good::string("Can be 'none' / 'all' / 'next' or mix of: ") + CTypeToString::ItemDrawFlagsToString(FItemDrawAll);
	m_iAccessLevel = FCommandAccessWaypoint;

	StringVector args;
	args.push_back(sNone);
	args.push_back(sAll);
	args.push_back(sNext);
	for (int i = 0; i < EItemDrawFlagTotal; ++i)
		args.push_back(CTypeToString::ItemDrawFlagsToString(1 << i).duplicate());

	m_cAutoCompleteArguments.push_back(EConsoleAutoCompleteArgValuesForever);
	m_cAutoCompleteValues.push_back(args);
}

TCommandResult CItemDrawTypeCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

    if ( argc == 0 )
    {
        const good::string& sFlags = CTypeToString::ItemDrawFlagsToString(pClient->iItemDrawFlags);
        BULOG_I(pClient->GetEdict(), "Draw item flags: %s.", sFlags.size() ? sFlags.c_str(): "none");
        return ECommandPerformed;
    }

    // Retrieve flags from string arguments.
    bool bFinished = false;
    TItemDrawFlags iFlags = FItemDontDraw;

    if ( argc == 1 )
    {
        if ( sHelp == argv[0] )
        {
            PrintCommand( pClient->GetEdict() );
            return ECommandPerformed;
        }
        else if ( sNone == argv[0] )
            bFinished = true;
        else if ( sAll == argv[0] )
        {
            iFlags = FItemDrawAll;
            bFinished = true;
        }
        else if ( sNext == argv[0] )
        {
            int iNew = (pClient->iItemDrawFlags)  ?  pClient->iItemDrawFlags << 1  :  1;
            iFlags = (iNew > FItemDrawAll) ? 0 : iNew;
            bFinished = true;
        }
    }

    if ( !bFinished )
    {
        for ( int i=0; i < argc; ++i )
        {
            int iAddFlag = CTypeToString::ItemDrawFlagsFromString(argv[i]);
            if ( iAddFlag == -1 )
            {
                BULOG_W( pClient->GetEdict(), "Error, invalid draw type(s). Can be 'none' / 'all' / 'next' or mix of: %s", CTypeToString::ItemDrawFlagsToString(FItemDrawAll).c_str() );
                return ECommandError;
            }
            FLAG_SET(iAddFlag, iFlags);
        }
    }

    pClient->iItemDrawFlags = iFlags;
    BULOG_I(pClient->GetEdict(), "Items drawing is %s.", iFlags ? "on" : "off");
    return ECommandPerformed;
}

//----------------------------------------------------------------------------------------------------------------
// Config commands.
//----------------------------------------------------------------------------------------------------------------
TCommandResult CConfigEventsCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	if ( pClient == NULL )
    {
        BLOG_W( "Please login to server to execute this command." );
        return ECommandError;
    }

    if ( argc == 0 )
    {
        BULOG_I(pClient->GetEdict(), "Display game events: %s.", pClient->bDebuggingEvents ?  "on" : "off");
        return ECommandPerformed;
    }

    int iValue = -1;
    if ( argc == 1 )
        iValue = CTypeToString::BoolFromString(argv[0]);

    if ( iValue == -1 )
    {
        BULOG_W(pClient->GetEdict(), "Error, invalid argument (must be 'on' or 'off').");
        return ECommandError;
    }

    pClient->bDebuggingEvents = iValue != 0;
    CPlayers::CheckForDebugging();
    BULOG_I(pClient->GetEdict(), "Display game events: %s.", pClient->bDebuggingEvents ?  "on" : "off");
    return ECommandPerformed;
}

CConfigLogCommand::CConfigLogCommand()
{
	m_sCommand = "log";
	m_sHelp = "set console log level (none, trace, debug, info, warning, error).";
	m_iAccessLevel = FCommandAccessConfig;

	StringVector args;
	args.push_back(sNone);
	for (int i = 0; i < good::ELogLevelTotal; ++i)
		args.push_back(CTypeToString::LogLevelToString(i).duplicate());

	m_cAutoCompleteArguments.push_back(EConsoleAutoCompleteArgValues);
	m_cAutoCompleteValues.push_back(args);
}

TCommandResult CConfigLogCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	edict_t* pEdict = ( pClient ) ? pClient->GetEdict() : NULL;
    if ( argc == 0 )
    {
        BULOG_I( pEdict, "Console log level: %s.", CTypeToString::LogLevelToString(CUtil::iLogLevel).c_str() );
        return ECommandPerformed;
    }

    good::TLogLevel iLogLevel = -1;
    if ( argc == 1 )
        iLogLevel = CTypeToString::LogLevelFromString(argv[0]);

    if ( iLogLevel == -1 )
    {
        BULOG_W( pEdict, "Error, invalid argument (must be none, trace, debug, info, warning, error)." );
        return ECommandError;
    }

    CUtil::iLogLevel = iLogLevel;
    BULOG_I( pEdict, "Console log level: %s.", argv[0] );
    return ECommandPerformed;
}

CConfigAdminsSetAccessCommand::CConfigAdminsSetAccessCommand()
{
	m_sCommand = "access";
	m_sHelp = "set access flags for given admin";
	m_sDescription = good::string("Arguments: <steam-id> <access-flags>. Can be none / all / mix of: ") +
		CTypeToString::AccessFlagsToString(FCommandAccessAll);
	m_iAccessLevel = FCommandAccessConfig;

	StringVector args0;
	args0.push_back("steam-id");

	StringVector args;
	args.push_back(sNone);
	args.push_back(sAll);
	for (int i = 0; i < ECommandAccessFlagTotal; ++i)
		args.push_back(CTypeToString::AccessFlagsToString(1 << i).duplicate());

	m_cAutoCompleteArguments.push_back( EConsoleAutoCompleteArgValues );
	m_cAutoCompleteValues.push_back( args0 );

	m_cAutoCompleteArguments.push_back( EConsoleAutoCompleteArgValuesForever );
	m_cAutoCompleteValues.push_back( args );
}

TCommandResult CConfigAdminsSetAccessCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	edict_t* pEdict = ( pClient ) ? pClient->GetEdict() : NULL;

    if ( argc < 2 )
    {
        BULOG_W(pEdict, "Error, you show provide steam id and access level.");
        return ECommandError;
    }

    good::string sSteamId = argv[0];

    good::string_buffer sbBuffer(szMainBuffer, iMainBufferSize, false); // Don't deallocate after use.
    for ( int i=1; i<argc; ++i )
        sbBuffer << argv[i] << " ";
    sbBuffer.erase( sbBuffer.size()-1 ); // Erase last space.

    int iFlags = CTypeToString::AccessFlagsFromString( sbBuffer );
    if ( iFlags == -1 )
    {
        BULOG_W( pEdict, "Error, invalid access flags: %s.", sbBuffer.c_str() );
        return ECommandError;
    }

    CConfiguration::SetClientAccessLevel(sSteamId, iFlags);
    bool bFound = false;
    for (int i = 0; i < CPlayers::Size(); ++i)
    {
        CPlayer* pPlayer = CPlayers::Get(i);
        if ( pPlayer && !pPlayer->IsBot() )
        {
            CClient* pClient = (CClient*)pPlayer;
            if ( sSteamId == pClient->GetSteamID() )
            {
                pClient->iCommandAccessFlags = iFlags;
                BULOG_I( pEdict, "Set access flags: '%s' for %s.", sbBuffer.c_str(), pClient->GetName() );
                bFound = true;
                break;
            }
        }
    }

    if ( !bFound )
        BULOG_I( pEdict, "Set access flags: '%s' for %s.", sbBuffer.c_str(), sSteamId.c_str() );

    return ECommandPerformed;
}

TCommandResult CConfigAdminsShowCommand::Execute( CClient* pClient, int argc, const char** argv )
{
	if ( CConsoleCommand::Execute( pClient, argc, argv ) == ECommandPerformed )
		return ECommandPerformed;

	edict_t* pEdict = ( pClient ) ? pClient->GetEdict() : NULL;

    for (int i = 0; i < CPlayers::Size(); ++i)
    {
        CPlayer* pPlayer = CPlayers::Get(i);
        if ( pPlayer && !pPlayer->IsBot() )
        {
            CClient* pClient = (CClient*)pPlayer;
            if ( pClient->iCommandAccessFlags )
                BULOG_I( pEdict, "Name: %s, access: %s, steam id: %s.", pClient->GetName(),
                         CTypeToString::AccessFlagsToString(pClient->iCommandAccessFlags).c_str(), pClient->GetSteamID().c_str() );
        }
    }

    return ECommandPerformed;
}


//----------------------------------------------------------------------------------------------------------------
// Static "botrix" command (server side).
//----------------------------------------------------------------------------------------------------------------
#ifdef BOTRIX_SOURCE_ENGINE_2006

void bbotCommandCallback()
{
    int argc = MIN2(CBotrixPlugin::pEngineServer->Cmd_Argc(), 16);

    static const char* argv[16];
    for (int i = 0; i < argc; ++i)
        argv[i] = CBotrixPlugin::pEngineServer->Cmd_Argv(i);

#else

void bbotCommandCallback( const CCommand &command )
{
    int argc = command.ArgC();
    const char** argv = command.ArgV();

#endif

    CClient* pClient = CPlayers::GetListenServerClient();

    TCommandResult result = CBotrixCommand::instance->Execute( pClient, argc-1, &argv[1] );
    if (result == ECommandRequireAccess)
        BULOG_W(pClient ? pClient->GetEdict() : NULL, "Error, you don't have access to this command.");
    else if (result == ECommandNotFound)
        BULOG_W(pClient ? pClient->GetEdict() : NULL, "Error, command not found.");
    else if (result == ECommandError)
        BULOG_W(pClient ? pClient->GetEdict() : NULL, "Command error.");
}

#if defined(BOTRIX_NO_COMMAND_COMPLETION)
#elif defined(BOTRIX_OLD_COMMAND_COMPLETION)

int bbotCompletion( const char* partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
    int len = strlen(partial);
    return CBotrixCommand::instance->AutoComplete(partial, len, commands, 0, 0);
}

#else

void CBotrixCommand::CommandCallback( const CCommand &command )
{
    bbotCommandCallback(command);
}

#endif // BOTRIX_OLD_COMMAND_COMPLETION

//----------------------------------------------------------------------------------------------------------------
// Main "botrix" command.
//----------------------------------------------------------------------------------------------------------------
CBotrixCommand::CBotrixCommand():
#if defined(BOTRIX_NO_COMMAND_COMPLETION)
    m_cServerCommand(MAIN_COMMAND, bbotCommandCallback, "Botrix plugin's commands. " PLUGIN_VERSION " Beta(BUILD " __DATE__ ")\n", FCVAR_NONE, 0)
#elif defined(BOTRIX_OLD_COMMAND_COMPLETION)
    m_cServerCommand(MAIN_COMMAND, bbotCommandCallback, "Botrix plugin's commands. " PLUGIN_VERSION " Beta(BUILD " __DATE__ ")\n", FCVAR_NONE, bbotCompletion)
#else
    m_cServerCommand(MAIN_COMMAND, this, "Botrix plugin's commands. " PLUGIN_VERSION " Beta(BUILD " __DATE__ ")\n", FCVAR_NONE, this)
#endif
{
    m_sCommand = "botrix";
    if ( CBotrixPlugin::instance->IsEnabled() )
    {
        Add(new CBotCommand);
        Add(new CConfigCommand);
        Add(new CItemCommand);
        Add(new CWaypointCommand);
        Add(new CPathCommand);
        Add(new CVersionCommand);
        Add(new CDisableCommand);
    }
    else
        Add(new CEnableCommand);

#ifndef DONT_USE_VALVE_FUNCTIONS
  #ifdef BOTRIX_SOURCE_ENGINE_2006
    CBotrixPlugin::pCVar->RegisterConCommandBase( &m_cServerCommand );
  #else
    CBotrixPlugin::pCVar->RegisterConCommand( &m_cServerCommand );
  #endif
#endif

	aBoolsCompletion.push_back("on");
	aBoolsCompletion.push_back("off");

	aWaypointCompletion.push_back("current");
	aWaypointCompletion.push_back("destination");
}

CBotrixCommand::~CBotrixCommand()
{
#if !defined(BOTRIX_SOURCE_ENGINE_2006) && !defined(DONT_USE_VALVE_FUNCTIONS)
    CBotrixPlugin::pCVar->UnregisterConCommand( &m_cServerCommand );
#endif

	aBoolsCompletion.clear();
	aWaypointCompletion.clear();
}
