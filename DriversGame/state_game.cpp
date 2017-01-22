//////////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2015
//////////////////////////////////////////////////////////////////////////////////
// Description: State of game
//////////////////////////////////////////////////////////////////////////////////

//
// TODO:
//		- General code refactoring from C-style to better C++ style
//		- Move replay director to separate source files and as a state
//		- Make CState_Game state object initialized from here as g_pState_Game to make it more accessible
//

#include "state_game.h"
#include "CameraAnimator.h"
#include "materialsystem/MeshBuilder.h"

#include "session_stuff.h"
#include "Rain.h"

#include "KeyBinding/InputCommandBinder.h"

#include "sys_console.h"

#include "system.h"
#include "FontCache.h"

#include "DrvSynHUD.h"

#include "Shiny.h"

static CCameraAnimator	s_cameraAnimator;
CCameraAnimator*		g_pCameraAnimator = &s_cameraAnimator;

CGameSession*			g_pGameSession = NULL;

extern ConVar			net_server;

ConVar					g_pause("g_pause", "0");
ConVar					g_director("g_director", "0");
ConVar					g_freecam("g_freecam", "0");
ConVar					g_freecam_speed("g_freecam_speed", "10", NULL, CV_ARCHIVE);
ConVar					g_mouse_sens("g_mouse_sens", "1.0", "mouse sensitivity", CV_ARCHIVE);

ConVar					director_timeline_zoom("director_timeline_zoom", "1.0", 0.1, 10.0, "Timeline scale", CV_ARCHIVE);

int						g_nOldControlButtons	= 0;
int						g_nDirectorCameraType	= CAM_MODE_TRIPOD_ZOOM;

#define					DIRECTOR_DEFAULT_CAMERA_FOV	 (60.0f) // old: 52

struct freeCameraProps_t
{
	freeCameraProps_t()
	{
		fov = DIRECTOR_DEFAULT_CAMERA_FOV;
		position = vec3_zero;
		angles = vec3_zero;
		velocity = vec3_zero;
		zAxisMove = false;
	}

	Vector3D	position;
	Vector3D	angles;
	Vector3D	velocity;
	float		fov;

	bool		zAxisMove;
} g_freeCamProps;


void Game_ShutdownSession(bool restart = false);
void Game_InitializeSession();

void Game_QuickRestart(bool demo)
{
	if(GetCurrentStateType() != GAME_STATE_GAME)
		return;

	//SetCurrentState(NULL);

	if(!demo)
	{
		g_replayData->Stop();
		g_replayData->Clear();
	}

	g_State_Game->QuickRestart(demo);
}

void Game_OnPhysicsUpdate(float fDt, int iterNum);

DECLARE_CMD(restart, "Restarts game quickly", 0)
{
	Game_QuickRestart(false);
}

DECLARE_CMD(fastseek, "Does instant replay. You can fetch to frame if specified", 0)
{
	if(g_pGameSession == NULL)
		return;

	int replayTo = 0;
	if(CMD_ARGC > 0)
		replayTo = atoi(CMD_ARGV(0).c_str());

	g_replayData->Stop();
	g_replayData->m_tick = 0;
	g_replayData->m_state = REPL_INIT_PLAYBACK;

	Game_QuickRestart(true);

	const float frameRate = 1.0f / 60.0f;

	while(replayTo > 0)
	{
		// TODO: use g_replayData->m_demoFrameRate
		Game_OnPhysicsUpdate(frameRate, 0);

		replayTo--;
	}

	g_pCameraAnimator->Reset();
}

void Game_InstantReplay(int replayTo)
{
	if(g_pGameSession == NULL)
		return;

	if(replayTo == 0 && g_replayData->m_state == REPL_PLAYING)
	{
		g_replayData->Stop();
		g_replayData->m_tick = 0;
		g_replayData->m_state = REPL_INIT_PLAYBACK;

		Game_QuickRestart(true);
	}
	else
	{
		if(replayTo >= g_replayData->m_tick)
		{
			replayTo -= g_replayData->m_tick;
		}
		else
		{
			g_replayData->Stop();
			g_replayData->m_tick = 0;
			g_replayData->m_state = REPL_INIT_PLAYBACK;

			Game_QuickRestart(true);
		}
	}

	g_pGameWorld->m_level.WaitForThread();

	g_pCameraAnimator->Reset();

	const float frameRate = 1.0f / 60.0f;

	while(replayTo > 0)
	{
		// TODO: use g_replayData->m_demoFrameRate
		g_pPhysics->Simulate(frameRate, PHYSICS_ITERATION_COUNT, Game_OnPhysicsUpdate);

		replayTo--;
		replayTo--;
	}
}

DECLARE_CMD(instantreplay, "Does instant replay (slowly). You can fetch to frame if specified", 0)
{
	int replayTo = 0;
	if(CMD_ARGC > 0)
		replayTo = atoi(CMD_ARGV(0).c_str());

	Game_InstantReplay( replayTo );
}

DECLARE_CMD(start, "loads a level or starts mission", 0)
{
	if(CMD_ARGC == 0)
	{
		Msg("Usage: start <name> - starts game with specified level or mission\n");
		return;
	}

	// unload game
	if(GetCurrentStateType() == GAME_STATE_GAME)
	{
		g_State_Game->UnloadGame();
	}

	// always set level name
	g_pGameWorld->SetLevelName( CMD_ARGV(0).c_str() );

	// first try load mission script
	if( !g_State_Game->LoadMissionScript(CMD_ARGV(0).c_str()) )
	{
		// fail-safe mode
	}

	SetCurrentState( g_states[GAME_STATE_GAME], true);
}

//------------------------------------------------------------------------------

void fnMaxplayersTest(ConVar* pVar,char const* pszOldValue)
{
	if(g_pGameSession != NULL && g_pGameSession->GetSessionType() == SESSION_NETWORK)
		Msg("maxplayers will be changed upon restart\n");
}

ConVar sv_maxplayers("maxplayers", "1", fnMaxplayersTest, "Maximum players allowed on the server\n");

//------------------------------------------------------------------------------
// Loads new game world
//------------------------------------------------------------------------------

bool Game_LoadWorld()
{
	Msg("-- LoadWorld --\n");

	g_pGameWorld->Init();
	return g_pGameWorld->LoadLevel();
}

//------------------------------------------------------------------------------
// Initilizes game session
//------------------------------------------------------------------------------

void Game_InitializeSession()
{
	Msg("-- InitializeSession --\n");

	if(!g_pGameSession)
	{
		if(net_server.GetBool())
			g_svclientInfo.maxPlayers = sv_maxplayers.GetInt();
		else if(g_svclientInfo.maxPlayers <= 1)
			net_server.SetBool(true);

		if( g_svclientInfo.maxPlayers > 1 )
		{
			CNetGameSession* netSession = new CNetGameSession();
			g_pGameSession = netSession;
		}
		else
			g_pGameSession = new CGameSession();
	}

#ifndef __INTELLISENSE__

	OOLUA::set_global(GetLuaState(), "gameses", g_pGameSession);
	OOLUA::set_global(GetLuaState(), "gameHUD", g_pGameHUD);

#endif // __INTELLISENSE__

	if(g_replayData->m_state != REPL_INIT_PLAYBACK)
		g_replayData->Clear();

	g_pCameraAnimator->Reset();

	g_pGameSession->Init();

	//reset cameras
	g_nDirectorCameraType = 0;

	// reset buttons
	ZeroInputControls();
}

void Game_ShutdownSession(bool restart)
{
	Msg("-- ShutdownSession%s --\n", restart ? "Restart" : "");
	g_parallelJobs->Wait();

	effectrenderer->RemoveAllEffects();

	if(g_pGameSession)
	{
		if(!restart)
			g_pGameSession->FinalizeMissionManager();

		g_pGameSession->Shutdown();
	}

	delete g_pGameSession;
	g_pGameSession = NULL;
}

void Game_DirectorControlKeys(int key, bool down);

void Game_HandleKeys(int key, bool down)
{
	if(g_director.GetBool())
		Game_DirectorControlKeys(key, down);
}

void Game_UpdateFreeCamera(float fDt)
{
	Vector3D f, r;
	AngleVectors(g_freeCamProps.angles, &f, &r);

	Vector3D camMoveVec(0.0f);

	if(g_nClientButtons & IN_FORWARD)
		camMoveVec += f;
	else if(g_nClientButtons & IN_BACKWARD)
		camMoveVec -= f;

	if(g_nClientButtons & IN_LEFT)
		camMoveVec -= r;
	else if(g_nClientButtons & IN_RIGHT)
		camMoveVec += r;

	g_freeCamProps.velocity += camMoveVec * 200.0f * fDt;

	float camSpeed = length(g_freeCamProps.velocity);

	// limit camera speed
	if(camSpeed > g_freecam_speed.GetFloat())
	{
		float speedDiffScale = g_freecam_speed.GetFloat() / camSpeed;
		g_freeCamProps.velocity *= speedDiffScale;
	}

	btSphereShape collShape(0.5f);

	// update camera collision
	if(camSpeed > 1.0f)
	{
		g_freeCamProps.velocity -= normalize(g_freeCamProps.velocity) * 90.0f * fDt;

		eqPhysCollisionFilter filter;
		filter.type = EQPHYS_FILTER_TYPE_EXCLUDE;
		filter.flags = EQPHYS_FILTER_FLAG_DYNAMICOBJECTS | EQPHYS_FILTER_FLAG_STATICOBJECTS;

		int cycle = 0;

		CollisionData_t coll;
		while(g_pPhysics->TestConvexSweep(&collShape, Quaternion(0,0,0,0), g_freeCamProps.position, g_freeCamProps.position+g_freeCamProps.velocity, coll, 0xFFFFFFFF, &filter))
		{
			if(coll.fract == 0.0f)
			{
				float nDot = dot(coll.normal, g_freeCamProps.velocity);
				g_freeCamProps.velocity -= coll.normal*nDot;
			}

			filter.AddObject( coll.hitobject );

			cycle++;
			if(cycle > MAX_COLLISION_FILTER_OBJECTS)
				break;
		}
	}
	else
	{
		g_freeCamProps.velocity = vec3_zero;
	}

	/*
	g_pPhysics->m_physics.SetDebugRaycast(true);

	// test code, must be removed after fixing raycast broadphase
	CollisionData_t coll;
	g_pPhysics->TestConvexSweep(&collShape, Quaternion(0,0,0,0), g_freeCamProps.position, g_freeCamProps.position+f*2000.0f, coll, 0xFFFFFFFF);

	debugoverlay->Box3D(coll.position - 0.5f, coll.position + 0.5f, ColorRGBA(0,1,0,0.25f), 0.1f);
	debugoverlay->Line3D(coll.position, coll.position + coll.normal, ColorRGBA(0,0,1,0.25f), ColorRGBA(0,0,1,0.25f) );
	
	g_pPhysics->m_physics.SetDebugRaycast(false);
	*/

	g_freeCamProps.position += g_freeCamProps.velocity * fDt;
}

static const wchar_t* cameraTypeStrings[] = {
	L"Outside car",
	L"In car",
	L"Tripod",
	L"Tripod (fixed zoom)",
	L"Static",
};

static const ColorRGB cameraColors[] = {
	ColorRGB(1.0f,0.25f,0.25f),
	ColorRGB(0.0f,0.25f,0.65f),
	ColorRGB(0.2f,0.7f,0.2f),
	ColorRGB(0.5f,0.2f,0.7f),
	ColorRGB(0.8f,0.8f,0.2f),
};

bool g_director_ShiftKey = false;
const float DIRECTOR_FASTFORWARD_TIMESCALE = 4.0f;

extern ConVar sys_timescale;

void Game_DirectorControlKeys(int key, bool down)
{
	CCar* viewedCar = g_pGameSession->GetViewCar();

	if(key == KEY_SHIFT)
	{
		g_director_ShiftKey = down;
	}
	else if(key == KEY_BACKSPACE)
	{
		//sys_timescale.GetFloat();
		sys_timescale.SetFloat( down ? DIRECTOR_FASTFORWARD_TIMESCALE : 1.0f );
	}

	if(down)
	{
		//Msg("Director mode keypress: %d\n", key);

		int replayCamera = g_replayData->m_currentCamera;
		replaycamera_t* currentCamera = g_replayData->GetCurrentCamera();
		replaycamera_t* prevCamera = g_replayData->m_cameras.inRange(replayCamera-1) ? &g_replayData->m_cameras[replayCamera-1] : NULL;
		replaycamera_t* nextCamera = g_replayData->m_cameras.inRange(replayCamera+1) ? &g_replayData->m_cameras[replayCamera+1] : NULL;
		int totalTicks = g_replayData->m_numFrames;

		int highTick = nextCamera ? nextCamera->startTick : totalTicks;
		int lowTick = prevCamera ? prevCamera->startTick : 0;

		if(key == KEY_ADD)
		{
			replaycamera_t cam;

			cam.fov = g_freeCamProps.fov;
			cam.origin = g_freeCamProps.position;
			cam.rotation = g_freeCamProps.angles;
			cam.startTick = g_replayData->m_tick;
			cam.targetIdx = viewedCar->m_replayID;
			cam.type = g_nDirectorCameraType;

			int camIndex = g_replayData->AddCamera(cam);
			g_replayData->m_currentCamera = camIndex;

			// set camera after keypress
			g_freecam.SetBool(false);

			Msg("Add camera at tick %d\n", cam.startTick);
		}
		else if(key == KEY_KP_ENTER)
		{
			if(currentCamera && g_pause.GetBool())
			{
				Msg("Set camera\n");
				currentCamera->fov = g_freeCamProps.fov;
				currentCamera->origin = g_freeCamProps.position;
				currentCamera->rotation = g_freeCamProps.angles;
				currentCamera->targetIdx = viewedCar->m_replayID;
				currentCamera->type = g_nDirectorCameraType;

				g_freecam.SetBool(false);
			}
		}
		else if(key == KEY_DELETE)
		{
			if(replayCamera >= 0 && g_replayData->m_cameras.numElem())
			{
				g_replayData->m_cameras.removeIndex(replayCamera);
				g_replayData->m_currentCamera--;
			}
		}
		else if(key == KEY_SPACE)
		{
			//Msg("Add camera keyframe\n");
		}
		else if(key >= KEY_1 && key <= KEY_5)
		{
			g_nDirectorCameraType = key - KEY_1;
		}
		else if(key == KEY_PGUP)
		{
			if(g_replayData->m_cameras.inRange(replayCamera+1) && g_pause.GetBool())
				g_replayData->m_currentCamera++;
		}
		else if(key == KEY_PGDN)
		{
			if(g_replayData->m_cameras.inRange(replayCamera-1) && g_pause.GetBool())
				g_replayData->m_currentCamera--;
		}
		else if(key == KEY_LEFT)
		{
			if(currentCamera && g_pause.GetBool())
			{
				currentCamera->startTick -= g_director_ShiftKey ? 10 : 1;
				
				if(currentCamera->startTick < lowTick)
					currentCamera->startTick = lowTick;
			}
				
		}
		else if(key == KEY_RIGHT)
		{
			if(currentCamera && g_pause.GetBool())
			{
				currentCamera->startTick += g_director_ShiftKey ? 10 : 1;
				
				if(currentCamera->startTick > highTick)
					currentCamera->startTick = highTick;
			}
		}
	}
}

DECLARE_CMD(director_pick_ray, "Director mode - picks object with ray", 0)
{
	if(!g_director.GetBool())
		return;

	Vector3D start = g_freeCamProps.position;
	Vector3D dir;
	AngleVectors(g_freeCamProps.angles, &dir);

	Vector3D end = start + dir*1000.0f;

	CollisionData_t coll;
	g_pPhysics->TestLine(start, end, coll, OBJECTCONTENTS_VEHICLE);

	if(coll.hitobject != NULL && (coll.hitobject->m_flags & BODY_ISCAR))
	{
		CCar* car = (CCar*)coll.hitobject->GetUserData();

		if(car)
			g_pGameSession->SetPlayerCar( car );
	}
}

void Game_DrawDirectorUI( float fDt )
{
	const IVector2D& screenSize = g_pHost->GetWindowSize();

	materials->Setup2D(screenSize.x,screenSize.y);

	wchar_t* controlsText = varargs_w(
		L"PLAY = &#FFFF00;O&;\n"
		L"TOGGLE FREE CAMERA = &#FFFF00;F&;\n\n"

		L"NEXT CAMERA = &#FFFF00;PAGE UP&;\n"
		L"PREV CAMERA = &#FFFF00;PAGE DOWN&;\n\n"

		L"INSERT NEW CAMERA = &#FFFF00;KP_PLUS&;\n"
		L"UPDATE CAMERA = &#FFFF00;KP_ENTER&;\n"
		L"DELETE CAMERA = &#FFFF00;DEL&;\n"

		L"MOVE CAMERA START = &#FFFF00;LEFT ARROW&; and &#FFFF00;RIGHT ARROW&;\n\n"

		//L"SET CAMERA KEYFRAME = &#FFFF00;SPACE&;\n\n"

		L"CAMERA TYPE = &#FFFF00;1-5&; (Current is &#FFFF00;'%s'&;)\n"
		L"CAMERA ZOOM = &#FFFF00;MOUSE WHEEL&; (%.2f deg.)\n"
		L"TARGET VEHICLE = &#FFFF00;LEFT MOUSE CLICK ON OBJECT&;\n"
		
		L"SEEK = &#FFFF00;fastseek <frame>&; (in console)\n", cameraTypeStrings[g_nDirectorCameraType], g_freeCamProps.fov);

	wchar_t* shortText =	L"PAUSE = &#FFFF00;O&;\n"
							L"TOGGLE FREE CAMERA = &#FFFF00;F&;\n"
							L"FAST FORWARD 4x = &#FFFF00;BACKSPACE&;\n";

	eqFontStyleParam_t params;
	params.styleFlag = TEXT_STYLE_SHADOW | TEXT_STYLE_USE_TAGS;
	params.textColor = color4_white;

	Vector2D directorTextPos(15, screenSize.y/3);

	if(g_pause.GetBool())
		g_pHost->GetDefaultFont()->RenderText(controlsText, directorTextPos, params);
	else
		g_pHost->GetDefaultFont()->RenderText(shortText, directorTextPos, params);

	replaycamera_t* currentCamera = g_replayData->GetCurrentCamera();
	int replayCamera = g_replayData->m_currentCamera;
	int currentTick = g_replayData->m_tick;
	int totalTicks = g_replayData->m_numFrames;

	int totalCameras = g_replayData->m_cameras.numElem();

	wchar_t* framesStr = varargs_w(L"FRAME: &#FFFF00;%d / %d&;\nCAMERA: &#FFFF00;%d&; (frame %d) / &#FFFF00;%d&;", currentTick, totalTicks, replayCamera+1, currentCamera ? currentCamera->startTick : 0, totalCameras);

	Rectangle_t timelineRect(0,screenSize.y-100, screenSize.x, screenSize.y-70);
	CMeshBuilder meshBuilder(materials->GetDynamicMesh());

	BlendStateParam_t blending;
	blending.srcFactor = BLENDFACTOR_SRC_ALPHA;
	blending.dstFactor = BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

	g_pShaderAPI->SetTexture(0,0,0);
	materials->SetRasterizerStates(CULL_BACK);
	materials->SetDepthStates(false,false);
	materials->SetBlendingStates(blending);
	materials->BindMaterial(materials->GetDefaultMaterial());

	const float pixelsPerTick = 1.0f / 4.0f * director_timeline_zoom.GetFloat();
	const float currentTickOffset = currentTick*pixelsPerTick;
	const float lastTickOffset = totalTicks*pixelsPerTick;

	float timelineCenterPos = timelineRect.GetCenter().x;

	meshBuilder.Begin(PRIM_TRIANGLE_STRIP);
		float ticksOffset = lastTickOffset-currentTickOffset;

		Rectangle_t drawnTimeline(timelineRect.GetCenter().x - currentTickOffset, screenSize.y-100.0f, timelineRect.GetCenter().x + ticksOffset , screenSize.y-70.0f);
		drawnTimeline.vleftTop.x = clamp(drawnTimeline.vleftTop.x,0.0f,timelineRect.vrightBottom.x);
		drawnTimeline.vrightBottom.x = clamp(drawnTimeline.vrightBottom.x,0.0f,timelineRect.vrightBottom.x);

		meshBuilder.Color4f(1,1,1, 0.25f);
		meshBuilder.Quad2(drawnTimeline.GetLeftTop(), drawnTimeline.GetRightTop(), drawnTimeline.GetLeftBottom(), drawnTimeline.GetRightBottom());

		for(int i = 0; i < totalCameras; i++)
		{
			replaycamera_t* camera = &g_replayData->m_cameras[i];

			float cameraTickPos = (camera->startTick-currentTick) * pixelsPerTick;

			replaycamera_t* nextCamera = i+1 <g_replayData->m_cameras.numElem() ? &g_replayData->m_cameras[i+1] : NULL;
			float nextTickPos = ((nextCamera ? nextCamera->startTick : totalTicks)-currentTick) * pixelsPerTick;

			// draw colored rectangle
			Rectangle_t cameraColorRect(timelineRect.GetCenter().x + cameraTickPos, screenSize.y-95.0f, timelineRect.GetCenter().x + nextTickPos, screenSize.y-75.0f);

			ColorRGB camRectColor(cameraColors[camera->type]);

			if(currentCamera == camera && g_pause.GetBool())
			{
				camRectColor *= fabs(sinf((float)g_pHost->GetCurTime()*2.0f));

				// draw start tick position
				Rectangle_t currentTickRect(timelineRect.GetCenter() - Vector2D(2, 25) + Vector2D(cameraTickPos,0), timelineRect.GetCenter() + Vector2D(2, 0) + Vector2D(cameraTickPos,0));
				meshBuilder.Color4f(1.0f,0.0f,0.0f,0.8f);
				meshBuilder.Quad2(currentTickRect.GetLeftTop(), currentTickRect.GetRightTop(), currentTickRect.GetLeftBottom(), currentTickRect.GetRightBottom());
			}

			meshBuilder.Color4fv(ColorRGBA(camRectColor, 0.7f));
			meshBuilder.Quad2(cameraColorRect.GetLeftTop(), cameraColorRect.GetRightTop(), cameraColorRect.GetLeftBottom(), cameraColorRect.GetRightBottom());

			// draw start tick position
			Rectangle_t currentTickRect(timelineRect.GetCenter() - Vector2D(2, 15) + Vector2D(cameraTickPos,0), timelineRect.GetCenter() + Vector2D(2, 15) + Vector2D(cameraTickPos,0));
			meshBuilder.Color4f(0.9f,0.9f,0.9f,0.8f);
			meshBuilder.Quad2(currentTickRect.GetLeftTop(), currentTickRect.GetRightTop(), currentTickRect.GetLeftBottom(), currentTickRect.GetRightBottom());
		}

		// current tick
		Rectangle_t currentTickRect(timelineRect.GetCenter() - Vector2D(2, 20), timelineRect.GetCenter() + Vector2D(2, 20));
		meshBuilder.Color4f(0,0,0,1.0f);
		meshBuilder.Quad2(currentTickRect.GetLeftTop(), currentTickRect.GetRightTop(), currentTickRect.GetLeftBottom(), currentTickRect.GetRightBottom());

		// end tick
		Rectangle_t lastTickRect(timelineRect.GetCenter() - Vector2D(2, 20) + Vector2D(ticksOffset,0), timelineRect.GetCenter() + Vector2D(2, 20) + Vector2D(ticksOffset,0));
		meshBuilder.Color4f(1,0.05f,0,1.0f);
		meshBuilder.Quad2(lastTickRect.GetLeftTop(), lastTickRect.GetRightTop(), lastTickRect.GetLeftBottom(), lastTickRect.GetRightBottom());

	meshBuilder.End();

	params.align = TEXT_ALIGN_HCENTER;

	Vector2D frameInfoTextPos(screenSize.x/2, screenSize.y - (screenSize.y/6));
	g_pHost->GetDefaultFont()->RenderText(framesStr, frameInfoTextPos, params);

	if(g_freecam.GetBool())
	{
		Vector2D halfScreen = Vector2D(screenSize)*0.5f;

		Vertex2D_t tmprect[] =
		{
			Vertex2D_t(halfScreen+Vector2D(0,-3), vec2_zero),
			Vertex2D_t(halfScreen+Vector2D(3,3), vec2_zero),
			Vertex2D_t(halfScreen+Vector2D(-3,3), vec2_zero)
		};

		// Draw crosshair
		materials->DrawPrimitives2DFFP(PRIM_TRIANGLES, tmprect, elementsOf(tmprect), NULL, ColorRGBA(1,1,1,0.45));
	}
}

ConVar eq_profiler_display("eqProfiler_display", "0", "Display profiler on screen");
extern ConVar g_pause;

//-------------------------------------------------------------------------------

CState_Game* g_State_Game = new CState_Game();

CState_Game::CState_Game() : CBaseStateHandler()
{
	m_demoMode = false;
	m_isGameRunning = false;
	m_fade = 1.0f;
	m_doLoadingFrames = 0;
	m_missionScriptName = "defaultmission";

	RegisterInputJoysticEssentials();
}

CState_Game::~CState_Game()
{

}

void CState_Game::UnloadGame()
{
	if(!g_pPhysics)
		return;

	m_isGameRunning = false;
	g_pGameHUD->Cleanup();

	// renderer must be reset
	g_pShaderAPI->Reset(STATE_RESET_ALL);
	g_pShaderAPI->Apply();

	g_pGameWorld->Cleanup();

	g_pPhysics->SceneShutdown();

	Game_ShutdownSession();

	g_pModelCache->ReleaseCache();

	ses->Shutdown();

	delete g_pPhysics;
	g_pPhysics = NULL;
}

void CState_Game::LoadGame()
{
	soundsystem->SetVolumeScale( 0.0f );

	UnloadGame();

	ses->Init(EQ_DRVSYN_DEFAULT_SOUND_DISTANCE);

	PrecacheStudioModel( "models/error.egf" );
	PrecacheScriptSound( "menu.back" );
	PrecacheScriptSound( "menu.roll" );

	g_pPhysics = new CPhysicsEngine();

	g_pPhysics->SceneInit();
	g_pGameHUD->Init();

	if( Game_LoadWorld() )
	{
		Game_InitializeSession();
		g_pause.SetBool(false);
	}
	else
	{
		SetNextState(g_states[GAME_STATE_TITLESCREEN]);
		m_loadingError = true;
	}
}

bool CState_Game::LoadMissionScript( const char* name )
{
	m_missionScriptName = name;

	// don't start both times
	EqString scriptFileName(varargs("scripts/missions/%s.lua", name));

	// then we load custom script
	if( !EqLua::LuaBinding_LoadAndDoFile( GetLuaState(), scriptFileName.c_str(), "MissionLoader" ) )
	{
		MsgError("mission script init error:\n\n%s\n", OOLUA::get_last_error(GetLuaState()).c_str());

		m_missionScriptName = name;

		// okay, try reinitialize with default mission script
		if( !EqLua::LuaBinding_LoadAndDoFile( GetLuaState(), "scripts/missions/defaultmission.lua", "MissionLoader"))
		{
			MsgError("default mission script init error:\n\n%s\n", OOLUA::get_last_error(GetLuaState()).c_str());
			return false;
		}

		return false;
	}

	return true;
}

const char* CState_Game::GetMissionScriptName() const
{
	return m_missionScriptName.c_str();
}

void CState_Game::StopStreams()
{
	ses->StopAllSounds();
}

void CState_Game::QuickRestart(bool replay)
{
	g_pGameHUD->Cleanup();

	StopStreams();

	m_isGameRunning = false;
	m_exitGame = false;
	m_fade = 1.0f;

	// renderer must be reset
	g_pShaderAPI->Reset(STATE_RESET_ALL);
	g_pShaderAPI->Apply();

	g_pGameWorld->Cleanup(false);

	Game_ShutdownSession(true);

	g_pGameWorld->LoadLevel();

	g_pGameHUD->Init();

	g_pGameWorld->Init();

	Game_InitializeSession();

	g_pause.SetBool(false);

	//-------------------------

	if(!replay)
		SetupMenuStack( m_gameMenuName.c_str() );
}

void CState_Game::OnEnterSelection( bool isFinal )
{
	if(isFinal)
	{
		m_fade = 0.0f;
		m_exitGame = true;
		m_showMenu = false;
	}
}

void CState_Game::SetupMenuStack( const char* name )
{
	OOLUA::Table mainMenuStack;
	if(!OOLUA::get_global(GetLuaState(), name, mainMenuStack))
		WarningMsg("Failed to get %s table (DrvSynMenus.lua ???)!\n", name);
	else
		SetMenuObject( mainMenuStack );
}

void CState_Game::OnMenuCommand( const char* command )
{
	if(!stricmp(command, "continue"))
	{
		m_showMenu = false;
	}
	else if(!stricmp(command, "showMap"))
	{
		Msg("TODO: show the map\n");
	}
	else if(!stricmp(command, "restartGame"))
	{
		m_showMenu = false;
		m_exitGame = false;
		m_fade = 0.0f;

		if(g_pGameSession->GetMissionStatus() == MIS_STATUS_INGAME)
			g_pGameSession->SignalMissionStatus(MIS_STATUS_FAILED, 0.0f);

		m_scheduledRestart = true;
	}
	else if(!stricmp(command, "quickReplay") || !stricmp(command, "goToDirector"))
	{
		m_showMenu = false;
		m_exitGame = false;
		m_fade = 0.0f;

		if(!stricmp(command, "goToDirector"))
			g_director.SetBool(true);
		else
			g_director.SetBool(false);

		if(g_pGameSession->GetMissionStatus() == MIS_STATUS_INGAME)
		{
			SetupMenuStack("MissionEndMenuStack");
			g_pGameSession->SignalMissionStatus(MIS_STATUS_FAILED, 0.0f);
		}

		m_scheduledQuickReplay = true;
	}
}

// when changed to this state
// @from - used to transfer data
void CState_Game::OnEnter( CBaseStateHandler* from )
{
	if(m_isGameRunning)
		return;

	m_loadingError = false;
	m_exitGame = false;
	m_showMenu = false;

	m_scheduledRestart = false;
	m_scheduledQuickReplay = false;

	m_doLoadingFrames = 2;

	m_fade = 1.0f;

	m_menuTitleToken = g_localizer->GetToken("MENU_GAME_TITLE_PAUSE");
}

bool CState_Game::DoLoadingFrame()
{
	LoadGame();

    if(!g_pGameSession)
        return false;	// no game session causes a real problem

	if(g_pGameSession->GetSessionType() == SESSION_SINGLE)
		m_gameMenuName = "GameMenuStack";
	else if(g_pGameSession->GetSessionType() == SESSION_NETWORK)
		m_gameMenuName = "MPGameMenuStack";

	SetupMenuStack( m_gameMenuName.c_str() );

	return true;
}

// when the state changes to something
// @to - used to transfer data
void CState_Game::OnLeave( CBaseStateHandler* to )
{
	m_demoMode = false;

	if(!g_pGameSession)
		return;

	UnloadGame();
}

int CState_Game::GetPauseMode() const
{
	if(!g_pGameSession)
		return PAUSEMODE_PAUSE;

	if(g_pGameSession->IsGameDone())
	{
		return g_pGameSession->GetMissionStatus() == MIS_STATUS_SUCCESS ? PAUSEMODE_COMPLETE : PAUSEMODE_GAMEOVER;
	}

	if((g_pause.GetBool() || m_showMenu) && g_pGameSession->GetSessionType() == SESSION_SINGLE)
		return PAUSEMODE_PAUSE;

	return PAUSEMODE_NONE;
}

void CState_Game::SetPauseState( bool state )
{
	if(!m_exitGame)
		m_showMenu = state;

	if(m_showMenu)
		m_selection = 0;

	// update pause state
	UpdatePauseState();
}

void CState_Game::StartReplay( const char* path )
{
	if(g_replayData->LoadFromFile( path ))
		SetCurrentState( this, true );
}

void CState_Game::DrawLoadingScreen()
{
	const IVector2D& screenSize = g_pHost->GetWindowSize();

	materials->Setup2D(screenSize.x, screenSize.y);
	g_pShaderAPI->Clear( true,true, false );

	IEqFont* font = g_fontCache->GetFont("Roboto Condensed", 30, TEXT_STYLE_BOLD+TEXT_STYLE_ITALIC);

	const wchar_t* loadingStr = LocalizedString("#GAME_IS_LOADING");

	eqFontStyleParam_t param;
	param.styleFlag |= TEXT_STYLE_SHADOW;

	font->RenderText(loadingStr, Vector2D(100,screenSize.y - 100), param);
}

//-------------------------------------------------------------------------------
// Game frame step along with rendering
//-------------------------------------------------------------------------------
bool CState_Game::Update( float fDt )
{
	if(m_loadingError)
		return false;

	const IVector2D& screenSize = g_pHost->GetWindowSize();

	if(!m_isGameRunning)
	{
		DrawLoadingScreen();
		
		m_doLoadingFrames--;

		if( m_doLoadingFrames > 0 )
			return true;
		else if( m_doLoadingFrames == 0 )	
			return DoLoadingFrame(); // actual level loading happened here

		if(g_pGameWorld->m_level.IsWorkDone() && materials->GetLoadingQueue() == 0)
			m_isGameRunning = true;

		return true;
	}

	float fGameFrameDt = fDt;

	bool replayDirectorMode = (g_replayData->m_state == REPL_PLAYING && g_director.GetBool());

	bool gameDone = g_pGameSession->IsGameDone(false);
	bool gameDoneTimedOut = g_pGameSession->IsGameDone();

	// force end this game
	if(gameDone && m_showMenu && !gameDoneTimedOut)
	{
		g_pGameSession->SignalMissionStatus(g_pGameSession->GetMissionStatus(), -1.0f);
		m_showMenu = false;
	}

	gameDoneTimedOut = g_pGameSession->IsGameDone();

	if(gameDoneTimedOut && !m_exitGame)
	{
		if(m_demoMode)
		{
			m_exitGame = true;
			m_fade = 0.0f;
			SetNextState(g_states[GAME_STATE_TITLESCREEN]);
		}
		else if(!m_showMenu && !replayDirectorMode)
		{
			// set other menu
			m_showMenu = !m_scheduledRestart && !m_scheduledQuickReplay;

			SetupMenuStack("MissionEndMenuStack");
		}
	}

	// update pause state
	if( !UpdatePauseState() )
		fGameFrameDt = 0.0f;

	// reset buttons
	if(m_showMenu)
		ZeroInputControls();

	//
	// Update, Render, etc
	//
	DoGameFrame( fGameFrameDt );

	if(m_exitGame || m_scheduledRestart || m_scheduledQuickReplay)
	{
		ColorRGBA blockCol(0.0,0.0,0.0,m_fade);

		Vertex2D_t tmprect1[] = { MAKETEXQUAD(0, 0,screenSize.x, screenSize.y, 0) };

		materials->Setup2D(screenSize.x,screenSize.y);

		BlendStateParam_t blending;
		blending.srcFactor = BLENDFACTOR_SRC_ALPHA;
		blending.dstFactor = BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

		materials->DrawPrimitives2DFFP(PRIM_TRIANGLE_STRIP,tmprect1,elementsOf(tmprect1), NULL, blockCol, &blending);

		m_fade += fDt;

		if(m_fade >= 1.0f)
		{
			if(m_scheduledRestart)
				Game_QuickRestart(false);

			if(m_scheduledQuickReplay)
				Game_InstantReplay(0);

			m_scheduledRestart = false;
			m_scheduledQuickReplay = false;

			return !m_exitGame;
		}

		soundsystem->SetVolumeScale(1.0f-m_fade);
	}
	else
	{
		if( m_fade > 0.0f )
		{
			ColorRGBA blockCol(0.0,0.0,0.0,1.0f);

			Vertex2D_t tmprect1[] = { MAKETEXQUAD(0, 0,screenSize.x, screenSize.y*m_fade*0.5f, 0) };
			Vertex2D_t tmprect2[] = { MAKETEXQUAD(0, screenSize.y*0.5f + screenSize.y*(1.0f-m_fade)*0.5f,screenSize.x, screenSize.y, 0) };

			materials->Setup2D(screenSize.x, screenSize.y);
			materials->DrawPrimitives2DFFP(PRIM_TRIANGLE_STRIP,tmprect1,elementsOf(tmprect1), NULL, blockCol);
			materials->DrawPrimitives2DFFP(PRIM_TRIANGLE_STRIP,tmprect2,elementsOf(tmprect2), NULL, blockCol);

			m_fade -= fDt*5.0f;

			soundsystem->SetVolumeScale(1.0f-m_fade);
		}
		else
			soundsystem->SetVolumeScale( 1.0f );
	}

	DrawMenu(fDt);

	return true;
}

bool CState_Game::UpdatePauseState()
{
	int pauseMode = GetPauseMode();

	if( pauseMode > 0 )
	{
		ISoundPlayable* musicChan = soundsystem->GetStaticStreamChannel(CHAN_STREAM);
		if(musicChan)
			musicChan->Pause();

		ISoundPlayable* voiceChan = soundsystem->GetStaticStreamChannel(CHAN_VOICE);
		if(voiceChan)
			voiceChan->Pause();
	}
	else
	{
		if(m_pauseState != (pauseMode > 0))
		{
			ISoundPlayable* musicChan = soundsystem->GetStaticStreamChannel(CHAN_STREAM);
			if(musicChan && musicChan->GetState() != SOUND_STATE_PLAYING)
				musicChan->Play();

			ISoundPlayable* voiceChan = soundsystem->GetStaticStreamChannel(CHAN_VOICE);
			if(voiceChan && voiceChan->GetState() != SOUND_STATE_PLAYING)
				voiceChan->Play();
		}
	}

	soundsystem->SetPauseState( pauseMode > 0);
	m_pauseState = (pauseMode > 0);

	return (pauseMode == 0);
}

void CState_Game::DrawMenu( float fDt )
{
	if( !m_showMenu )
		return;

	const IVector2D& screenSize = g_pHost->GetWindowSize();

	materials->Setup2D(screenSize.x,screenSize.y);

	IVector2D halfScreen(screenSize.x/2, screenSize.y/2);

	IEqFont* font = g_fontCache->GetFont("Roboto", 30);

	eqFontStyleParam_t fontParam;
	fontParam.align = TEXT_ALIGN_HCENTER;
	fontParam.styleFlag |= TEXT_STYLE_SHADOW;
	fontParam.textColor = color4_white;
	fontParam.scale = 20.0f;

	{
		lua_State* state = GetLuaState();

		EqLua::LuaStackGuard g(state);

		int numElems = 0;

		oolua_ipairs(m_menuElems)
			numElems++;
		oolua_ipairs_end()

		int menuPosY = halfScreen.y - numElems*font->GetLineHeight(fontParam)*0.5f;

		Vector2D mTextPos(halfScreen.x, menuPosY);

		fontParam.textColor = ColorRGBA(0.7f,0.7f,0.7f,1.0f);
		font->RenderText(m_menuTitleToken ? m_menuTitleToken->GetText() : L"Undefined token", mTextPos, fontParam);

		oolua_ipairs(m_menuElems)
			int idx = _i_index_-1;

			OOLUA::Table elem;
			m_menuElems.safe_at(_i_index_, elem);

			const wchar_t* token = NULL;

			ILocToken* tok = NULL;
			if(elem.safe_at("label", tok))
				token = tok ? tok->GetText() : L"Undefined token";

			EqLua::LuaTableFuncRef labelValue;
			if(labelValue.Get(elem, "labelValue", true) && labelValue.Push() && labelValue.Call(0, 1))
			{
				int val = 0;
				OOLUA::pull(state, val);

				token = tok ? varargs_w(tok->GetText(), val) : L"Undefined token";
			}

			if(m_selection == idx)
				fontParam.textColor = ColorRGBA(1,0.7f,0.0f,1.0f);
			else
				fontParam.textColor = ColorRGBA(1,1,1,1.0f);

			Vector2D eTextPos(halfScreen.x, menuPosY+_i_index_*font->GetLineHeight(fontParam));

			font->RenderText(token ? token : L"No token", eTextPos, fontParam);
		oolua_ipairs_end()

	}
}

CCar* CState_Game::GetViewCar() const
{
	CCar* viewedCar = g_pGameSession ? g_pGameSession->GetViewCar() : NULL;

	if(g_replayData->m_state == REPL_PLAYING && g_replayData->m_cameras.numElem() > 0)
	{
		// replay controls camera
		replaycamera_t* replCamera = g_replayData->GetCurrentCamera();

		if(replCamera)
			viewedCar = g_replayData->GetCarByReplayIndex( replCamera->targetIdx );
	}

	return viewedCar;
}

Vector3D CState_Game::GetViewVelocity() const
{
	CCar* viewedCar = GetViewCar();

	Vector3D cam_velocity = vec3_zero;

	// animate the camera if car is present
	if( viewedCar && g_pCameraAnimator->GetMode() <= CAM_MODE_INCAR && !g_freecam.GetBool() )
		cam_velocity = viewedCar->GetVelocity();

	return cam_velocity;
}

void GRJob_DrawEffects(void* data, int i)
{
	float fDt = *(float*)data;
	effectrenderer->DrawEffects( fDt );
}

void CState_Game::RenderMainView3D( float fDt )
{
	static float jobFrametime = fDt;
	jobFrametime = fDt;

	// post draw effects
	g_parallelJobs->AddJob(GRJob_DrawEffects, &jobFrametime);
	g_parallelJobs->Submit();

	// rebuild view
	const IVector2D& screenSize = g_pHost->GetWindowSize();
	g_pGameWorld->BuildViewMatrices(screenSize.x,screenSize.y, 0);

	// frustum update
	PROFILE_CODE(g_pGameWorld->UpdateOccludingFrustum());

	// render
	PROFILE_CODE(g_pGameWorld->Draw( 0 ));
}

void CState_Game::RenderMainView2D( float fDt )
{
	const IVector2D& screenSize = g_pHost->GetWindowSize();

	// draw HUD
	if( g_replayData->m_state != REPL_PLAYING )
		g_pGameHUD->Render( fDt, screenSize );

	if(	g_director.GetBool() && g_replayData->m_state == REPL_PLAYING)
		Game_DrawDirectorUI( fDt );

	if(!g_pause.GetBool())
		PROFILE_UPDATE();

	if(eq_profiler_display.GetBool())
	{
		EqString profilerStr = PROFILE_GET_TREE_STRING().c_str();

		materials->Setup2D(screenSize.x,screenSize.y);

		eqFontStyleParam_t params;
		params.styleFlag = TEXT_STYLE_SHADOW | TEXT_STYLE_FROM_CAP;

		static IEqFont* consoleFont = g_fontCache->GetFont("console", 16);

		consoleFont->RenderText(profilerStr.c_str(), Vector2D(45), params);
	}
}

void CState_Game::DoGameFrame(float fDt)
{
	// Update game
	PROFILE_FUNC();

	// session update
	g_pGameSession->UpdateLocalControls( g_nClientButtons, g_joySteeringValue, g_joyAccelBrakeValue );
	g_pGameSession->Update(fDt);

	//Game_UpdateCamera(fDt);
	DoCameraUpdates( fDt );

	// render all
	RenderMainView3D( fDt );
	RenderMainView2D( fDt );

	g_nOldControlButtons = g_nClientButtons;
}

void CState_Game::DoCameraUpdates( float fDt )
{
	int camControls = (g_replayData->m_state == REPL_PLAYING) ? 0 : g_nClientButtons;

	CViewParams* curView = g_pGameWorld->GetView();

	if( g_freecam.GetBool() )
	{
		Game_UpdateFreeCamera( g_pHost->GetFrameTime() );

		curView->SetOrigin(g_freeCamProps.position);
		curView->SetAngles(g_freeCamProps.angles);
		curView->SetFOV(g_freeCamProps.fov);

		g_pCameraAnimator->SetOrigin(g_freeCamProps.position);
	}
	else
	{
		if(!g_pCameraAnimator->IsScripted())
		{
			CCar* viewedCar = g_pGameSession->GetViewCar();

			if(g_replayData->m_state == REPL_PLAYING && g_replayData->m_cameras.numElem() > 0)
			{
				// replay controls camera
				replaycamera_t* replCamera = g_replayData->GetCurrentCamera();

				if(replCamera)
				{
					// Process camera
					viewedCar = g_replayData->GetCarByReplayIndex( replCamera->targetIdx );

					g_pCameraAnimator->SetMode( (ECameraMode)replCamera->type );
					g_pCameraAnimator->SetOrigin( replCamera->origin );
					g_pCameraAnimator->SetAngles( replCamera->rotation );
					g_pCameraAnimator->SetFOV( replCamera->fov );

					g_pCameraAnimator->Update(fDt, 0, viewedCar);
				}
			}

			/*
			if( viewedCar && viewedCar->GetPhysicsBody() )
			{
				DkList<CollisionPairData_t>& pairs = viewedCar->GetPhysicsBody()->m_collisionList;

				if(pairs.numElem())
				{
					float powScale = 1.0f / (float)pairs.numElem();

					float invMassA = viewedCar->GetPhysicsBody()->GetInvMass();

					for(int i = 0; i < pairs.numElem(); i++)
					{
						float appliedImpulse = fabs(pairs[i].appliedImpulse) * invMassA * powScale;

						if(pairs[i].impactVelocity > 1.5f)
							g_pCameraAnimator->ViewShake(min(appliedImpulse*0.5f, 5.0f), min(appliedImpulse*0.5f, 0.25f));
					}
				}
			}
			*/

			g_pCameraAnimator->Update(fDt, camControls, viewedCar);
		}

		// set final result to the world renderer
		g_pGameWorld->SetView( g_pCameraAnimator->GetComputedView() );

		// as always
		g_freeCamProps.position = curView->GetOrigin();
		g_freeCamProps.angles = curView->GetAngles();
		g_freeCamProps.fov = DIRECTOR_DEFAULT_CAMERA_FOV;
	}

	// also update various systems
	Vector3D viewVelocity = GetViewVelocity();

	Vector3D f,r,u;
	AngleVectors(curView->GetAngles(), &f, &r, &u);

	// all positions and velocity props
	soundsystem->SetListener(curView->GetOrigin(), f, u, viewVelocity);
	effectrenderer->SetViewSortPosition(curView->GetOrigin());
	g_pRainEmitter->SetViewVelocity(viewVelocity);
}

void CState_Game::HandleKeyPress( int key, bool down )
{
	if(!m_isGameRunning)
		return;

	if( m_demoMode )
	{
		if(m_fade <= 0.0f)
		{
			m_fade = 0.0f;
			m_exitGame = true;
			SetNextState(g_states[GAME_STATE_TITLESCREEN]);
		}

		return;
	}

	if(key == KEY_ESCAPE && down)
	{
		if(m_showMenu && IsCanPopMenu())
		{
			EmitSound_t es("menu.back");
			ses->Emit2DSound( &es );

			PopMenu();

			return;
		}

		SetPauseState( !m_showMenu );
	}

	if( m_showMenu )
	{
		if(!down)
			return;

		if(key == KEY_ENTER)
		{
			PreEnterSelection();
			EnterSelection();
		}
		else if(key == KEY_LEFT || key == KEY_RIGHT)
		{
			if(ChangeSelection(key == KEY_LEFT ? -1 : 1))
			{
				EmitSound_t es("menu.roll");
				ses->Emit2DSound( &es );
			}
		}
		else if(key == KEY_UP)
		{
redecrement:

			m_selection--;

			if(m_selection < 0)
			{
				int nItem = 0;
				m_selection = m_numElems-1;
			}

			//if(pItem->type == MIT_SPACER)
			//	goto redecrement;

			EmitSound_t ep("menu.roll");
			ses->Emit2DSound(&ep);
		}
		else if(key == KEY_DOWN)
		{
reincrement:
			m_selection++;

			if(m_selection > m_numElems-1)
				m_selection = 0;

			//if(pItem->type == MIT_SPACER)
			//	goto reincrement;

			EmitSound_t ep("menu.roll");
			ses->Emit2DSound(&ep);
		}
	}
	else
	{
		Game_HandleKeys(key, down);

		//if(!MenuKeys( key, down ))
			g_inputCommandBinder->OnKeyEvent( key, down );
	}
}

void CState_Game::HandleMouseMove( int x, int y, float deltaX, float deltaY )
{
	if(!m_isGameRunning)
		return;

	g_pHost->SetCenterMouseEnable( g_freecam.GetBool() );

	if( m_showMenu )
		return;

	if(g_freecam.GetBool() && !g_pSysConsole->IsVisible()) // && g_pHost->m_hasWindowFocus)
	{
		if(g_freeCamProps.zAxisMove)
		{
			g_freeCamProps.angles.z += deltaX * g_mouse_sens.GetFloat();
		}
		else
		{
			g_freeCamProps.angles.x += deltaY * g_mouse_sens.GetFloat();
			g_freeCamProps.angles.y += deltaX * g_mouse_sens.GetFloat();
		}
	}
}

void CState_Game::HandleMouseClick( int x, int y, int buttons, bool down )
{
	if(!m_isGameRunning)
		return;

	if( m_showMenu )
		return;

	if(buttons == MOU_B2)
	{
		g_freeCamProps.zAxisMove = down;
	}

	g_inputCommandBinder->OnMouseEvent(buttons, down);
}

void CState_Game::HandleMouseWheel(int x,int y,int scroll)
{
	if(!m_isGameRunning)
		return;

	g_freeCamProps.fov -= scroll;
}

void CState_Game::HandleJoyAxis( short axis, short value )
{

}
