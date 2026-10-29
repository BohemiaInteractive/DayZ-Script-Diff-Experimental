#ifdef DIAG_DEVELOPER

class ConstructionDebug
{
	//Context: ConstructionBase.CanDismantlePart
	protected static string m_DismantlePartName;
	protected static bool m_DismantleConstructed;
	protected static bool m_DismantleHasDependents;
	protected static bool m_DismantleSupportFailed;

	//Context: ActionDismantlePart
	protected static string m_ActionDismantlePartName;
	protected static string m_ActionDismantleReason;

	//Context: ActionPartInfo
	protected static int m_TargetComponentIdx = -1;
	protected static ref map<int, ref ActionInfoDebugData> m_ActionInfoDebug = new map<int, ref ActionInfoDebugData>();
	
	//Context: Collision printout data
	protected static string m_CollidingPartName;
	protected static ref array<string> m_ValidCollisions = new array<string>();
	
	//Context: Item falling printout data
	protected static string m_ItemFallPartName;
	protected static int m_ItemFallTotalCount;
	protected static int m_ItemFallWaveCount;
	protected static ref array<string> m_ItemFallLines = new array<string>();
	
	//Context: Item falling debug frame lease; constructions stop refreshing the tick once their part is no longer targeted
	protected static int m_ItemFallDebugTick;
	protected static ref array<ConstructionBase> m_ItemFallDebugActive = new array<ConstructionBase>();
	
	//Context: Insidebox printout data
	protected static string m_InsideboxPartName;
	
	static void ResetValues()
	{
		m_DismantlePartName = "";
		m_DismantleConstructed = false;
		m_DismantleHasDependents = false;
		m_DismantleSupportFailed = false;
		
		m_ActionDismantlePartName = "";
		m_ActionDismantleReason = "";
		
		m_TargetComponentIdx = -1;
		m_ActionInfoDebug.Clear();
		
		m_CollidingPartName = "";
		m_ValidCollisions.Clear();
		
		m_ItemFallPartName = "";
		m_ItemFallTotalCount = 0;
		m_ItemFallWaveCount = 0;
		m_ItemFallLines.Clear();
		
		m_InsideboxPartName = "";
	}

	static void SetCanDismantlePartData(ConstructionDismantleDebugData data)
	{
		m_DismantlePartName = data.partName;
		m_DismantleConstructed = data.constructed;
		m_DismantleHasDependents = data.hasDependents;
		m_DismantleSupportFailed = !data.supportState;
	}
	
	static void ResetDismantlePartData()
	{
		m_DismantlePartName = "";
	}

	static void SetActionDismantleData(ActionDismantleDebugData data)
	{
		m_ActionDismantlePartName = data.partName;
		m_ActionDismantleReason = data.reason;
	}
	
	static void ResetActionDismantleData()
	{
		m_ActionDismantlePartName = "";
	}

	static void AddBuildPartInfoData(ActionInfoDebugData data)
	{
		UpdateTargetComponentIdx(data.componentIdx);
		
		m_ActionInfoDebug.Set(data.partName.Hash(), data);
	}
	
	static void RemoveBuildPartInfoData(string partName) //?
	{
		m_ActionInfoDebug.Remove(partName.Hash());
	}
	
	protected static void UpdateTargetComponentIdx(int idx)
	{
		if (m_TargetComponentIdx != idx)
		{
			m_ActionInfoDebug.Clear();
			m_TargetComponentIdx = idx;
		}
	}
	
	static void SetCollisionPrintoutData(string partName, notnull array<string> collisions)
	{
		m_CollidingPartName = partName;
		m_ValidCollisions.Copy(collisions);
	}
	
	static void ResetCollisionPrintoutData()
	{
		m_CollidingPartName = "";
		m_ValidCollisions.Clear();
	}
	
	static void SetItemFallDebugData(string partName, int totalAffected, int waveCount, notnull array<string> lines)
	{
		m_ItemFallPartName = partName;
		m_ItemFallTotalCount = totalAffected;
		m_ItemFallWaveCount = waveCount;
		m_ItemFallLines.Copy(lines);
	}
	
	static void ResetItemFallDebugData()
	{
		m_ItemFallPartName = "";
		m_ItemFallTotalCount = 0;
		m_ItemFallWaveCount = 0;
		m_ItemFallLines.Clear();
	}
	
	static void SetInsideboxCheckDebugData(string partName)
	{
		m_InsideboxPartName = partName;
	}
	
	static void Update()
	{
		m_ItemFallDebugTick++;
		SweepItemFallDebugStale();
		
		UpdateFreeCameraItemFallDebug();
		
		//full debug panels only under the full debug flag, item falling panel under its own flag
		if (DiagMenu.GetBool(DiagMenuIDs.BASEBUILDING_FULLDEBUG))
		{
			DrawCanDismantlePart();
			DrawActionDismantle();
			DrawBuildPartInfo();
			DrawCollisionPrintouts();
			DrawInsideboxPrintouts();
		}
		
		DrawItemFallPrintouts();
	}
	
	//! constructions whose debugged part is no longer targeted stop refreshing the tick, clean them up so their shapes do not linger
	static void SweepItemFallDebugStale()
	{
		for (int i = m_ItemFallDebugActive.Count() - 1; i >= 0; i--)
		{
			ConstructionBase construction = m_ItemFallDebugActive[i];
			if (!construction || m_ItemFallDebugTick - construction.GetItemFallDebugLastTick() > ConstructionBase.ITEM_FALL_DEBUG_STALE_FRAMES)
			{
				m_ItemFallDebugActive.Remove(i);
				if (construction)
					construction.ResetItemFallDebugVisuals();
			}
		}
	}
	
	static int GetItemFallDebugTick()
	{
		return m_ItemFallDebugTick;
	}
	
	static void RegisterItemFallDebug(ConstructionBase construction)
	{
		if (construction && m_ItemFallDebugActive.Find(construction) < 0)
			m_ItemFallDebugActive.Insert(construction);
	}
	
	static void UnregisterItemFallDebug(ConstructionBase construction)
	{
		int idx = m_ItemFallDebugActive.Find(construction);
		if (idx >= 0)
			m_ItemFallDebugActive.Remove(idx);
	}
	
	//! while the player flies the free camera the user actions are disabled and the action based drivers never run,
	//! so raycast from the free camera and drive the item fall debug for the part under the crosshair instead
	static void UpdateFreeCameraItemFallDebug()
	{
		if (g_Game.IsDedicatedServer())
			return;
		
		if (!DiagMenu.GetBool(DiagMenuIDs.BASEBUILDING_ITEMFALLDEBUG))
			return;
		
		if (!DeveloperFreeCamera.IsFreeCameraEnabledNoPlayerMovement())
			return;
		
		FreeDebugCamera camera = FreeDebugCamera.GetInstance();
		if (!camera)
			return;
		
		vector rayStart = camera.GetPosition();
		vector rayEnd = rayStart + camera.GetDirection() * ConstructionBase.ITEM_FALL_DEBUG_FRECAM_RAY_DIST;
		
		RaycastRVParams rayInput = new RaycastRVParams(rayStart, rayEnd, camera);
		rayInput.flags = CollisionFlags.ALLOBJECTS;
		array<ref RaycastRVResult> results = new array<ref RaycastRVResult>();
		if (!DayZPhysics.RaycastRVProxy(rayInput, results))
			return;
		
		//nearest construction capable hit wins
		RaycastRVResult nearestRes = null;
		float nearestDist = 0.0;
		int i;
		int resCount = results.Count();
		for (i = 0; i < resCount; i++)
		{
			RaycastRVResult res = results.Get(i);
			EntityAI ent;
			if (!res || !res.obj || !Class.CastTo(ent, res.obj) || !ent.CanUseConstruction())
				continue;
			
			float dist = vector.DistanceSq(res.pos, rayStart);
			if (!nearestRes || dist < nearestDist)
			{
				nearestRes = res;
				nearestDist = dist;
			}
		}
		
		if (!nearestRes)
			return;
		
		EntityAI targetEntity;
		if (!Class.CastTo(targetEntity, nearestRes.obj))
			return;
		
		ConstructionBase construction = ConstructionBase.Cast(targetEntity.GetConstructionBasic());
		if (!construction)
			return;
		
		//first component name mapping to a constructed part, tool and dismantle checks do not apply for the preview,
		//UpdateItemFallDebugVisuals mirrors the server gates and prints the suppression reasons
		array<string> compNames = new array<string>();
		targetEntity.GetActionComponentNameList(nearestRes.component, compNames);
		int compCount = compNames.Count();
		for (i = 0; i < compCount; i++)
		{
			if (construction.GetConstructionPart(compNames[i]))
			{
				construction.UpdateItemFallDebugVisuals(compNames[i]);
				return;
			}
		}
	}

	protected static void DrawCanDismantlePart()
	{
		if (m_DismantlePartName != "")
		{
			DbgUI.Begin("CanDismantlePart Generic Debug", 0, 0);
			{
				DbgUI.Text("cannot dismantle: " + m_DismantlePartName);
				if (!m_DismantleConstructed)
					DbgUI.Text("     not constructed");
				if (m_DismantleHasDependents)
					DbgUI.Text("     has dependent part/s");
				if (m_DismantleSupportFailed)
					DbgUI.Text("     support check failed");
			}
			DbgUI.End();
		}
	}

	protected static void DrawActionDismantle()
	{
		if (m_ActionDismantlePartName != "")
		{
			DbgUI.Begin("ActionDismantlePart Specific Debug", 0, 200);
			{
				DbgUI.Text("cannot dismantle: " + m_ActionDismantlePartName);
				DbgUI.Text("reason: " + m_ActionDismantleReason);
			}
			DbgUI.End();
		}
	}

	protected static void DrawBuildPartInfo()
	{
		if (m_TargetComponentIdx == -1 && m_ActionInfoDebug.Count() == 0)
			return;
		
		DbgUI.Begin("ActionPartInfo Debug", 400, 0);
		{
			DbgUI.Text("target component idx: " + m_TargetComponentIdx);
			DbgUI.Spacer(10);
			
			foreach (int hash, ref ActionInfoDebugData data : m_ActionInfoDebug)
			{
				if (data.failureMask & EConstructionInfoCategories.PART_MISSING_REQUIREMENT)
					DbgUI.Text(data.partName + " - missing required part/s!");
				if (data.failureMask & EConstructionInfoCategories.PART_MISSING_SUPPORT)
					DbgUI.Text(data.partName + " - missing statics support/s!");
				if (data.failureMask & EConstructionInfoCategories.PART_CONFLICTED)
					DbgUI.Text(data.partName + " - conflicts with part: " + data.conflictingPart + "!");
				if (data.failureMask & EConstructionInfoCategories.MATERIALS_PRESENCE)
					DbgUI.Text(data.partName + " - missing material in slot: " + data.missingSlot + " (qty: " + data.missingQty + ")!");
				if (data.failureMask & EConstructionInfoCategories.MATERIALS_DAMAGE)
					DbgUI.Text(data.partName + " - material/s ruined!");
				if (data.failureMask & EConstructionInfoCategories.TOOL_PRESENCE)
					DbgUI.Text(data.partName + " - not buildable with: " + data.badToolType + "!");
				DbgUI.Spacer(10);
			}
		}
		DbgUI.End();
	}
	
	protected static void DrawCollisionPrintouts()
	{
		if (m_CollidingPartName != "")
		{
			DbgUI.Begin("IsCollidingEx Debug", 0, 300);
			{
				DbgUI.Text("part " + m_CollidingPartName + " blocked by:");
				foreach (string name : m_ValidCollisions)
				{
					DbgUI.Text("    " + name);
				}
			}
			DbgUI.End();
		}
	}
	
	protected static void DrawItemFallPrintouts()
	{
		if (m_ItemFallPartName != "")
		{
			DbgUI.Begin("ItemFall Debug", 0, 450);
			{
				DbgUI.Text(string.Format("part %1 removed: %2 affected over %3 wave(s):", m_ItemFallPartName, m_ItemFallTotalCount, m_ItemFallWaveCount));
				DbgUI.Text("colors: wave 0=yellow, wave 1=green, wave 2=blue, wave 3+=white");
				foreach (string line : m_ItemFallLines)
				{
					DbgUI.Text(line);
				}
			}
			DbgUI.End();
		}
	}
	
	protected static void DrawInsideboxPrintouts()
	{
		if (m_InsideboxPartName != "")
		{
			DbgUI.Begin("Insidebox Debug", 800, 0);
			{
				DbgUI.Text("insidebox check failed for part: " + m_InsideboxPartName);
			}
			DbgUI.End();
		}
	}
}

class ConstructionDismantleDebugData
{
	string partName;
	bool constructed;
	bool hasDependents;
	bool supportState;
}

class ActionDismantleDebugData
{
	string partName;
	string reason;
}

class ActionInfoDebugData
{
	string partName;
	int failureMask;
	int componentIdx; //target component idx
	string conflictingPart;
	string missingSlot;
	float missingQty;
	string badToolType;
}

//------------------------------

class CharacterStepsDebug
{
	protected static string m_BoneName;
	protected static string m_LastSurfaceType;
	
	static void Update()
	{
		DrawStepPrintouts();
	}
	
	static void SetStepData(string boneName, string lastSurfaceName)
	{
		m_BoneName = boneName;
		m_LastSurfaceType = lastSurfaceName;
	}
	
	protected static void DrawStepPrintouts()
	{
		if (m_BoneName != "")
		{
			DbgUI.Begin("PlayerSurface Debug", 10, 10);
			{
				DbgUI.Text("Leg: " + m_BoneName);
				DbgUI.Text("Last surface type: " + m_LastSurfaceType);
			}
			DbgUI.End();
		}
	}
}

#endif
