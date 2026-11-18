/*!
	Frame scoped, shared cache of the expensive liquid target queries.

	ActionManagerClient.FindContextualUserActions() runs once per frame and feeds
	the very same PlayerBase, ActionTarget and item to every registered
	ActionInput, which then calls ActionBase.Can() on each of its candidate
	actions. With bare hands next to water that means CCTLiquid.Can() runs 4-6
	times per frame (drink pond, wash hands, wash head, eat snow, wash hands snow,
	...), and with a container in hands another few (fill, mix mortar, wet
	clothing) - every one of them tracing the exact same cursor position.

	The engine queries those conditions need depend only on
		player + target object + component index + cursor hit position + surface detection type
	and *not* on the action's liquid mask, max distance or vertical reach. So they
	are evaluated once per frame and shared, while each condition still applies
	its own mask and reach checks on the cached values.

	Result: N liquid conditions per frame cost 2 GetSurface + 1 raycast in total
	instead of 2N + N.

	Everything is lazy: a payload is only queried when the first condition of the
	frame actually reaches that check, so an early rejecting frame stays cheap.
*/
class CCTLiquidFrameProbe : Managed
{
	protected static ref CCTLiquidFrameProbe m_Instance;

	/*!
		Identity of the currently cached frame. The tick time is a secondary guard
		only - the primary invalidation is the explicit InvalidateFrame() from
		ActionManagerBase.Update(), because GetTickTime() is a float and its
		resolution degrades below one frame on long running servers.
	*/
	protected PlayerBase m_Player;
	protected Object m_TargetObject;
	protected int m_ComponentIndex;
	protected vector m_HitPosition;
	protected int m_SurfaceDetectionType;
	protected bool m_HasFrame;

	//! Cursor liquid surface (includeWater).
	protected bool m_LiquidSurfaceQueried;
	protected bool m_LiquidSurfaceValid;
	protected float m_LiquidSurfaceHeight;
	protected int m_LiquidSurfaceType;
	protected bool m_LiquidSurfaceHasType;

	//! Cursor terrain/roadway surface (no water), for the water-below-grass test.
	protected bool m_NonWaterSurfaceQueried;
	protected bool m_NonWaterSurfaceValid;
	protected float m_NonWaterSurfaceHeight;

	//! Line of sight to the liquid point. Memoized on the tested position.
	protected bool m_ClearPathQueried;
	protected vector m_ClearPathPosition;
	protected bool m_ClearPathValue;

	//! Sea levels, world constants but re-read once per frame.
	protected bool m_SeaLevelsQueried;
	protected float m_SeaLevel;
	protected float m_SeaLevelMax;

	//! Player water level. Memoized on (player, playerPos) rather than on the
	//! Begin() frame key, because it depends only on where the player stands and
	//! not on the target being evaluated.
	protected bool m_WaterLevelQueried;
	protected PlayerBase m_WaterLevelPlayer;
	protected vector m_WaterLevelPos;
	protected vector m_WaterLevelValue;

	//! Well liquid source components. Memoized on the tested object + component.
	protected bool m_WellQueried;
	protected Object m_WellObject;
	protected int m_WellComponentIndex;
	protected bool m_WellValue;

	//! Scratch buffers, one set for the whole game instead of one per condition.
	protected ref SurfaceDetectionParameters m_SurfaceParams;
	protected ref SurfaceDetectionParameters m_SurfaceParamsNoWater;
	protected ref SurfaceDetectionResult m_SurfaceResult;
	protected ref RaycastRVParams m_RaycastParams;
	protected ref array<ref RaycastRVResult> m_RaycastResults;
	protected ref TIntArray m_ComponentIndices;

	#ifdef DIAG_DEVELOPER
	static int m_StatFrames;        //!< frames in which the probe was rebuilt
	static int m_StatBeginCalls;    //!< Can() calls that touched the probe
	static int m_StatCacheHits;     //!< Can() calls served from the cached frame
	static int m_StatSurfaceTraces;
	static int m_StatRaycasts;
	static int m_StatWaterLevelQueries;
	#endif

	void CCTLiquidFrameProbe()
	{
		m_SurfaceParams = new SurfaceDetectionParameters();
		m_SurfaceParams.includeWater = true;
		m_SurfaceParams.syncMode = UseObjectsMode.NoWait;
		m_SurfaceParams.rsd = RoadSurfaceDetection.CLOSEST;

		m_SurfaceParamsNoWater = new SurfaceDetectionParameters();
		m_SurfaceParamsNoWater.includeWater = false;
		m_SurfaceParamsNoWater.syncMode = UseObjectsMode.NoWait;
		m_SurfaceParamsNoWater.rsd = RoadSurfaceDetection.CLOSEST;

		m_SurfaceResult = new SurfaceDetectionResult();
		m_RaycastParams = new RaycastRVParams(vector.Zero, vector.Zero, null);
		m_RaycastParams.flags = CollisionFlags.FIRSTCONTACT;
		m_RaycastParams.type = ObjIntersectFire;
		m_RaycastParams.sorted = true;
		m_RaycastResults = new array<ref RaycastRVResult>();
		m_ComponentIndices = new TIntArray();
	}

	static CCTLiquidFrameProbe GetInstance()
	{
		if (!m_Instance)
			m_Instance = new CCTLiquidFrameProbe();

		return m_Instance;
	}

	/*!
		Declares the inputs the calling condition is about to evaluate. If they
		match the currently cached frame everything below is served from the cache,
		otherwise the cache is dropped.

		\return true when the cached frame was reused
	*/
	bool Begin(PlayerBase player, Object targetObject, int componentIndex, vector hitPosition, SurfaceDetectionType surfaceDetectionType)
	{
		#ifdef DIAG_DEVELOPER
		m_StatBeginCalls++;
		#endif

		if (m_HasFrame && m_Player == player && m_TargetObject == targetObject && m_ComponentIndex == componentIndex && m_HitPosition == hitPosition && m_SurfaceDetectionType == surfaceDetectionType)
		{
			#ifdef DIAG_DEVELOPER
			m_StatCacheHits++;
			#endif

			return true;
		}

		#ifdef DIAG_DEVELOPER
		m_StatFrames++;
		#endif

		m_Player = player;
		m_TargetObject = targetObject;
		m_ComponentIndex = componentIndex;
		m_HitPosition = hitPosition;
		m_SurfaceDetectionType = surfaceDetectionType;
		m_HasFrame = true;
		
		m_SurfaceParams.type = surfaceDetectionType;
		m_SurfaceParamsNoWater.type = surfaceDetectionType;

		m_LiquidSurfaceQueried = false;
		m_NonWaterSurfaceQueried = false;
		m_ClearPathQueried = false;
		m_SeaLevelsQueried = false;
		m_WellQueried = false;

		return false;
	}

	//! Liquid surface under the cached cursor position.
	bool GetCursorLiquidSurface(out float surfaceHeight, out int liquidType, out bool hasSurfaceType)
	{
		if (!m_LiquidSurfaceQueried)
		{
			m_LiquidSurfaceQueried = true;
			m_LiquidSurfaceValid = TraceLiquidSurface(m_HitPosition, m_LiquidSurfaceHeight, m_LiquidSurfaceType, m_LiquidSurfaceHasType);
		}

		surfaceHeight = m_LiquidSurfaceHeight;
		liquidType = m_LiquidSurfaceType;
		hasSurfaceType = m_LiquidSurfaceHasType;

		return m_LiquidSurfaceValid;
	}

	//! Highest non-liquid surface under the cached cursor position.
	bool GetCursorNonWaterSurface(out float surfaceHeight)
	{
		if (!m_NonWaterSurfaceQueried)
		{
			m_NonWaterSurfaceQueried = true;
			m_NonWaterSurfaceValid = TraceNonWaterSurface(m_HitPosition, m_NonWaterSurfaceHeight);
		}

		surfaceHeight = m_NonWaterSurfaceHeight;
		return m_NonWaterSurfaceValid;
	}

	bool HasClearPath(PlayerBase player, vector surfacePos)
	{
		if (m_ClearPathQueried && m_ClearPathPosition == surfacePos)
			return m_ClearPathValue;

		m_ClearPathQueried = true;
		m_ClearPathPosition = surfacePos;
		m_ClearPathValue = TraceClearPath(player, surfacePos);

		return m_ClearPathValue;
	}

	void GetSeaLevels(out float seaLevel, out float seaLevelMax)
	{
		if (!m_SeaLevelsQueried)
		{
			m_SeaLevelsQueried = true;
			m_SeaLevel = g_Game.SurfaceGetSeaLevel();
			m_SeaLevelMax = g_Game.SurfaceGetSeaLevelMax();
		}

		seaLevel = m_SeaLevel;
		seaLevelMax = m_SeaLevelMax;
	}

	/*!
		Shared water level under the player, as returned by
		HumanCommandSwim.WaterLevelCheck: (totalWaterDepth, characterDepth, 0).

		Several liquid actions resolve their stance from this every frame (see
		ActionBase.Can() -> GetClosestPossibleStance() -> GetStanceMask()), all with
		the same arguments, so the query is done once per frame and shared.
	*/
	void GetPlayerWaterLevel(notnull PlayerBase player, out vector waterInfo)
	{
		vector playerPos = player.GetPosition();

		if (!m_WaterLevelQueried || m_WaterLevelPlayer != player || m_WaterLevelPos != playerPos)
		{
			#ifdef DIAG_DEVELOPER
			m_StatWaterLevelQueries++;
			#endif

			m_WaterLevelQueried = true;
			m_WaterLevelPlayer = player;
			m_WaterLevelPos = playerPos;
			m_WaterLevelValue = HumanCommandSwim.WaterLevelCheck(player, playerPos);
		}

		waterInfo = m_WaterLevelValue;
	}

	//! Is the aimed at component one of the well's liquid source components.
	bool IsWellComponentValid(Object well, int componentIndex)
	{
		if (m_WellQueried && m_WellObject == well && m_WellComponentIndex == componentIndex)
			return m_WellValue;

		m_WellQueried = true;
		m_WellObject = well;
		m_WellComponentIndex = componentIndex;

		m_ComponentIndices.Clear();
		well.GetActionComponentsForSelectionName(well.GetViewGeometryLevel(), UAMisc.LIQUID_SOURCE_SELECTION, m_ComponentIndices);

		if (m_ComponentIndices.Count() > 0)
			m_WellValue = m_ComponentIndices.Find(componentIndex) != -1;
		else
			m_WellValue = true;

		return m_WellValue;
	}

	/*!
		Uncached surface trace at an arbitrary position, for the camera ray march
		in CCTLiquidSyntheticCursor which samples many different positions.
	*/
	bool TraceLiquidSurface(vector probePosition, out float surfaceHeight, out int liquidType, out bool hasSurfaceType)
	{
		surfaceHeight = 0;
		liquidType = LIQUID_NONE;
		hasSurfaceType = false;

		#ifdef DIAG_DEVELOPER
		m_StatSurfaceTraces++;
		#endif

		m_SurfaceParams.position = probePosition;
		m_SurfaceResult.surface = null; //! The result object is reused, never read a stale surface.
		if (!g_Game.GetSurface(m_SurfaceParams, m_SurfaceResult))
			return false;

		SurfaceInfo surfaceInfo = m_SurfaceResult.surface;
		if (!surfaceInfo)
			return false;

		if (surfaceInfo.GetSurfaceType() == string.Empty)
			return false;

		hasSurfaceType = true;
		surfaceHeight = m_SurfaceResult.height;

		liquidType = g_Game.CorrectLiquidType(surfaceInfo.GetLiquidType());
		return liquidType != LIQUID_NONE;
	}

	bool TraceNonWaterSurface(vector probePosition, out float surfaceHeight)
	{
		surfaceHeight = 0;

		#ifdef DIAG_DEVELOPER
		m_StatSurfaceTraces++;
		#endif

		m_SurfaceParamsNoWater.position = probePosition;
		m_SurfaceResult.surface = null; //! The result object is reused, never read a stale surface.
		if (!g_Game.GetSurface(m_SurfaceParamsNoWater, m_SurfaceResult))
			return false;

		SurfaceInfo surfaceInfo = m_SurfaceResult.surface;
		if (!surfaceInfo)
			return false;

		if (surfaceInfo.GetSurfaceType() == string.Empty)
			return false;

		//! Check if we have the same or an other liquid surface
		if (surfaceInfo.GetLiquidType() > LIQUID_NONE)
			return false;

		surfaceHeight = m_SurfaceResult.height;
		return true;
	}

	bool TraceClearPath(PlayerBase player, vector surfacePos)
	{
		vector playerPos = player.GetPosition();
		float headHeight = MiscGameplayFunctions.GetPlayerHeadHeight(player);
		playerPos[1] = playerPos[1] + headHeight;
		
		m_RaycastParams.begPos = playerPos;
		m_RaycastParams.endPos = surfacePos;
		m_RaycastParams.ignore = player;

		#ifdef DIAG_DEVELOPER
		m_StatRaycasts++;
		#endif

		m_RaycastResults.Clear();
		if (!DayZPhysics.RaycastRVProxy(m_RaycastParams, m_RaycastResults))
			return true;

		float surfaceDistSq = vector.DistanceSq(playerPos, surfacePos);
		foreach (RaycastRVResult result : m_RaycastResults)
		{
			if (!result || !result.obj)
				continue;

			//! Something solid is between player and the liquid point.
			if (vector.DistanceSq(playerPos, result.pos) < surfaceDistSq)
				return false;
		}

		return true;
	}

	//! Drops the cached frame. Called once per action manager update, and usable
	//! from anywhere that changes the world mid frame.
	void Invalidate()
	{
		m_HasFrame = false;
		m_WaterLevelQueried = false;
	}

	//! No-op when no liquid condition has ever run, so the hook in
	//! ActionManagerBase.Update() does not create the probe for nothing.
	static void InvalidateFrame()
	{
		if (m_Instance)
			m_Instance.Invalidate();
	}

	#ifdef DIAG_DEVELOPER
	static void ResetStats()
	{
		m_StatFrames = 0;
		m_StatBeginCalls = 0;
		m_StatCacheHits = 0;
		m_StatSurfaceTraces = 0;
		m_StatRaycasts = 0;
		m_StatWaterLevelQueries = 0;
	}

	static string GetStatsString()
	{
		float hitRate = 0;
		float tracesPerFrame = 0;
		float raysPerFrame = 0;
		float waterPerFrame = 0;

		if (m_StatBeginCalls > 0)
		{
			float calls = m_StatBeginCalls;
			float hits = m_StatCacheHits;
			hitRate = 100.0 * hits / calls;
		}

		if (m_StatFrames > 0)
		{
			float frames = m_StatFrames;
			float traces = m_StatSurfaceTraces;
			float rays = m_StatRaycasts;
			float waterQueries = m_StatWaterLevelQueries;
			tracesPerFrame = traces / frames;
			raysPerFrame = rays / frames;
			waterPerFrame = waterQueries / frames;
		}

		return string.Format("CCTLiquid probe: Can()=%1 | probeFrames=%2 | cacheHits=%3 (%4%%) | GetSurface=%5 (%6/frame) | RaycastRV=%7 (%8/frame) | WaterLevel=%9",
			m_StatBeginCalls, m_StatFrames, m_StatCacheHits, hitRate, m_StatSurfaceTraces, tracesPerFrame, m_StatRaycasts, raysPerFrame, waterPerFrame);
	}
	#endif
}
