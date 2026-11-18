/*!
	Lightweight liquid target condition.

	Handles the two paths that every liquid action needs:
		- liquid source objects (wells, barrels, ...)
		- liquid surfaces under the real (engine provided) action cursor

	The camera based synthetic cursor / sea level cursor reconstruction lives in
	CCTLiquidSyntheticCursor, so actions that do not need it (drinking, filling,
	washing, snow, ...) never pay for it.

	All engine queries go through CCTLiquidFrameProbe, which shares them between
	every liquid condition evaluated in the same frame. This class only holds the
	per action parameters (liquid mask, reach) and applies them to the shared
	result.
*/
class CCTLiquid : CCTBase
{
	protected const float LIQUID_SURFACE_RAYCAST_OFFSET = 0.2;
	protected const float MAX_HEIGHT_DIFF_SURFACE = 0.25;
	protected const float SEA_HIT_RAW_Y_TOLERANCE = 1.25;

	protected float m_MaxVerticalReach;
	protected float m_MaxActionDistance;
	protected float m_MaxActionDistanceSq;
	protected int m_AllowedLiquidSource;
	protected int m_SurfaceDetectionType;

	//! Precomputed mask tests, so the per-frame paths are plain bool reads.
	protected bool m_CanBeSaltwater;
	protected bool m_CanBeFreshwater;

	//! Shared, frame scoped engine queries.
	protected CCTLiquidFrameProbe m_Probe;

	protected int m_LiquidType = LIQUID_NONE;
	protected int m_WaterSourceType = EWaterSourceObjectType.NONE;
	protected int m_LiquidSourceType = ELiquidSourceObjectType.NONE;

	/*!
		\param allowSeaLevelCursorFallback kept for signature compatibility only,
		it has no effect here. Use CCTLiquidSyntheticCursor if you need the camera
		based cursor reconstruction.
	*/
	void CCTLiquid(int allowedLiquidSource, float maximalTargetDistance = UAMaxDistances.DEFAULT, float maxVerticalReach = UAMaxDistances.DEFAULT, SurfaceDetectionType surfaceDetectionType = SurfaceDetectionType.Roadway, bool allowSeaLevelCursorFallback = false)
	{
		m_MaxActionDistance = maximalTargetDistance;
		m_MaxActionDistanceSq = maximalTargetDistance * maximalTargetDistance;
		m_MaxVerticalReach = maxVerticalReach;
		m_AllowedLiquidSource = allowedLiquidSource;
		m_SurfaceDetectionType = surfaceDetectionType;

		m_CanBeSaltwater = (allowedLiquidSource & LIQUID_SALTWATER) != 0;
		m_CanBeFreshwater = (allowedLiquidSource & LIQUID_FRESHWATER) != 0;

		m_Probe = CCTLiquidFrameProbe.GetInstance();
	}

	#ifdef DIAG_DEVELOPER
	protected string m_ProfilerName = "CCTLiquid::Can";
	#endif

	override bool Can(PlayerBase player, ActionTarget target)
	{
		#ifdef DIAG_DEVELOPER
		g_Game.ProfilerStart(m_ProfilerName);
		bool profiledResult = CanInternal(player, target);
		g_Game.ProfilerStop(m_ProfilerName);
		return profiledResult;
		#else
		return CanInternal(player, target);
		#endif
	}

	protected bool CanInternal(PlayerBase player, ActionTarget target)
	{
		if (!target)
			return false;

		m_LiquidType = LIQUID_NONE;
		m_WaterSourceType = EWaterSourceObjectType.NONE;
		m_LiquidSourceType = ELiquidSourceObjectType.NONE;

		vector playerPos = player.GetPosition();
		Object targetObj = target.GetObject();
		vector hitPosition = target.GetCursorHitPos();

		//! Join (or open) the shared frame. Every engine query below is served from
		//! it, so all liquid conditions of this frame pay for it only once.
		m_Probe.Begin(player, targetObj, target.GetComponentIndex(), hitPosition, m_SurfaceDetectionType);

		//! Pre-calculate object validity once.
		if (targetObj)
		{
			m_WaterSourceType = targetObj.GetWaterSourceObjectType();
			m_LiquidSourceType = targetObj.GetLiquidSourceObjectType();

			bool isValidInteract;
			if (targetObj.IsWell())
				isValidInteract = IsWellInReach(player, playerPos, targetObj, hitPosition) && m_Probe.IsWellComponentValid(targetObj, target.GetComponentIndex());
			else
				isValidInteract = IsValidTargetObject(playerPos, targetObj.GetPosition(), m_WaterSourceType, m_LiquidSourceType, target.IsProxy());

			if (isValidInteract)
			{
				m_LiquidType = targetObj.GetLiquidSourceType();
				return IsValidLiquid(m_LiquidType);
			}
		}

		//! Optional camera based cursor reconstruction (CCTLiquidSyntheticCursor).
		//! When it applies it is authoritative, we must not fall through to the
		//! generic surface checks.
		if (ResolveSyntheticCursor(player, playerPos, target, targetObj, hitPosition))
			return IsValidLiquid(m_LiquidType);

		bool hasSurfaceType;
		if (CheckSurfaceLiquid(player, targetObj, hitPosition, playerPos, hasSurfaceType))
			return IsValidLiquid(m_LiquidType);

		//! Sea level fallback can only ever produce saltwater, skip it entirely
		//! for masks that do not accept saltwater (snow, freshwater only, ...).
		if (m_CanBeSaltwater && !targetObj && !hasSurfaceType && m_LiquidType == LIQUID_NONE)
			CheckSeaLevelFallback(hitPosition, playerPos);

		return IsValidLiquid(m_LiquidType);
	}

	/*!
		Hook for camera based cursor reconstruction. The base condition has none.
		\return true when the cursor was resolved and m_LiquidType was set, the
		caller must then not run the generic surface checks.
	*/
	protected bool ResolveSyntheticCursor(PlayerBase player, vector playerPos, ActionTarget target, Object targetObj, vector hitPosition)
	{
		return false;
	}

	protected bool IsValidLiquid(int liquid)
	{
		return (liquid & m_AllowedLiquidSource) != 0;
	}

	protected bool IsValidTargetObject(vector playerPos, vector objectPosition, EWaterSourceObjectType waterSourceType, ELiquidSourceObjectType liquidSourceType, bool isProxy)
	{
		//! Early exit for invalid types
		if ((waterSourceType == EWaterSourceObjectType.NONE && liquidSourceType == ELiquidSourceObjectType.NONE) || isProxy)
			return false;

		//! Check if position is valid
		if (IsNearlyZero(objectPosition))
			return false;

		//! Check vertical distance first, it is the cheaper of the two.
		if (Math.AbsFloat(objectPosition[1] - playerPos[1]) > m_MaxVerticalReach)
			return false;

		return vector.DistanceSq(playerPos, objectPosition) <= m_MaxActionDistanceSq;
	}

	protected bool IsWellInReach(PlayerBase player, vector playerPos, Object well, vector hitPosition)
	{
		if (player.GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_SERVER)
		{
			float maxDistanceWithSize = m_MaxActionDistance + well.GetCollisionRadius();
			return vector.DistanceSq(well.GetPosition(), playerPos) <= maxDistanceWithSize * maxDistanceWithSize;
		}

		if (IsNearlyZero(hitPosition))
			return false;

		if (vector.DistanceSq(hitPosition, playerPos) <= m_MaxActionDistanceSq)
			return true;

		return vector.DistanceSq(hitPosition, MiscGameplayFunctions.GetPlayerHeadPosition(player)) <= m_MaxActionDistanceSq;
	}

	protected bool CheckSurfaceLiquid(PlayerBase player, Object targetObj, vector hitPosition, vector playerPos, out bool hasSurfaceType)
	{
		hasSurfaceType = false;

		//! Preserve ValidateDistances() range check for surface-liquid hits.
		if (IsNearlyZero(hitPosition))
			return false;

		float surfaceHeight;
		int liquidType;
		if (!m_Probe.GetCursorLiquidSurface(surfaceHeight, liquidType, hasSurfaceType))
			return false;

		vector liquidSurfacePos = hitPosition;
		liquidSurfacePos[1] = Math.Round(surfaceHeight * 1000) * 0.001;

		//! Cheap, per action rejections before touching the shared traces.
		if (Math.AbsFloat(playerPos[1] - surfaceHeight) > m_MaxVerticalReach)
			return false;

		if (targetObj && Math.AbsFloat(hitPosition[1] - surfaceHeight) > MAX_HEIGHT_DIFF_SURFACE)
			return false;

		//! height offset correction
		liquidSurfacePos[1] = liquidSurfacePos[1] + LIQUID_SURFACE_RAYCAST_OFFSET;

		if (vector.DistanceSq(playerPos, liquidSurfacePos) > m_MaxActionDistanceSq)
			return false;

		//! Do not allow water merely below grass/terrain.
		if (!IsCursorActuallyOnLiquidSurface(hitPosition, liquidSurfacePos))
			return false;

		if (!HasClearPathToLiquidSurface(player, liquidSurfacePos))
			return false;

		m_LiquidType = liquidType;
		return true;
	}

	protected bool HasClearPathToLiquidSurface(PlayerBase player, vector surfacePos)
	{
		return m_Probe.HasClearPath(player, surfacePos);
	}

	protected bool CheckSeaLevelFallback(vector hitPosition, vector playerPos)
	{
		vector normalizedSeaHit;
		if (!TryGetNormalizedSeaHitPoint(playerPos, hitPosition, normalizedSeaHit))
			return false;

		m_LiquidType = LIQUID_SALTWATER;
		return true;
	}

	protected bool TryGetLiquidSurfacePosAt(vector probePosition, out vector surfacePosition, out int liquidType)
	{
		surfacePosition = vector.Zero;

		float surfaceHeight;
		bool hasSurfaceType;
		if (!m_Probe.TraceLiquidSurface(probePosition, surfaceHeight, liquidType, hasSurfaceType))
			return false;

		surfacePosition = probePosition;
		surfacePosition[1] = surfaceHeight;
		return true;
	}

	protected bool TryGetNormalizedSeaHitPoint(vector playerPos, vector hitPosition, out vector normalizedHit)
	{
		normalizedHit = vector.Zero;

		if (IsNearlyZero(hitPosition))
			return false;

		float seaLevel, seaLevelMax;
		m_Probe.GetSeaLevels(seaLevel, seaLevelMax);

		//! Raw Y sanity check first, it is a plain float compare. Do not require
		//! exact sea Y here because synced cursor Y can differ.
		if (Math.AbsFloat(hitPosition[1] - seaLevel) > SEA_HIT_RAW_Y_TOLERANCE)
			return false;

		if (hitPosition[1] > seaLevelMax + m_MaxVerticalReach)
			return false;

		//! Preserve ValidateDistances() vertical behavior for normalized sea hits.
		if (Math.AbsFloat(playerPos[1] - seaLevel) > m_MaxVerticalReach)
			return false;

		vector candidate = hitPosition;
		candidate[1] = seaLevel;

		//! Validate real distance against normalized sea point.
		if (vector.DistanceSq(playerPos, candidate) > m_MaxActionDistanceSq)
			return false;

		//! Engine query last.
		if (!g_Game.SurfaceIsSea(hitPosition[0], hitPosition[2]))
			return false;

		normalizedHit = candidate;
		return true;
	}

	protected bool IsCursorActuallyOnLiquidSurface(vector hitPosition, vector liquidSurfacePos)
	{
		if (IsNearlyZero(hitPosition) || IsNearlyZero(liquidSurfacePos))
			return false;

		if (hitPosition[1] > liquidSurfacePos[1])
			return false;

		float nonWaterHeight;
		if (!m_Probe.GetCursorNonWaterSurface(nonWaterHeight))
			return true;

		//! If terrain/grass/road surface is higher, the player is aiming at ground
		//! over/near water, not the water itself.
		return nonWaterHeight <= liquidSurfacePos[1];
	}

	override bool CanContinue(PlayerBase player, ActionTarget target)
	{
		return true;
	}

	int GetLiquidType()
	{
		return m_LiquidType;
	}

	int GetWaterSourceType()
	{
		return m_WaterSourceType;
	}

	int GetLiquidSourceType()
	{
		return m_LiquidSourceType;
	}

	bool IsNearlyZero(vector value)
	{
		return vector.DistanceSq(value, vector.Zero) < 0.0001;
	}
}
