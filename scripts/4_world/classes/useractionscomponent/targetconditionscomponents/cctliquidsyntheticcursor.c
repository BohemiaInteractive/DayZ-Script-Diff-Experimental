/*!
	Liquid target condition with camera based cursor reconstruction.

	Some actions (fishing) must be usable while aiming at open water where the
	engine action cursor returns no hit at all, or returns a synced Y that is off
	the sea plane. This condition reconstructs a cursor from the camera ray and
	keeps it stable across frames.

	Only actions that actually need this should use it, everything else uses the
	plain CCTLiquid, which does not run any of the code below.
*/
class CCTLiquidSyntheticCursor : CCTLiquid
{
	protected const float SEA_FALLBACK_DISTANCE_BUFFER = 1.5;
	protected const float SYNTHETIC_POND_SAMPLE_STEP = 1.0;
	protected const float SYNTHETIC_POND_SURFACE_TOLERANCE = 0.35;
	protected const float SYNTHETIC_POND_FALLBACK_SURFACE_TOLERANCE = 1.75;
	protected const float SYNTHETIC_CURSOR_DISTANCE_BUFFER = 1.5;
	protected const float SEA_CURSOR_STICK_DISTANCE = 0.75;
	protected const int SEA_CURSOR_STICK_TIME_MS = 350;
	protected const float SYNTHETIC_CURSOR_CACHE_AIM_TOLERANCE = 1.75;
	protected const int SYNTHETIC_CURSOR_CACHE_MAX_AGE_MS = 350;
	protected const float SEA_CURSOR_MIN_DOWN_DOT = 0.035;
	protected const float POND_MAX_CAMERA_DIR_Y = 0.15;

	protected vector m_LastSyntheticHitCursor;
	protected bool m_HasLastSyntheticHitCursor;
	protected int m_LastSyntheticHitCursorTime;

	//! Derived once, used by every camera ray path.
	protected float m_SafeSeaDistanceSq;
	protected float m_SafeCursorDistance;
	protected float m_StickDistanceSq;
	protected float m_CacheAimToleranceSq;

	#ifdef DIAG_DEVELOPER
	protected bool m_DebugSyntheticCursor;
	protected ref array<Shape> m_DebugSyntheticCursorShapes;
	#endif

	void CCTLiquidSyntheticCursor(int allowedLiquidSource, float maximalTargetDistance = UAMaxDistances.DEFAULT, float maxVerticalReach = UAMaxDistances.DEFAULT, SurfaceDetectionType surfaceDetectionType = SurfaceDetectionType.Roadway, bool allowSeaLevelCursorFallback = true)
	{
		ClearSyntheticCursorCache();

		float safeSeaDistance = m_MaxActionDistance - SEA_FALLBACK_DISTANCE_BUFFER;
		if (safeSeaDistance <= 1.0)
			safeSeaDistance = m_MaxActionDistance * 0.8;

		m_SafeSeaDistanceSq = safeSeaDistance * safeSeaDistance;

		m_SafeCursorDistance = m_MaxActionDistance - SYNTHETIC_CURSOR_DISTANCE_BUFFER;
		if (m_SafeCursorDistance <= 1.0)
			m_SafeCursorDistance = m_MaxActionDistance * 0.8;

		m_StickDistanceSq = SEA_CURSOR_STICK_DISTANCE * SEA_CURSOR_STICK_DISTANCE;
		m_CacheAimToleranceSq = SYNTHETIC_CURSOR_CACHE_AIM_TOLERANCE * SYNTHETIC_CURSOR_CACHE_AIM_TOLERANCE;

		#ifdef DIAG_DEVELOPER
		m_ProfilerName = "CCTLiquidSyntheticCursor::Can";
		m_DebugSyntheticCursorShapes = new array<Shape>();
		#endif
	}
	
	override bool CanContinue(PlayerBase player, ActionTarget target)
	{
		return Can(player, target); //! Re-validate during action
	}

	override protected bool ResolveSyntheticCursor(PlayerBase player, vector playerPos, ActionTarget target, Object targetObj, vector hitPosition)
	{
		//! Only relevant when there is no target object at all.
		if (targetObj)
			return false;

		#ifdef DIAG_DEVELOPER
		m_DebugSyntheticCursor = DiagMenu.GetBool(DiagMenuIDs.SYNTHETIC_FISHING_CURSOR);
		#endif

		vector resolvedHitPosition;
		int resolvedLiquidType;
		if (!TryResolveSyntheticCursor(player, playerPos, hitPosition, resolvedHitPosition, resolvedLiquidType))
			return false;

		target.SetCursorHitPos(resolvedHitPosition);
		m_LiquidType = resolvedLiquidType;

		return true;
	}

	protected bool TryResolveSyntheticCursor(PlayerBase player, vector playerPos, vector currentHitPosition, out vector resolvedHitPosition, out int resolvedLiquidType)
	{
		resolvedHitPosition = currentHitPosition;
		resolvedLiquidType = LIQUID_NONE;

		//! First validate the existing/synced cursor.
		if (TryValidateHitCursor(player, playerPos, currentHitPosition, resolvedHitPosition, resolvedLiquidType))
			return true;

		//! If there is a real cursor hit but it is not valid liquid, do not
		//! synthesize water. This prevents looking at ground/terrain near a pond
		//! from creating a synthetic cursor.
		if (currentHitPosition != vector.Zero)
		{
			ClearSyntheticCursorCache();

			#ifdef DIAG_DEVELOPER
			if (m_DebugSyntheticCursor)
				DebugDrawSyntheticCursor(player, currentHitPosition, vector.Zero, false, "current non-liquid hit prevents synthetic cursor", LIQUID_NONE);
			#endif

			return false;
		}

		//! Server stops here. It should not invent a cursor from camera state.
		if (g_Game.IsHeadlessOrDedicatedServer())
			return false;

		//! Client fallback: build synthetic cursor only when current cursor is invalid.
		vector syntheticHitPosition;
		int syntheticLiquidType;

		if (!TryBuildSyntheticCursor(player, playerPos, syntheticHitPosition, syntheticLiquidType))
		{
			//! Do not immediately drop a previously valid cursor. A single frame can
			//! fail because the camera ray is shallow or surface detection missed.
			if (TryUseCachedSyntheticCursor(player, playerPos, currentHitPosition, resolvedHitPosition, resolvedLiquidType))
			{
				#ifdef DIAG_DEVELOPER
				if (m_DebugSyntheticCursor)
					DebugDrawSyntheticCursor(player, currentHitPosition, resolvedHitPosition, true, "accepted cached synthetic cursor", resolvedLiquidType);
				#endif

				return true;
			}

			ClearSyntheticCursorCache();

			#ifdef DIAG_DEVELOPER
			if (m_DebugSyntheticCursor)
				DebugDrawSyntheticCursor(player, currentHitPosition, syntheticHitPosition, false, "synthetic cursor failed", LIQUID_NONE);
			#endif

			return false;
		}

		//! Validate the generated cursor once.
		if (!TryValidateHitCursor(player, playerPos, syntheticHitPosition, resolvedHitPosition, resolvedLiquidType))
		{
			ClearSyntheticCursorCache();

			#ifdef DIAG_DEVELOPER
			if (m_DebugSyntheticCursor)
				DebugDrawSyntheticCursor(player, currentHitPosition, syntheticHitPosition, false, "synthetic cursor invalid after build", syntheticLiquidType);
			#endif

			return false;
		}

		#ifdef DIAG_DEVELOPER
		if (m_DebugSyntheticCursor)
			DebugDrawSyntheticCursor(player, currentHitPosition, resolvedHitPosition, true, "accepted synthetic cursor", resolvedLiquidType);
		#endif

		return true;
	}

	protected bool TryBuildSyntheticCursor(PlayerBase player, vector playerPos, out vector syntheticHitPosition, out int syntheticLiquidType)
	{
		syntheticHitPosition = vector.Zero;
		syntheticLiquidType = LIQUID_NONE;

		//! Client only. Server validates synced cursor position.
		if (g_Game.IsHeadlessOrDedicatedServer())
			return false;

		//! One camera query for both paths.
		vector cameraPos, cameraDir;
		if (!TryGetCameraRay(cameraPos, cameraDir))
			return false;

		if (m_CanBeSaltwater && TryBuildSeaLevelHitPosition(player, playerPos, cameraPos, cameraDir, syntheticHitPosition))
		{
			syntheticLiquidType = LIQUID_SALTWATER;
			return true;
		}

		if (m_CanBeFreshwater && TryBuildPondHitPosition(player, playerPos, cameraPos, cameraDir, syntheticHitPosition))
		{
			//! Let final validation detect exact liquid type.
			syntheticLiquidType = LIQUID_FRESHWATER;
			return true;
		}

		return false;
	}

	protected bool TryGetCameraRay(out vector cameraPos, out vector cameraDir)
	{
		cameraPos = g_Game.GetCurrentCameraPosition();
		cameraDir = g_Game.GetCurrentCameraDirection();

		if (cameraPos == vector.Zero || cameraDir == vector.Zero)
			return false;

		cameraDir.Normalize();
		return true;
	}

	protected bool TryBuildSeaLevelHitPosition(PlayerBase player, vector playerPos, vector cameraPos, vector cameraDir, out vector seaHitPosition)
	{
		seaHitPosition = vector.Zero;

		float seaLevel, seaLevelMax;
		m_Probe.GetSeaLevels(seaLevel, seaLevelMax);

		if (playerPos[1] > seaLevelMax + m_MaxVerticalReach)
			return false;

		//! Dynamic minimum downward angle. If the ray is too close to horizontal,
		//! tiny camera sway causes huge sea cursor jumps. Only reject near
		//! horizontal aim, the real intersection check below decides range.
		if (cameraDir[1] > -SEA_CURSOR_MIN_DOWN_DOT)
		{
			#ifdef DIAG_DEVELOPER
			if (m_DebugSyntheticCursor)
			{
				DbgUI.Begin("", 10, 200);
				DbgUI.Text(string.Format("CCTLiquidSyntheticCursor TryBuildSeaLevelHitPosition rejected: reason=camera too shallow | cameraDirY=%1 | minStableDownDot=%2", cameraDir[1], SEA_CURSOR_MIN_DOWN_DOT));
				DbgUI.End();
			}
			#endif

			return false;
		}

		if (cameraPos[1] - seaLevel <= 0.0)
			return false;

		//! Real intersection of camera ray with sea plane.
		float t = (seaLevel - cameraPos[1]) / cameraDir[1];
		if (t <= 0)
			return false;

		vector candidate = cameraPos + cameraDir * t;
		candidate[1] = seaLevel;

		//! If the real cursor/sea intersection is beyond allowed range, do not
		//! clamp a closer fake point.
		if (vector.DistanceSq(playerPos, candidate) > m_SafeSeaDistanceSq)
		{
			#ifdef DIAG_DEVELOPER
			if (m_DebugSyntheticCursor)
			{
				DbgUI.Begin("", 20, 200);
				DbgUI.Text(string.Format("CCTLiquidSyntheticCursor TryBuildSeaLevelHitPosition rejected: reason=Real sea intersection too far | player=%1 | candidate=%2", playerPos.ToString(), candidate.ToString()));
				DbgUI.End();
			}
			#endif

			return false;
		}

		if (!IsSeaHitPositionValid(playerPos, candidate))
			return false;

		if (!HasClearPathToLiquidSurface(player, GetRaycastTarget(candidate)))
			return false;

		//! Apply small deadband/sticky cache.
		vector stableCandidate = ApplySyntheticHitCursorStability(playerPos, candidate, LIQUID_SALTWATER);

		//! Re-validating is only needed when the deadband actually moved the point,
		//! which is the uncommon case. This saves a second raycast per frame.
		if (!IsNearlyEqual(stableCandidate, candidate))
		{
			if (!IsSeaHitPositionValid(playerPos, stableCandidate))
				return false;

			if (!HasClearPathToLiquidSurface(player, GetRaycastTarget(stableCandidate)))
				return false;
		}

		seaHitPosition = stableCandidate;
		return true;
	}

	protected bool TryBuildPondHitPosition(PlayerBase player, vector playerPos, vector cameraPos, vector cameraDir, out vector pondHitPosition)
	{
		#ifdef DIAG_DEVELOPER
		float debugBestDelta = 999999.0;
		int debugSampleCount = 0;
		int debugValidKnownSurfaceCount = 0;
		string debugPondFailReason = "no pond samples";
		#endif

		pondHitPosition = vector.Zero;

		//! If looking upward/horizontal, we are probably not aiming at a pond surface.
		if (cameraDir[1] > POND_MAX_CAMERA_DIR_Y)
			return false;

		bool rayDescends = cameraDir[1] < 0;

		vector bestSurfacePos = vector.Zero;
		int bestLiquidType = LIQUID_NONE;
		float bestDelta = 999999.0;

		for (float dist = 1.0; dist <= m_SafeCursorDistance; dist += SYNTHETIC_POND_SAMPLE_STEP)
		{
			#ifdef DIAG_DEVELOPER
			debugSampleCount++;
			#endif

			vector probePos = cameraPos + cameraDir * dist;

			vector surfacePos;
			int liquidType;

			if (!TryGetLiquidSurfacePosAt(probePos, surfacePos, liquidType))
			{
				#ifdef DIAG_DEVELOPER
				debugPondFailReason = "TryGetLiquidSurfaceAt failed";
				#endif
				continue;
			}

			if (!IsAllowedPondLiquidSurface(playerPos, surfacePos, liquidType))
			{
				#ifdef DIAG_DEVELOPER
				debugPondFailReason = "known pond surface invalid";
				#endif
				continue;
			}

			#ifdef DIAG_DEVELOPER
			debugValidKnownSurfaceCount++;
			#endif

			float rayToSurfaceDelta = Math.AbsFloat(probePos[1] - surfacePos[1]);
			if (rayToSurfaceDelta < bestDelta)
			{
				bestDelta = rayToSurfaceDelta;
				bestSurfacePos = surfacePos;
				bestLiquidType = liquidType;

				#ifdef DIAG_DEVELOPER
				debugBestDelta = bestDelta;
				debugPondFailReason = "best pond surface too far from camera ray";
				#endif
			}

			//! The ray is close enough to the liquid surface.
			if (rayToSurfaceDelta <= SYNTHETIC_POND_SURFACE_TOLERANCE)
			{
				surfacePos = ApplySyntheticHitCursorStability(playerPos, surfacePos, liquidType);

				if (!IsValidPondHitPosition(playerPos, surfacePos, liquidType))
					continue;

				if (!HasClearPathToLiquidSurface(player, GetRaycastTarget(surfacePos)))
					continue;

				pondHitPosition = surfacePos;
				return true;
			}

			//! A descending ray that already passed under the surface only gets
			//! further away with every following sample, stop marching.
			if (rayDescends && probePos[1] < surfacePos[1] && rayToSurfaceDelta > SYNTHETIC_POND_FALLBACK_SURFACE_TOLERANCE)
				break;
		}

		//! Last chance: use closest sampled surface if it was very close.
		if (!IsNearlyZero(bestSurfacePos) && bestDelta <= SYNTHETIC_POND_FALLBACK_SURFACE_TOLERANCE)
		{
			bestSurfacePos = ApplySyntheticHitCursorStability(playerPos, bestSurfacePos, bestLiquidType);
			if (IsValidPondHitPosition(playerPos, bestSurfacePos, bestLiquidType))
			{
				if (!HasClearPathToLiquidSurface(player, GetRaycastTarget(bestSurfacePos)))
					return false;

				pondHitPosition = bestSurfacePos;
				return true;
			}
		}

		#ifdef DIAG_DEVELOPER
		if (m_DebugSyntheticCursor)
		{
			DbgUI.Begin("", 50, 240);
			DbgUI.Text(string.Format("CCTLiquidSyntheticCursor TryBuildPondHitPosition failed: reason=%1 | samples=%2 | validSurfaces=%3 | bestDelta=%4 | tolerance=%5 | relaxedTolerance=%6", debugPondFailReason, debugSampleCount, debugValidKnownSurfaceCount, debugBestDelta, SYNTHETIC_POND_SURFACE_TOLERANCE, SYNTHETIC_POND_FALLBACK_SURFACE_TOLERANCE));
			DbgUI.End();
		}
		#endif

		return false;
	}
	
	protected bool TryValidateHitCursor(PlayerBase player, vector playerPos, vector hitPosition, out vector resolvedHitPosition, out int resolvedLiquidType)
	{
		resolvedHitPosition = hitPosition;
		resolvedLiquidType = LIQUID_NONE;

		if (IsNearlyZero(hitPosition))
			return false;

		//! Saltwater path. This normalizes synced sea hits like <x, 0.546, z> back
		//! onto the sea plane.
		if (m_CanBeSaltwater)
		{
			vector normalizedSeaHit;
			if (TryGetNormalizedSeaHitPoint(playerPos, hitPosition, normalizedSeaHit))
			{
				//! Cheap terrain-over-water test before the raycast.
				if (!IsCursorActuallyOnLiquidSurface(hitPosition, normalizedSeaHit))
					return false;

				if (!HasClearPathToLiquidSurface(player, normalizedSeaHit))
					return false;

				resolvedHitPosition = normalizedSeaHit;
				resolvedLiquidType = LIQUID_SALTWATER;
				return true;
			}
		}

		//! Freshwater / pond path.
		if (m_CanBeFreshwater)
		{
			vector surfacePos;
			int detectedLiquidType;

			if (TryValidatePondHitPosition(playerPos, hitPosition, surfacePos, detectedLiquidType))
			{
				if (!IsCursorActuallyOnLiquidSurface(hitPosition, surfacePos))
					return false;

				if (!HasClearPathToLiquidSurface(player, surfacePos))
					return false;

				resolvedHitPosition = surfacePos;
				resolvedLiquidType = detectedLiquidType;
				return true;
			}
		}

		return false;
	}

	protected bool TryValidatePondHitPosition(vector playerPos, vector hitPosition, out vector surfacePos, out int liquidType, int expectedLiquidType = LIQUID_NONE)
	{
		surfacePos = vector.Zero;
		liquidType = LIQUID_NONE;

		if (IsNearlyZero(hitPosition))
			return false;

		if (vector.DistanceSq(playerPos, hitPosition) > m_MaxActionDistanceSq)
			return false;

		if (!g_Game.SurfaceIsPond(hitPosition[0], hitPosition[2]))
			return false;

		//! Use detected liquid as the source of truth.
		if (!TryGetLiquidSurfacePosAt(hitPosition, surfacePos, liquidType))
			return false;

		//! If caller expected a specific liquid mask/type, require compatibility.
		if (expectedLiquidType != LIQUID_NONE && (liquidType & expectedLiquidType) == 0)
			return false;

		if (Math.AbsFloat(hitPosition[1] - surfacePos[1]) > SYNTHETIC_POND_SURFACE_TOLERANCE)
			return false;

		return IsAllowedPondLiquidSurface(playerPos, surfacePos, liquidType);
	}

	protected bool IsValidPondHitPosition(vector playerPos, vector hitPosition, int liquidType = LIQUID_NONE)
	{
		vector surfacePos;
		int validatedLiquidType;

		return TryValidatePondHitPosition(playerPos, hitPosition, surfacePos, validatedLiquidType, liquidType);
	}

	protected bool IsSeaHitPositionValid(vector playerPos, vector hitPosition)
	{
		vector normalizedHit;
		return TryGetNormalizedSeaHitPoint(playerPos, hitPosition, normalizedHit);
	}

	protected bool IsAllowedPondLiquidSurface(vector playerPos, vector surfacePos, int liquidType)
	{
		if (liquidType == LIQUID_NONE || liquidType == LIQUID_SALTWATER)
			return false;

		if ((liquidType & m_AllowedLiquidSource) == 0)
			return false;

		if (Math.AbsFloat(playerPos[1] - surfacePos[1]) > m_MaxVerticalReach)
			return false;

		if (vector.DistanceSq(playerPos, surfacePos) > m_MaxActionDistanceSq)
			return false;

		//! Engine query last.
		return g_Game.SurfaceIsPond(surfacePos[0], surfacePos[2]);
	}

	protected void ClearSyntheticCursorCache()
	{
		m_LastSyntheticHitCursor = vector.Zero;
		m_HasLastSyntheticHitCursor = false;
		m_LastSyntheticHitCursorTime = 0;
	}

	protected vector ApplySyntheticHitCursorStability(vector playerPos, vector candidate, int liquidType)
	{
		int now = g_Game.GetTime();

		if (m_HasLastSyntheticHitCursor && (now - m_LastSyntheticHitCursorTime) <= SEA_CURSOR_STICK_TIME_MS)
		{
			if (vector.DistanceSq(candidate, m_LastSyntheticHitCursor) <= m_StickDistanceSq)
			{
				bool cachedStillValid;
				if (liquidType == LIQUID_SALTWATER)
					cachedStillValid = IsSeaHitPositionValid(playerPos, m_LastSyntheticHitCursor);
				else
					cachedStillValid = IsValidPondHitPosition(playerPos, m_LastSyntheticHitCursor, liquidType);

				if (cachedStillValid)
				{
					m_LastSyntheticHitCursorTime = now;
					return m_LastSyntheticHitCursor;
				}
			}
		}

		m_LastSyntheticHitCursor = candidate;
		m_HasLastSyntheticHitCursor = true;
		m_LastSyntheticHitCursorTime = now;

		return candidate;
	}

	protected bool TryUseCachedSyntheticCursor(PlayerBase player, vector playerPos, vector currentHitPosition, out vector resolvedHitPosition, out int resolvedLiquidType)
	{
		resolvedHitPosition = vector.Zero;
		resolvedLiquidType = LIQUID_NONE;

		if (!m_HasLastSyntheticHitCursor)
			return false;

		int now = g_Game.GetTime();

		//! Cache is only a short grace fallback, not a persistent targeting source.
		if ((now - m_LastSyntheticHitCursorTime) > SYNTHETIC_CURSOR_CACHE_MAX_AGE_MS)
			return false;

		//! Important: cached cursor must still match current aim.
		if (!IsCachedSyntheticCursorRelevant(currentHitPosition, m_LastSyntheticHitCursor))
			return false;

		if (!TryValidateHitCursor(player, playerPos, m_LastSyntheticHitCursor, resolvedHitPosition, resolvedLiquidType))
			return false;

		m_LastSyntheticHitCursorTime = now;
		return true;
	}

	protected bool IsCachedSyntheticCursorRelevant(vector currentHitPosition, vector cachedHitPosition)
	{
		if (cachedHitPosition == vector.Zero)
			return false;

		//! If there is a current real hit, cached point must be near that hit. If it
		//! is not, do not let an old water point keep the action alive.
		if (currentHitPosition != vector.Zero)
			return vector.DistanceSq(currentHitPosition, cachedHitPosition) <= m_CacheAimToleranceSq;

		//! If there is no real hit, compare cached point to current camera ray.
		return IsPositionNearCurrentCameraRay(cachedHitPosition, SYNTHETIC_CURSOR_CACHE_AIM_TOLERANCE);
	}

	protected bool IsPositionNearCurrentCameraRay(vector position, float maxPerpendicularDistance)
	{
		if (position == vector.Zero)
			return false;

		vector cameraPos, cameraDir;
		if (!TryGetCameraRay(cameraPos, cameraDir))
			return false;

		float projectedDistance = vector.Dot(position - cameraPos, cameraDir);

		//! Cached point must be in front of camera.
		if (projectedDistance <= 0.0)
			return false;

		//! Do not allow using a cached point far beyond the active aim range.
		if (projectedDistance > m_SafeCursorDistance + maxPerpendicularDistance)
			return false;

		vector closestPointOnRay = cameraPos + cameraDir * projectedDistance;
		return vector.DistanceSq(position, closestPointOnRay) <= (maxPerpendicularDistance * maxPerpendicularDistance);
	}

	//! Liquid points are raycast against slightly above the surface.
	protected vector GetRaycastTarget(vector surfacePos)
	{
		vector result = surfacePos;
		result[1] = result[1] + LIQUID_SURFACE_RAYCAST_OFFSET;
		return result;
	}

	protected bool IsNearlyEqual(vector a, vector b)
	{
		return vector.DistanceSq(a, b) < 0.0001;
	}

	#ifdef DIAG_DEVELOPER
	protected void ClearDebugCursorShapes()
	{
		if (!m_DebugSyntheticCursorShapes)
			return;

		for (int i = 0; i < m_DebugSyntheticCursorShapes.Count(); i++)
		{
			Shape shape = m_DebugSyntheticCursorShapes[i];
			if (shape)
				Debug.RemoveShape(shape);
		}

		m_DebugSyntheticCursorShapes.Clear();
	}

	protected void DebugDrawSyntheticCursor(PlayerBase player, vector originalHitPosition, vector cursorHitPosition, bool accepted, string reason, int liquidType = LIQUID_NONE)
	{
		ClearDebugCursorShapes();

		if (!player)
			return;

		vector playerPos = player.GetPosition();
		float headHeight = MiscGameplayFunctions.GetPlayerHeadHeight(player);
		vector headPos = playerPos;
		headPos[1] = headPos[1] + headHeight;
		
		int colorLine = ARGB(255, 0, 180, 255);
		int colorOriginal = ARGB(255, 255, 255, 0);

		int finalColor;
		if (accepted)
			finalColor = ARGB(255, 0, 255, 64);
		else
			finalColor = ARGB(255, 255, 32, 32);

		//! Main synthetic cursor point
		if (!IsNearlyZero(cursorHitPosition))
		{
			m_DebugSyntheticCursorShapes.Insert(Debug.DrawSphere(cursorHitPosition, 0.35, finalColor, ShapeFlags.NOZBUFFER));
			m_DebugSyntheticCursorShapes.Insert(Debug.DrawLine(headPos, cursorHitPosition, colorLine));

			//! Small cross on the water plane
			vector x1 = cursorHitPosition;
			vector x2 = cursorHitPosition;
			vector z1 = cursorHitPosition;
			vector z2 = cursorHitPosition;

			x1[0] = x1[0] - 0.5;
			x2[0] = x2[0] + 0.5;
			z1[2] = z1[2] - 0.5;
			z2[2] = z2[2] + 0.5;

			m_DebugSyntheticCursorShapes.Insert(Debug.DrawLine(x1, x2, finalColor));
			m_DebugSyntheticCursorShapes.Insert(Debug.DrawLine(z1, z2, finalColor));
		}

		//! Original hit position, if any
		if (!IsNearlyZero(originalHitPosition))
		{
			m_DebugSyntheticCursorShapes.Insert(Debug.DrawSphere(originalHitPosition, 0.2, colorOriginal, ShapeFlags.NOZBUFFER));
			m_DebugSyntheticCursorShapes.Insert(Debug.DrawLine(playerPos, originalHitPosition, colorOriginal));
		}

		float dist = 0.0;
		bool isSea = false;
		bool isPond = false;

		if (!IsNearlyZero(cursorHitPosition))
		{
			dist = vector.Distance(playerPos, cursorHitPosition);
			isSea = g_Game.SurfaceIsSea(cursorHitPosition[0], cursorHitPosition[2]);
			isPond = g_Game.SurfaceIsPond(cursorHitPosition[0], cursorHitPosition[2]);
		}

		DbgUI.Begin("", 30, 200);
		{
			DbgUI.Text(string.Format("CCTLiquidSyntheticCursor TryResolveSyntheticCursor: accepted=%1 | reason=%2 | liquid=%3 | player=%4 | originalHit=%5 | resolvedHit=%6 | dist=%7 | isSea=%8 | isPond=%9", accepted, reason, liquidType, playerPos.ToString(), originalHitPosition.ToString(), cursorHitPosition.ToString(), dist, isSea, isPond));
		}
		DbgUI.End();
	}
	#endif
}
