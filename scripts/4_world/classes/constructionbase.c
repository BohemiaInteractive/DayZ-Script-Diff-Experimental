//! used to guide action sound selection, mostly. Could be also used for particles and stuff, if need be
enum ConstructionMaterialType
{
	MATERIAL_NONE	= 0,
	MATERIAL_LOG	= 1,
	MATERIAL_WOOD	= 2,
	MATERIAL_STAIRS	= 3,
	MATERIAL_METAL	= 4,
	MATERIAL_WIRE	= 5,
	MATERIAL_BRICK 	= 6,
	MATERIAL_RUBBLE = 7
}

enum EConstructionDirCheckType
{
	INDOORS,
	REFERENCE_DIRECTION,
	MISC
}

#ifdef DIAG_DEVELOPER
enum EConstructionDebugModes
{
	DISABLED,
	FAST,
	INSTANT
}
#endif

class ConstructionBase : ConstructionBasic
{
	static const float REPAIR_MATERIAL_PERCENTAGE = 0.15;
	static const float DECONSTURCT_MATERIAL_LOSS = 0.2;
	static const float DEFAULT_DECAY_RATE = -1.0; //! HP/s change
	static const float DEFAULT_DECAY_THRESHOLD = 0.3; //! Percentage of max health where the decay stops
	static const float DEFAULT_DECAY_RATE_COEF = 1.0; //! Default decay speed coef
	static const float SANDSTORM_DECAY_RATE_COEF = 1.5; //! Sandstorm decay speed coef
	
	protected ref map<string, ref ConstructionPart> m_ConstructionParts //! INSTANCED construction part data;
	protected ref array<ConstructionPart> m_ConstructionPartArray; //ordered array for easier access
	protected ref array<ConstructionPart> m_BuiltConstructionPartArray; //built parts array
	protected EntityAI 	m_EntityParent;
	protected bool m_AnyPartBuildableByHands;
	
	#ifdef DIAG_DEVELOPER
	//Debug
	protected Shape m_CollisionBox;
	protected Shape m_PlayerInsideTestCylinder;
	protected ref array<Shape> m_InsideTestBoxes;
	#endif
	
	//============================================
	// Construction
	//============================================	
	void ConstructionBase(EntityAI parent)
	{
		m_AnyPartBuildableByHands = false;
		m_ConstructionParts = new map<string, ref ConstructionPart>();
		m_ConstructionPartArray	= new array<ConstructionPart>();
		m_BuiltConstructionPartArray = new array<ConstructionPart>();
		
		#ifdef DIAG_DEVELOPER
		m_InsideTestBoxes = new array<Shape>();
		#endif
		
		//set parent object
		SetParent(parent);
	}
	
	void ~ConstructionBase()
	{
		ConstructionManagerStatic.RemoveFromProcessing(this);
		Cleanup();
	}
	
	override void Init()
	{
		UpdateConstructionParts();
		//InitBaseState(); //called from EEInit instead (physics updates require physics to be initialized)
	}
	
	override void EEInitConstruction()
	{
		if (m_EntityParent.IsPendingDeletion())
			return;
		
		foreach (ConstructionPart part : m_ConstructionPartArray)
		{
			part.PartInstanceDeferredInit(m_EntityParent);
		}
		
		#ifdef DIAG_DEVELOPER
		if ((g_Game.IsServer() || !g_Game.IsMultiplayer()) && IsCLIParam("RBLDRebuildEverythingWithLocks"))
		{
			RebuildingSpawner.RebuildAllWoodenPartsFiltered(this, m_EntityParent);
		}
		#endif
	}
	
	protected void Cleanup()
	{
		m_ConstructionParts = null;
		m_ConstructionPartArray	= null;
		m_BuiltConstructionPartArray = null;
		
		#ifdef DIAG_DEVELOPER
		m_InsideTestBoxes = null;
		#endif
	}
	
	protected void InitAttachmentGUIPartStructure();
	protected void InitAttachmentGUIPartLinks(ConstructionPart part);
	
	protected void SetParent(EntityAI parent)
	{
		m_EntityParent = parent;
	}
	
	//============================================
	// Syncing
	//============================================
	protected void HandleSyncDataBuild(ConstructionPart part);
	protected void HandleSyncDataDismantle(ConstructionPart part);
	protected void HandleSyncDataDestroy(ConstructionPart part);
	//! Returns 'true' if part state was locally changed
	protected bool UpdatePartSyncStateLocal(ConstructionPart part)
	{
		return false;
	}
	
	//============================================
	// Construction process
	//============================================
	//constructed parts
	void AddToConstructedParts(string part_name)
	{
		ConstructionPart part = GetConstructionPart(part_name);
		if (part)
		{
			if (LogManager.IsBaseBuildingLogEnable()) bsbDebugPrint("[bsb] Construction " + Object.GetDebugName(m_EntityParent) + " AddToConstructedParts part=" + part.GetPartName());
			part.SetBuiltState(true);
			part.TryHandleUtilityParts(m_EntityParent);
			if (m_BuiltConstructionPartArray.Find(part) == -1)
				m_BuiltConstructionPartArray.Insert(part);
			
			#ifndef SERVER
			if (m_EntityParent.IsInitialized())
				part.PlayConstructionEffects(m_EntityParent);
			#endif
		}
	}
	
	void RemoveFromConstructedParts(string part_name)
	{
		ConstructionPart part = GetConstructionPart(part_name);
		if (part) 
		{
			if (LogManager.IsBaseBuildingLogEnable()) bsbDebugPrint("[bsb] Construction " + Object.GetDebugName(m_EntityParent) + " RemoveFromConstructedParts part=" + part.GetPartName());
			part.SetBuiltState(false);
			part.TryHandleUtilityParts(m_EntityParent);
			m_BuiltConstructionPartArray.RemoveItem(part);
			
			#ifndef SERVER
			if (m_EntityParent.IsInitialized())
				part.PlayConstructionEffects(m_EntityParent);
			#endif
		}
	}
	
	/**
	\brief new build part method
	\note: 'player' CAN be null
	*/
	void BuildPartServerEx(Man player, string part_name, int action_id)
	{
		if (LogManager.IsBaseBuildingLogEnable()) bsbDebugPrint("[bsb] Construction BuildPartServerEx | " + part_name);
		//on action
		TakeMaterialsServer(part_name);
	}
	
	/**
	\brief new dismantle part method
	\note: 'player' CAN be null
	*/
	void DismantlePartServerEx(Man player, string part_name, int action_id)
	{
		string damageZone;
		DamageSystem.GetDamageZoneFromComponentName(m_EntityParent,part_name,damageZone);
		
		if (LogManager.IsBaseBuildingLogEnable()) bsbDebugPrint("[bsb] Construction DismantlePartServerEx | " + part_name);
		//receive materials
		if (player)
			ReceiveMaterialsServer(player, part_name, damageZone);
		
		//drop non-usable materials
		DropNonUsableMaterialsServer(player, part_name);
		DropEssentialAttachments(player, part_name);
		
		HandleEntityFalling(part_name);
	}
	
	//! destroy part method
	void DestroyPartServer(Man player, string part_name, int action_id, bool destroyed_by_connected_part = false)
	{
		if (LogManager.IsBaseBuildingLogEnable()) bsbDebugPrint("[bsb] Construction DestroyPartServer | " + part_name);
		//destroy attached materials (if locked)
		DestroyMaterialsServer(player, part_name);
		
		//drop non-usable materials
		DropNonUsableMaterialsServer(player, part_name);
		DropEssentialAttachments(player, part_name);
		
		HandleEntityFalling(part_name);
	}
	
	void DestroyConnectedParts(string part_name)
	{
		array<string> partNames;
		partNames = GetValidDepenentPartsArray(part_name);
		if (partNames && partNames.Count() > 0)
		{
			foreach (string partName : partNames)
			{
				if (!ExceptionCheck(partName))
				{
					if (LogManager.IsBaseBuildingLogEnable()) bsbDebugPrint("[bsb] Construction DestroyConnectedParts | " + partName);
					DestroyPartServer(null,partName,AT_DESTROY_PART,true);
				}
			}
		}
	}
	
	//! when supporting part is removed, entities on top of it should transform to surface Y height
	protected void HandleEntityFalling(string partName)
	{
		ConstructionPart part = GetConstructionPart(partName);
		if (part.CanSimulateItemFalling())
		{
			g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Call(ItemFall, partName); //deferred call to allow for material spawning/part's phx adjustment
		}
	}
	
	protected void ItemFall(string partName)
	{
		vector minMax[2];
		GetItemFallDetectionBoxData(partName, minMax);
		vector center = GetBoxCenter(minMax);
		center = m_EntityParent.ModelToWorld(center);		//convert to world coordinates
		vector edgeLength = GetCollisionBoxSize(minMax);
		vector orientation = m_EntityParent.GetOrientation();
		BoxCollidingParams params = new BoxCollidingParams();
		params.SetParams(center, orientation, edgeLength, ObjIntersect.View, ObjIntersect.Geom, false);
		
		array<Object> excludes; //null by default
		array<ref BoxCollidingResult> results = new array<ref BoxCollidingResult>();
		
		if (g_Game.IsBoxCollidingGeometryProxy(params, excludes, results))
		{
			foreach (BoxCollidingResult res : results)
			{
				Transport vehicle;
				if (Class.CastTo(vehicle, res.obj)) //special treatment for transports
				{
					dBodyActive(vehicle, ActiveState.ACTIVE);
					continue;
				}
				
				PlayerBase player;
				if (Class.CastTo(player, res.obj)) //special treatment for characters
				{
					player.GetPhysics().ApplyImpulse(-vector.Up);
					continue;
				}
				
				InventoryItem item;
				if (!Class.CastTo(item, res.obj))
					continue;
				
				item.ThrowPhysically(null, vector.Zero);
			}
		}
	}
	
	//! Handles decay for constructed parts, if enabled
	override void ProcessConstructionDecay(float dTime)
	{
		float healthTarget;
		float currentHP;
		float threshold;
		
		foreach (ConstructionPart part : m_ConstructionParts)
		{
			if (part.IsBuilt() && part.CanPartDecay())
			{
				string damageZone;
				DamageSystem.GetDamageZoneFromComponentName(m_EntityParent,part.GetPartName(),damageZone);
				if (damageZone != "")
				{
					currentHP = m_EntityParent.GetHealth(damageZone,"Health");
					threshold = part.GetPartDecayThreshold() * m_EntityParent.GetMaxHealth(damageZone,"Health");
					if (currentHP > threshold)
					{
						healthTarget = m_EntityParent.GetHealth(damageZone,"Health") + (dTime * part.GetPartDecayRate() * GetCurrentDecayCoef());
						healthTarget = Math.Clamp(healthTarget,threshold,m_EntityParent.GetMaxHealth(damageZone,"Health"));
						m_EntityParent.SetHealth(damageZone,"Health",healthTarget);
					}
				}
			}
		}
	}
	
	protected float GetCurrentDecayCoef()
	{
		if (g_Game.GetWeather().GetSandstorm().IsActive())
			return SANDSTORM_DECAY_RATE_COEF;
		
		return DEFAULT_DECAY_RATE_COEF;
	}
	
	array<ConstructionPart> GetConstructionPartArray()
	{
		return m_ConstructionPartArray;
	}
	
	override bool CanUseHandConstruction()
	{
		return m_AnyPartBuildableByHands != 0;
	}
	
	override void OnHealthLevelChanged(int oldLevel, int newLevel, string zone)
	{
		super.OnHealthLevelChanged(oldLevel, newLevel, zone);
		
		string partName = zone;
		partName.ToLower();
		
		if (newLevel == GameConstants.STATE_RUINED)
		{
			ConstructionPart constructionPart = GetConstructionPart(partName);
			
			if (constructionPart && IsPartConstructed(partName))
			{
				DestroyPartServer(null, partName, AT_DESTROY_PART);
			}
		}
	}
	
	//!Exceptions from 'dependent parts' hierarchy are handled here
	bool ExceptionCheck(string part_name)
	{
		return false;
	}
	
	//============================================
	// Construction-based event processing, not for BBB
	//============================================
	void ProcessOnPartBuiltServer(Man player, string part_name, int action_id)	
	{
		ConstructionPart part = GetConstructionPart(part_name);
		if (part.IsUtilityPart())
			return;
		
		string damageZone;
		if (DamageSystem.GetDamageZoneFromComponentName(m_EntityParent,part_name,damageZone))
		{
			m_EntityParent.SetAllowDamage(true);
			m_EntityParent.SetHealthMax(damageZone);
			m_EntityParent.ProcessInvulnerabilityCheck(m_EntityParent.GetInvulnerabilityTypeString());
		}
		
		part.OnBeforeBuildServer(m_EntityParent, player); //performs any checks specific part type may need
		HandleSyncDataBuild(part);
		
		UpdatePartSyncStateLocal(part);
		
		ConstructionManagerStatic.AddPartToProcess(this, part);
		
		part.OnBuiltServer(m_EntityParent, player);
		
		m_EntityParent.SetSynchDirty();
	}
	
	void ProcessOnPartDismantledServer(Man player, string part_name, int action_id)
	{
		ConstructionPart part = GetConstructionPart(part_name);
		part.OnBeforeDismantleServer(m_EntityParent, player); //performs any checks specific part type may need
		HandleSyncDataDismantle(part);
		
		UpdatePartSyncStateLocal(part);
		
		ConstructionManagerStatic.AddPartToProcess(this, part);
		
		part.OnDismantledServer(m_EntityParent, player);
		m_EntityParent.SetSynchDirty();
	}
	
	void ProcessOnPartDestroyedServer(Man player, string part_name, int action_id, bool destroyed_by_connected_part = false)
	{
		ConstructionPart part = GetConstructionPart(part_name);
		part.OnBeforeDestroyServer(m_EntityParent, player); //performs any checks specific part type may need
		HandleSyncDataDestroy(part);
		
		UpdatePartSyncStateLocal(part);
		
		ConstructionManagerStatic.AddPartToProcess(this, part);
		
		part.OnDestroyedServer(m_EntityParent, player);
		m_EntityParent.SetSynchDirty();
		
		DestroyConnectedParts(part_name);
	}
	
	//============================================
	// Update construction
	//============================================
	//!init ConstructionPart visuals
	void InitVisuals()
	{
		if (LogManager.IsBaseBuildingLogEnable()) bsbDebugPrint("[bsb] Construction " + Object.GetDebugName(m_EntityParent) + " InitVisuals");
		UpdateVisuals();
	}
	
	//!update SPECIFIC ConstructionPart visuals
	void UpdateSpecificPartVisuals(ConstructionPart part)
	{
		if (part.IsBuilt())
		{
			ShowConstructionPart(part.GetPartName());
		}
		else
		{
			HideConstructionPart(part.GetPartName());
		}
	}
	
	//!update ALL ConstructionPart visuals. Use only if the update queue system won't cut it.
	void UpdateVisuals()
	{
		if (LogManager.IsBaseBuildingLogEnable()) bsbDebugPrint("[bsb] Construction " + Object.GetDebugName(m_EntityParent) + " UpdateVisuals");
		
		foreach (ConstructionPart part : m_ConstructionPartArray)
		{
			if (part.IsBuilt())
			{
				ShowConstructionPart(part.GetPartName());
			}
			else
			{
				HideConstructionPart(part.GetPartName());
			}
		}
	}
	
	//!update SPECIFIC part's physics
	void UpdateSpecificPartPhysics(ConstructionPart part)
	{
		if (part.IsBuilt())
		{
			ShowConstructionPartPhysics(part.GetPartName());
		}
		else
		{
			HideConstructionPartPhysics(part.GetPartName());
		}
	}
	
	//!update ALL parts physics. Sets hide first, then shows the rest. Use only if the update queue system won't cut it.
	void UpdatePhysics()
	{
		if (LogManager.IsBaseBuildingLogEnable()) bsbDebugPrint("[bsb] Construction " + Object.GetDebugName(m_EntityParent));
		
		array<ConstructionPart> builtArray = new array<ConstructionPart>();
		foreach (ConstructionPart anyPart : m_ConstructionPartArray)
		{
			if (anyPart.IsBuilt())
				builtArray.Insert(anyPart);
			else
				UpdateSpecificPartPhysics(anyPart);
		}
		
		foreach (ConstructionPart builtPart : builtArray)
		{
			UpdateSpecificPartPhysics(builtPart);
		}
		
		builtArray = null;
	}
	
	void UpdateNavmesh()
	{
		g_Game.GetWorld().UpdatePathgraphEntityState(m_EntityParent);
	}
	
	//!Parse type data and create part instance data objects
	protected void UpdateConstructionParts()
	{
		m_ConstructionParts.Clear();
		m_ConstructionPartArray.Clear();
		m_BuiltConstructionPartArray.Clear();
		
		EntityAIType entityType;
		ConstructionDataTypeHolder holder;
		if (Class.CastTo(entityType, m_EntityParent.GetEntityType()) && Class.CastTo(holder, entityType.GetConstructionDataHolderBasic()))
		{
			InitAttachmentGUIPartStructure(); //TODO: move GUI stuff to type level maybe?
			
			array<ConstructionPartTypeData> partTypesArray = holder.GetConstructionPartTypeDataArray();
			foreach (ConstructionPartTypeData typeData : partTypesArray)
			{
				string partPath = typeData.GetPartPath();
				string spawnTypeStr;
				ConstructionPart part;
				//if no special treatment defined..
				if (!g_Game.ConfigGetText(partPath + " custom_part_type", spawnTypeStr) || !Class.CastTo(part, CreatePartInstanceObject(spawnTypeStr)))
				{
					//parameters are purely optional at this point, all config data is cached on entity type level
					part = CreatePartInstanceObject(GetDefaultPartInstanceType());
				}
				
				part.SetPartTypeData(typeData);
				PartInstanceInitData initData = new PartInstanceInitData();
				initData.m_PartPath = partPath;
				initData.m_ParentEnt = m_EntityParent;
				part.InitAdditionalPartInstanceInfo(initData);
				
				InitAttachmentGUIPartLinks(part);
				
				m_ConstructionParts.Insert(typeData.GetPartName(), part);
				m_ConstructionPartArray.Insert(part);
				m_AnyPartBuildableByHands |= part.GetBuildIndicationTypeMask() & EConstructionTools.TOOL_HANDS;
			}
		}
	}
	
	protected ConstructionPart CreatePartInstanceObject(string partType)
	{
		return ConstructionPart.Cast(partType.ToType().Spawn());
	}
	
	//! Override on lower implements to create desired part types
	protected string GetDefaultPartInstanceType()
	{
		ErrorEx("missing valid override!");
		return "";
	}
	
	protected void QueueFullUpdateParts(array<ConstructionPart> changedParts)
	{
		foreach (ConstructionPart part : changedParts)
		{
			ConstructionManagerStatic.AddPartToProcess(this, part);
		}
	}
	
	//============================================
	// Parts
	//============================================	
	map<string, ref ConstructionPart> GetConstructionParts()
	{
		return m_ConstructionParts;
	}
	
	ConstructionPart GetConstructionPart(string part_name)
	{
		return m_ConstructionParts.Get(part_name);
	}
	
	bool CanBuildPart(string part_name, ItemBase tool, bool use_tool)
	{
		int dump;
		return CanBuildPartEx(part_name, tool, dump);
	}
	
	//! not meant to replace, but rather extend the functionality for specific use
	bool CanBuildPartEx(string partName, ItemBase tool, out EConstructionInfoCategories failureReasonsMask)
	{
		failureReasonsMask = EConstructionInfoCategories.NONE;
		
		if (IsPartConstructed(partName))
			failureReasonsMask |= EConstructionInfoCategories.PART_CONSTRUCTED;
		if (!HasRequiredPart(partName))
			failureReasonsMask |= EConstructionInfoCategories.PART_MISSING_REQUIREMENT;
		if (!PartHasAnySupportFulfilled(partName))
			failureReasonsMask |= EConstructionInfoCategories.PART_MISSING_SUPPORT;
		if (HasConflictPart(partName))
			failureReasonsMask |= EConstructionInfoCategories.PART_CONFLICTED;
		if (!HasMaterials(partName))
			failureReasonsMask |= EConstructionInfoCategories.MATERIALS_PRESENCE;
		if (MaterialIsRuined(partName))
			failureReasonsMask |= EConstructionInfoCategories.MATERIALS_DAMAGE;
		if (!CanUseToolToBuildPart(partName, tool))
			failureReasonsMask |= EConstructionInfoCategories.TOOL_PRESENCE;
		
		return failureReasonsMask == EConstructionInfoCategories.NONE;
	}
	
	//! Simplified condition for utility (non-)parts
	bool CanBuildUtilityPart(string partName)
	{
		if (IsPartConstructed(partName))
			return false;
		if (!HasRequiredPart(partName))
			return false;
		if (!PartHasAnySupportFulfilled(partName))
			return false;
		if (HasConflictPart(partName))
			return false;
		
		return true;
	}
	
	bool MaterialIsRuined(string part_name)
	{
		ConstructionPart part = GetConstructionPart(part_name);
		array<ref ConstructionMaterialData> materials = part.GetMaterialsData();
		foreach (ConstructionMaterialData dta : materials)
		{
			ItemBase attachment = ItemBase.Cast(m_EntityParent.FindAttachmentBySlotName(dta.m_MaterialSlotName));
			if (attachment && attachment.IsRuined())
				return true;
		}
		
		return false;
	}
	
	//Get all construction parts that can currently be built
	void GetConstructionPartsToBuild(string main_part_name, out array<ConstructionPart> construction_parts, ItemBase tool, out string real_constructionTarget, bool use_tool)
	{
		construction_parts.Clear();
		
		foreach (ConstructionPart part : m_ConstructionPartArray)
		{
			if ((main_part_name == part.GetMainPartName() || main_part_name == part.GetPartName()) && CanBuildPart(part.GetPartName(), tool, use_tool))
			{
				construction_parts.Insert(part);
			}
		}
	}
	
	//TODO: double parsing every update due to info actions. Cache results once every update only?
	//Get all construction parts from the targeted actionComponentName, that currently CANNOT be built
	void GetConstructionPartsUnableToBuild(string actionComponentName, out array<ConstructionPart> constructionParts, ItemBase tool, out string realConstructionTarget, bool useTool)
	{
		constructionParts.Clear();
		
		foreach (ConstructionPart part : m_ConstructionPartArray)
		{
			if (actionComponentName == part.GetMainPartName() && !CanBuildPart(part.GetPartName(), tool, useTool))
			{
				constructionParts.Insert(part);
			}
		}
	}
	
	//Returns (first found) base construction part
	ConstructionPart GetBaseConstructionPart()
	{
		foreach (ConstructionPart part : m_ConstructionPartArray)
		{
			if (part.IsBase())
				return part;
		}
		
		return NULL;
	}
	
	//!checks if construction part has all the required parts built
	bool HasRequiredPart(string part_name)
	{
		ConstructionPart part = m_ConstructionParts.Get(part_name);
		
		//check if simple required parts are already built
		array<string> requiredParts = part.GetRequiredParts();
		foreach (string partName : requiredParts)
		{
			if (!IsPartConstructed(partName))
				return false;
		}
		
		return true;
	}
	
	//!checks if construction part has all the required support
	bool PartHasAnySupportFulfilled(string part_name)
	{
		return true;
	}
	
	//checks if there are conflict parts already built. Simple variant, still in use
	bool HasConflictPart(string part_name)
	{
		string conflictingPartName = "";
		return HasConflictPart(part_name, conflictingPartName);
	}
	
	//checks if there are conflict parts already built. Outputs the conflicting part name.
	bool HasConflictPart(string partName, out string conflictingPartName)
	{
		conflictingPartName = "";
		ConstructionPart part = GetConstructionPart(partName);
		array<string> conflictedParts = part.GetConflictedParts();
		foreach (string partNameTest : conflictedParts)
		{
			if (IsPartConstructed(partNameTest))
			{
				conflictingPartName = partNameTest;
				return true;
			}
		}
		
		return false;
	}
	
	//DECONSTRUCTION
	ConstructionPart GetConstructionPartToDismantle(string part_name, ItemBase tool)
	{
		if (CanDismantlePart(part_name, tool))
		{
			return GetConstructionPart(part_name);
		}
		
		return NULL;
	}

	bool CanDismantlePart(string part_name, ItemBase tool)
	{
		if (IsPartConstructed(part_name) && !HasDependentPart(part_name) && SupportRemovalCheck(part_name) && CanUseToolToDismantlePart(part_name, tool))
		{
			#ifdef DIAG_DEVELOPER
			if (DiagMenu.GetBool(DiagMenuIDs.BASEBUILDING_FULLDEBUG))
			{
				ConstructionDebug.ResetDismantlePartData();
			}
			#endif
			return true;
		}
		
		#ifdef DIAG_DEVELOPER
		if (GetConstructionPart(part_name))
		{
			if (DiagMenu.GetBool(DiagMenuIDs.BASEBUILDING_FULLDEBUG))
			{
				ConstructionDismantleDebugData data = new ConstructionDismantleDebugData();
				data.partName = part_name;
				data.constructed = IsPartConstructed(part_name);
				data.hasDependents = HasDependentPart(part_name);
				data.supportState = SupportRemovalCheck(part_name);
				
				ConstructionDebug.SetCanDismantlePartData(data);
			}
		}
		#endif
		
		return false;
	}
	
	//!checks if construction part has dependent part (that is already built) because of which it cannot be dismantled
	bool HasDependentPart(string part_name)
	{
		foreach (ConstructionPart part : m_BuiltConstructionPartArray)
		{
			if (!part.IsUtilityPart() && part.GetRequiredParts().Find(part_name) > -1)
			{
				return true;
			}
		}
		
		return false;
	}
	
	//! Checks if all supports would hold on part removal. Returns 'false' if some would fail
	bool SupportRemovalCheck(string part_name)
	{
		return true;
	}
	
	bool CanRepairPart(string partName, ItemBase tool = null, PlayerBase player = null, bool facingCheck = false)
	{
		ConstructionPart constructionPart = GetConstructionPart(partName);
		if (constructionPart)
		{
			//damage check
			string zoneName;
			if (!DamageSystem.GetDamageZoneFromComponentName(m_EntityParent, partName, zoneName)) //no zone, no repair
				return false;
			
			float zoneHealth = m_EntityParent.GetHealthLevel(zoneName);
			if (zoneHealth == GameConstants.STATE_PRISTINE || zoneHealth == GameConstants.STATE_RUINED)
				return false;
			
			//materials check
			if (!HasMaterials(partName,true))
				return false;
			
			if (!player) //for debug purposes, I guess?
				return true;
			
			//facing and position checks
			if (!ConstructionDirectionCheck(player, constructionPart, EConstructionDirCheckType.INDOORS) || !ConstructionDirectionCheck(player, constructionPart, EConstructionDirCheckType.REFERENCE_DIRECTION))
				return false;
			
			if (!m_EntityParent.IsFacingPlayerEx(player, partName) && !player.GetInputController().CameraIsFreeLook())
			{
				//Camera check (client-only)
				if (!g_Game.IsDedicatedServer())
				{
					if (facingCheck)
					{
						if (m_EntityParent.IsFacingCamera(partName))
							return false;
					}
				}
				
				ConstructionActionData cad = player.GetConstructionActionData();
				cad.SetTargetPart(constructionPart);
				return true;
			}
		}
		
		return false;
	}
	
	//! Check character positioning with a box check (character must be inside at least one of the defined boxes)
	bool ConstructionInsiderBoxCheck(PlayerBase player, ConstructionPart part)
	{
		const float CYLINDER_HEIGHT = 0.01;
		const float CYLINDER_RADIUS = 0.01;
		
		#ifdef DIAG_DEVELOPER
		bool fullDebugEnabled = DiagMenu.GetBool(DiagMenuIDs.BASEBUILDING_COLLISIONDEBUG);
		if (fullDebugEnabled || (!fullDebugEnabled && m_InsideTestBoxes.Count() > 0))
			DestroyDebugInsideTestBoxes();
		#endif
		
		TConstructionPartInsiderBoxData modelMinMaxs = part.GetInsiderBoxData();
		if (!modelMinMaxs)
			return true;
		
		vector matPlayer[4];
		Math3D.MatrixIdentity4(matPlayer);
		vector adjustedPos = player.GetPosition();
		adjustedPos[1] = adjustedPos[1] + CYLINDER_HEIGHT / 2;
		matPlayer[3] = adjustedPos;
		
		#ifdef DIAG_DEVELOPER
		if (fullDebugEnabled)
		{
			if (m_PlayerInsideTestCylinder)
				m_PlayerInsideTestCylinder.Destroy();
			m_PlayerInsideTestCylinder = Debug.DrawCylinder("0 0 0", CYLINDER_RADIUS * 5, CYLINDER_HEIGHT, COLOR_YELLOW_A);
			m_PlayerInsideTestCylinder.SetMatrix(matPlayer);
		}
		#endif
		
		vector matParent[4];
		m_EntityParent.GetTransform(matParent);
		bool success = false;
		
		foreach (array<vector> minMaxPair : modelMinMaxs)
		{
			vector min = minMaxPair[0];
			vector max = minMaxPair[1];
			
			bool playerInside = false;
			
			//temporary test solution
			playerInside = Math3D.IntersectCylinderOBB(min, max, matParent, matPlayer, CYLINDER_RADIUS, CYLINDER_HEIGHT);
			
			if (playerInside)
			{
				success = true;
				#ifdef DIAG_DEVELOPER
				if (fullDebugEnabled)
				{
					DrawDebugInsideTestBox(min, max, ARGB(90, 128, 128, 128));
				}
				else
				#else
					return true;
				#endif
			}
			#ifdef DIAG_DEVELOPER
			else
			{
				if (fullDebugEnabled)
				{
					DrawDebugInsideTestBox(min, max, ARGB(90, 128, 0, 128));
				}
			}
			#endif
		}
		
		#ifdef DIAG_DEVELOPER
		if (fullDebugEnabled)
		{
			if (!success)
				ConstructionDebug.SetInsideboxCheckDebugData(part.GetPartName());
			else
				ConstructionDebug.SetInsideboxCheckDebugData("");
		}
		#endif
		
		return success;
	}
	
	//TODO: make this reliant on configuration, don't inherently check the points!
	bool ConstructionDirectionCheck(PlayerBase player, ConstructionPart part, EConstructionDirCheckType type)
	{
		string name = part.GetPartName();
		//first we check actual part names in case of individual handling
		string memStart = "" + name + "_pos";
		string memEnd;
		
		if (m_EntityParent.MemoryPointExists(memStart))
		{
			switch (type)
			{
				case EConstructionDirCheckType.INDOORS:
					memEnd = "" + name + "_inside";
					break;
				
				case EConstructionDirCheckType.REFERENCE_DIRECTION:
					memEnd = "" + name + "_ref";
					break;
				
				default:
					memEnd = "";
					break;
			}
		}
		
		//next we check main part name for mass handling
		name = part.GetMainPartName();
		memStart = "" + name + "_pos";
		
		if (m_EntityParent.MemoryPointExists(memStart))
		{
			switch (type)
			{
				case EConstructionDirCheckType.INDOORS:
					memEnd = "" + name + "_inside";
					break;
				
				case EConstructionDirCheckType.REFERENCE_DIRECTION:
					memEnd = "" + name + "_ref";
					break;
				
				default:
					memEnd = "";
					break;
			}
		}
		
		if (memEnd != "" && m_EntityParent.MemoryPointExists(memEnd))
		{
			return MiscGameplayFunctions.MemPointDirectionalCheck(player, m_EntityParent, memStart, memEnd);
		}
		
		return true;
	}
	
	//returns array of BUILT parts that directly depend on 'part_name' in some fashion
	protected array<string> GetValidDepenentPartsArray(string part_name, array<string> recurs = null)
	{
		string name = "";
		array<string> dependentParts;
		
		foreach (ConstructionPart part : m_BuiltConstructionPartArray)
		{
			name = part.GetPartName();
			
			array<string> reqPartsArray = part.GetRequiredParts();
			if (part.IsBuilt() && reqPartsArray && reqPartsArray.Find(part_name) > -1)
			{
				if (!dependentParts)
					dependentParts = new array<string>();
				
				if (!recurs || (recurs.Find(name) == -1))
					dependentParts.Insert(name);
			}
		}
		
		return dependentParts;
	}
	
	//gets simple required parts of a construction part; caches into ConstructionPartTypeData array on init
	array<string> GetRequiredParts(string part_name, string main_part_name)
	{
		string cfgPath = "cfgVehicles" + " " + m_EntityParent.GetType() + " " + "Construction" + " " + main_part_name + " " + part_name + " " + "required_parts";
		array<string> requiredParts = new array<string>();
		g_Game.ConfigGetTextArray(cfgPath, requiredParts);
		
		return requiredParts;
	}
	
	//DESTROY
	ConstructionPart GetConstructionPartToDestroy(string part_name)
	{
		if (CanDestroyPart(part_name))
			return GetConstructionPart(part_name);
		
		return NULL;
	}	

	bool CanDestroyPart(string part_name)
	{
		if (IsPartConstructed(part_name) && !HasDependentPart(part_name) && SupportRemovalCheck(part_name))
			return true;
		
		return false;
	}	
	
	//CONSTRUCTION PART STATE
	//! Animates AnimationSource. Use on server only, AUTO-SYNCED
	protected void ShowConstructionPart(string part_name)
	{
		if (LogManager.IsBaseBuildingLogEnable()) bsbDebugPrint("[bsb] Construction ShowConstructionPart " + m_EntityParent.GetType() + " | part = " + part_name);
		
		ConstructionPart part = m_ConstructionParts.Get(part_name);
		part.HandleShowConstructionPart(m_EntityParent);
	}
	
	//! Animates AnimationSource. Use on server only, AUTO-SYNCED
	protected void HideConstructionPart(string part_name)
	{
		if (LogManager.IsBaseBuildingLogEnable()) bsbDebugPrint("[bsb] Construction HideConstructionPart " + m_EntityParent.GetType() + " | part = " + part_name);
		
		ConstructionPart part = m_ConstructionParts.Get(part_name);
		part.HandleHideConstructionPart(m_EntityParent);
	}
	
	//! sets physics LOCALLY
	void ShowConstructionPartPhysics(string part_name)
	{
		if (LogManager.IsBaseBuildingLogEnable()) bsbDebugPrint("[bsb] ShowConstructionPartPhysics " + m_EntityParent.GetType() + " | part = " + part_name);
		
		ConstructionPart part = m_ConstructionParts.Get(part_name);
		part.HandleAddPhysics(m_EntityParent);
	}
	
	//! sets physics LOCALLY
	void HideConstructionPartPhysics(string part_name)
	{
		if (LogManager.IsBaseBuildingLogEnable()) bsbDebugPrint("[bsb] HideConstructionPartPhysics " + m_EntityParent.GetType() + " | part = " + part_name);
		
		ConstructionPart part = m_ConstructionParts.Get(part_name);
		part.HandleRemovePhysics(m_EntityParent);
	}	
	
	//! returns true if part constructed
	bool IsPartConstructed(string part_name)
	{
		ConstructionPart part = GetConstructionPart(part_name);
		if (part)
			return part.IsBuilt();
		
		return false;
	}
	
	//============================================
	// Materials for construction
	//============================================
	//! returns true if materials to build the part are available
	bool HasMaterialsEx(string part_name, bool repairing = false, out string slotName = "", out float requiredQuantity = 0.0)
	{
		#ifdef DIAG_DEVELOPER
		if (g_Game.IsClient() || !g_Game.IsMultiplayer()) //weird, I know, but this is just used for initial filtering purposes. Action conditions take care of the rest for both sides
		{
			PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
			if (player)
			{
				if (player.GetConstructionActionData().GetFastConstructionMode())
					return true;
			}
		}
		#endif
		
		ConstructionPart part = GetConstructionPart(part_name);
		array<ref ConstructionMaterialData> materials = part.GetMaterialsData();
		foreach (ConstructionMaterialData dta : materials)
		{
			float quantity = dta.m_MaterialQuantity;
			if (quantity == 0.0) //0 - ignores quantity
				continue;
			
			if (repairing)
			{
				quantity *= REPAIR_MATERIAL_PERCENTAGE;
				quantity = Math.Max(Math.Floor(quantity),1);
			}
			
			//if the selected material (or its quantity) is not available
			if (!HasMaterialWithQuantityAttached(dta.m_MaterialSlotName, quantity))
			{
				slotName = dta.m_MaterialSlotName;
				requiredQuantity = quantity;
				return false;
			}
		}
		
		return true;	//return true even if no material required
	}
	
	//simplified variant of the above (legacy)
	bool HasMaterials(string part_name, bool repairing = false)
	{
		string dumpSlotName;
		float dumpQty;
		return HasMaterialsEx(part_name, repairing, dumpSlotName, dumpQty);
	}
	//check if parent object has attachment of required quantity attached to it
	protected bool HasMaterialWithQuantityAttached(string slot_name, float quantity)
	{
		ItemBase attachment = ItemBase.Cast(m_EntityParent.FindAttachmentBySlotName(slot_name));
		if (attachment && ((int)quantity == -1 || attachment.GetQuantity() >= quantity)) //-1 - deletes the object
			return true;
		
		return false;
	}
	
	//take materials when building
	void TakeMaterialsServer(string part_name, bool repairing = false)
	{
		ConstructionPart part = GetConstructionPart(part_name);
		array<ref ConstructionMaterialData> materials = part.GetMaterialsData();
		foreach (ConstructionMaterialData dta : materials)
		{
			ItemBase attachment = ItemBase.Cast(m_EntityParent.FindAttachmentBySlotName(dta.m_MaterialSlotName));
			if (!attachment)
				continue;
			
			if (repairing && dta.m_SkipOnRepair)
				continue;
			
			if (dta.m_MaterialLockable)
			{
				//lock attachment
				InventoryLocation invloc = new InventoryLocation();
				attachment.GetInventory().GetCurrentInventoryLocation(invloc);
		
				m_EntityParent.GetInventory().SetSlotLock(invloc.GetSlot(), true);
			}
			else
			{
				if (dta.m_MaterialQuantity > 0.0)			//0 - ignores quantity
				{
					float quantity = dta.m_MaterialQuantity;
					if (repairing)
					{
						quantity *= REPAIR_MATERIAL_PERCENTAGE;
						quantity = Math.Max(Math.Floor(quantity),1);
					}
					//subtract quantity
					attachment.AddQuantity(-quantity);
				}
				else if (dta.m_MaterialQuantity == -1.0)	//-1 - deletes the object
				{
					g_Game.ObjectDelete(attachment);
				}
			}
		}
	}
	
	//receive materials when dismantling
	protected void ReceiveMaterialsServer(notnull Man player, string part_name, string damagezone_name)
	{
		ConstructionPart part = GetConstructionPart(part_name);
		bool isBase = part.IsBase();
		string mainPartName = part.GetMainPartName();
		string cfgPath = "cfgVehicles" + " " + m_EntityParent.GetType() + " "+ "Construction" + " " + mainPartName + " " + part_name + " " + "Materials";
		
		if (g_Game.ConfigIsExisting(cfgPath))
		{
			StaticConstructionMethods.SpawnConstructionMaterialPiles(m_EntityParent,player,cfgPath,part_name,damagezone_name,isBase);
		}
	}
	
	//destroy lockable materials when destroying
	protected void DestroyMaterialsServer(Man player, string part_name)
	{
		ConstructionPart part = GetConstructionPart(part_name);
		array<ref ConstructionMaterialData> materials = part.GetMaterialsData();
		foreach (ConstructionMaterialData dta : materials)
		{
			//get material
			ItemBase attachment = ItemBase.Cast(m_EntityParent.FindAttachmentBySlotName(dta.m_MaterialSlotName));
			if (!attachment)
				continue;
			
			//material still attached
			if (dta.m_MaterialLockable)
			{
				InventoryLocation invloc = new InventoryLocation();
				attachment.GetInventory().GetCurrentInventoryLocation(invloc);
				if (LogManager.IsBaseBuildingLogEnable()) bsbDebugPrint("[bsb] " + Object.GetDebugName(m_EntityParent) + " DestroyMaterialsServer unlock slot=" + invloc.GetSlot());
				
				m_EntityParent.GetInventory().SetSlotLock(invloc.GetSlot() , false);
				g_Game.ObjectDelete(attachment);
			}
		}
	}
	
	//TODO: remove config parsing and read from cached data
	void DropNonUsableMaterialsServer(Man player, string part_name)
	{
		ConstructionPart part = GetConstructionPart(part_name);
		
		string cfgPath = "cfgVehicles" + " " + m_EntityParent.GetType() + " " + "Construction" + " " + part.GetMainPartName() + " " + part.GetPartName() + " " + "platform_support";
		string platformSupport = "";
		
		if (g_Game.ConfigIsExisting(cfgPath))
		{		
			g_Game.ConfigGetText(cfgPath, platformSupport);
		}
		
		if (platformSupport != "" || part.IsBase())
		{
			string atCfgPath = "cfgVehicles" + " " + m_EntityParent.GetType() + " "+ "GUIInventoryAttachmentsProps";
			
			if (g_Game.ConfigIsExisting(atCfgPath))
			{
				int	count = g_Game.ConfigGetChildrenCount(atCfgPath);
				for (int i = 0; i < count; ++i)
				{
					string childName;
					g_Game.ConfigGetChildName(atCfgPath, i, childName);
					childName.ToLower();
					
					//TODO: rip and tear
					if (childName.Contains(platformSupport)) //necessary; 'Level_3' will also drop stuff from 'Level_3_Wall_1', 'Level_3_Wall_2' and so on. FML
					{
						array<string> attachment_slots = new array<string>();
						g_Game.ConfigGetTextArray(atCfgPath + " " + childName + " " + "attachmentSlots", attachment_slots);
						
						for (int j = 0; j < attachment_slots.Count(); ++j)
						{
							//get material
							ItemBase attachment = ItemBase.Cast(m_EntityParent.FindAttachmentBySlotName(attachment_slots[j]));
							
							//material still attached
							if (attachment)
							{
								InventoryLocation invloc = new InventoryLocation();
								attachment.GetInventory().GetCurrentInventoryLocation(invloc);
								if (LogManager.IsBaseBuildingLogEnable()) bsbDebugPrint("[bsb] " + Object.GetDebugName(m_EntityParent) + " DropNonUsableMaterials UNlocking slot=" + invloc.GetSlot());
								
								//unlock slot
								m_EntityParent.GetInventory().SetSlotLock(invloc.GetSlot() , false);
								
								EntityAI parent = m_EntityParent;
								if (!parent)
									parent = player;
								
								int quantityMax = attachment.GetTargetQuantityMax(-1);
								InventoryLocation dst = new InventoryLocation();
								vector mat[4];
								attachment.GetTransform(mat);
								//TODO: why are we spawning and deleting here, instead of moving to location??
								if (parent.MemoryPointExists("" + part_name + "_materials"))
								{
									vector destination = parent.GetMemoryPointPos("" + part_name + "_materials");
									destination = g_Game.ObjectModelToWorld(parent,destination);
									float health = attachment.GetHealth("","Health");
									float quantity = attachment.GetQuantity() - 1;
									if (quantity < 1.0)
										quantity = 1.0;
									float dir[4];
									invloc.GetDir(dir);
									dst.SetGroundEx(attachment,destination,dir);
									
									if (player)
									{
										vector posHead = MiscGameplayFunctions.GetPlayerHeadPosition(PlayerBase.Cast(player));
										MiscGameplayFunctions.CreateItemBasePilesDispersed(attachment.GetType(),posHead,destination,UAItemsSpreadRadius.NARROW,quantity,health,player);
									}
									else
									{
										MiscGameplayFunctions.CreateItemBasePiles(attachment.GetType(),destination,quantity,health,true);
									}
									attachment.AddQuantity(-quantity);
								}
								else
								{
									dst.SetGround(attachment,mat);
									
									for (int k = attachment.GetQuantity(); k > quantityMax; )
									{
										Object o = parent.GetInventory().LocationCreateEntity(dst, attachment.GetType(), ECE_PLACE_ON_SURFACE, RF_DEFAULT);
										ItemBase new_item = ItemBase.Cast(o);
										
										if (new_item)
										{
											MiscGameplayFunctions.TransferItemProperties(attachment, new_item);
											attachment.AddQuantity(-quantityMax);
											new_item.SetQuantity(quantityMax);
										}
										k -= quantityMax;
									}
								}
								
								//drop
								if (attachment.GetQuantity() > 0)
								{
									if (g_Game.IsMultiplayer())
									{
										parent.ServerTakeToDst(invloc, dst);
									}
									else
									{
										parent.LocalTakeToDst(invloc, dst);
									}
								}
								else
								{
									attachment.Delete();
								}
							}
						}
					}
				}
			}
		}
	}
	
	//! Drop other, non-material attachments tied to the construction part
	protected void DropEssentialAttachments(Man player, string partName)
	{
		ConstructionPart part = GetConstructionPart(partName);
		if (part.HasEssentialAttachmentSlots())
		{
			array<string> slots = part.GetEssentialAttachmentSlots();
			ItemBase attachment;
			GameInventory parentInventory = m_EntityParent.GetInventory();
			vector groundPos;
			foreach (string slotName: slots)
			{
				if (Class.CastTo(attachment, m_EntityParent.FindAttachmentBySlotName(slotName)))
				{
					//first get rid of any possible locks preventing the drop
					parentInventory.SetSlotLock(InventorySlots.GetSlotIdFromString(slotName), false);
					
					//now drop
					int mode = InventoryMode.LOCAL;
					if (g_Game.IsMultiplayer())
						mode = InventoryMode.SERVER;
					
					if (m_EntityParent.MemoryPointExists(slotName))
					{
						groundPos = m_EntityParent.ModelToWorld(m_EntityParent.GetMemoryPointPos(slotName));
					}
					else if (player)
					{
						groundPos = player.GetPosition();
					}
					else
					{
						groundPos = attachment.GetPosition();
					}
					
					vector m4[4];
					Math3D.MatrixIdentity4(m4);
					m4[3] = groundPos;
					
					parentInventory.DropEntityWithTransform(mode, m_EntityParent, attachment, m4);
				}
			}
		}
	}
	
	//set lock on materials that are attached and cannot be locked/unlocked
	void SetLockOnAttachedMaterials(string part_name, bool lock_slot)
	{
		ConstructionPart part = GetConstructionPart(part_name);
		array<ref ConstructionMaterialData> materials = part.GetMaterialsData();
		foreach (ConstructionMaterialData dta : materials)
		{
			ItemBase attachment = ItemBase.Cast(m_EntityParent.FindAttachmentBySlotName(dta.m_MaterialSlotName));
			if (attachment && dta.m_MaterialLockable)
			{
				InventoryLocation inventoryLocation = new InventoryLocation();
				attachment.GetInventory().GetCurrentInventoryLocation(inventoryLocation);
				m_EntityParent.GetInventory().SetSlotLock(inventoryLocation.GetSlot(), lock_slot);
				
				if (LogManager.IsBaseBuildingLogEnable()) bsbDebugPrint("[bsb] " + Object.GetDebugName(m_EntityParent) + " SetLockOnAttachedMaterials lock=" + lock_slot +" slot=" + inventoryLocation.GetSlot());
			}
		}
	}
	
	//============================================
	// Construction tools
	//============================================	
	bool CanUseToolToBuildPart(string part_name, ItemBase tool)
	{
		#ifdef DIAG_DEVELOPER
		if (g_Game.IsClient() || !g_Game.IsMultiplayer())
		{
			PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
			if (player)
			{
				if (player.GetConstructionActionData().GetFastConstructionMode())
					return true;
			}
		}
		#endif
		
		ConstructionPart part = GetConstructionPart(part_name);
		int actionMask = part.GetBuildActionTypeMask();
		if (actionMask != EConstructionTools.TOOL_NO_ACTION)
		{
			if (tool)
			{
				//TODO: store on ItemBaseType someplace?
				string toolCfgPath = "cfgVehicles" + " " + tool.GetType() + " " + "build_action_type";
				if (g_Game.ConfigIsExisting(toolCfgPath))
				{
					int toolActionType = g_Game.ConfigGetInt(toolCfgPath);
					if ((actionMask & toolActionType) > 0)
					{
						return true;
					}
				}
			}
			else if (actionMask & EConstructionTools.TOOL_HANDS)
			{
				return true;
			}
		}
		
		return false;
	}
	
	bool CanUseToolToDismantlePart(string part_name, ItemBase tool)
	{
		ConstructionPart part = GetConstructionPart(part_name);
		int actionMask = part.GetDismantleActionTypeMask();
		if (actionMask != EConstructionTools.TOOL_NO_ACTION)
		{
			if (tool)
			{
				//TODO: store on ItemBaseType someplace?
				string toolCfgPath = "cfgVehicles" + " " + tool.GetType() + " " + "dismantle_action_type";
				
				if (g_Game.ConfigIsExisting(toolCfgPath))
				{
					int toolActionType = g_Game.ConfigGetInt(toolCfgPath);
					if ((actionMask & toolActionType) > 0)
					{
						return true;
					}
				}
			}
			else if (actionMask & EConstructionTools.TOOL_HANDS)
			{
				return true;
			}
		}
		
		return false;
	}
	
	ConstructionMaterialType GetMaterialType(string part_name)
	{
		ConstructionPart part = GetConstructionPart(part_name);
		return part.GetMaterialType();
	}
	
	//============================================
	// Collision check
	//============================================
	//! Collision check for building part
	bool IsCollidingEx(CollisionCheckData check_data)
	{
		if (CfgGameplayHandler.GetDisableIsCollidingCheck())
			return false;
		
		ConstructionPart part = GetConstructionPart(check_data.m_PartName);
		if (part && part.GetCollisionData().Count() == 2)
		{
			vector center;
			float absoluteOfset = 0.05; 	//we need to lift BBox even more, because it colliddes with house floors due to various reasons (probably geometry or float imperfections)
			vector edgeLength;
			vector minMax[2];				//data used for creating trigger
			array<Object> excludedObjects = new array<Object>();
			array<Object> collidedObjects = new array<Object>();
			
			excludedObjects.Insert(m_EntityParent);
			if (check_data.m_AdditionalExcludes.Count() > 0)
			{
				excludedObjects.InsertAll(check_data.m_AdditionalExcludes);
			}
			
			GetCollisionBoxData(check_data.m_PartName, minMax);
			center = GetBoxCenter(minMax);
			center = m_EntityParent.ModelToWorld(center);		//convert to world coordinates
			edgeLength = GetCollisionBoxSize(minMax);
			
			#ifdef DIAG_DEVELOPER 
			bool collisionDebugEnabled = DiagMenu.GetBool(DiagMenuIDs.BASEBUILDING_COLLISIONDEBUG);
			#endif
			if (g_Game.IsBoxCollidingGeometry(Vector(center[0], center[1] + absoluteOfset, center[2]), m_EntityParent.GetOrientation(), edgeLength, check_data.m_PrimaryGeometry, check_data.m_SecondaryGeometry, excludedObjects, collidedObjects))
			{
				bool validCollision = false;
				#ifdef DIAG_DEVELOPER 
				array<string> validEntityCollisions = new array<string>();
				#endif
				
				for (int i = 0; i < collidedObjects.Count(); ++i)
				{
					EntityAI entity = EntityAI.Cast(collidedObjects.Get(i));
					if (entity && !entity.IsIgnoredByConstruction())
					{
						validCollision = true;
						#ifdef DIAG_DEVELOPER 
						if (collisionDebugEnabled)
						{
							validEntityCollisions.Insert(entity.GetType());
						}
						#else
						break;
						#endif
					}
				}
				
				#ifdef DIAG_DEVELOPER 
				if (collisionDebugEnabled)
				{
					if (validCollision)
					{
						ConstructionDebug.SetCollisionPrintoutData(check_data.m_PartName, validEntityCollisions);
						DrawDebugCollisionBox(minMax, ARGB(150, 0, 0, 255));
					}
					else
					{
						ConstructionDebug.ResetCollisionPrintoutData();
						DrawDebugCollisionBox(minMax, ARGB(150, 255, 255, 255));
					}
				}
				else if (m_CollisionBox)
				{
					DestroyDebugCollisionBox();
				}
				#endif
				
				return validCollision;
			}
			#ifdef DIAG_DEVELOPER
			else //Debug
			{
				if (collisionDebugEnabled)
				{
					ConstructionDebug.ResetCollisionPrintoutData();
					DrawDebugCollisionBox(minMax, ARGB(150, 255, 255, 255));
				}
				else if (m_CollisionBox)
				{
					DestroyDebugCollisionBox();
				}
			}
			#endif
		}
		return false;
	}

	vector GetCollisionBoxSize(vector min_max[2])
	{
		vector boxSize = Vector(0, 0, 0);
		
		boxSize[0] = Math.AbsFloat(min_max[1][0] - min_max[0][0]);
		boxSize[1] = Math.AbsFloat(min_max[1][1] - min_max[0][1]);
		boxSize[2] = Math.AbsFloat(min_max[1][2] - min_max[0][2]);
		
		return boxSize;
	}
	
	//outputs collision box minMax from construction config and model p3d, in MODELSPACE coordinates
	protected void GetCollisionBoxData(string part_name, out vector min_max[2])
	{
		ConstructionPart path = GetConstructionPart(part_name);
		array<string> dta = path.GetCollisionData();
		if (dta.Count() == 2)
		{
			if (m_EntityParent.MemoryPointExists(dta[0]))
			{
				min_max[0] = m_EntityParent.GetMemoryPointPos(dta[0]);
			}
			if (m_EntityParent.MemoryPointExists(dta[1]))
			{
				min_max[1] = m_EntityParent.GetMemoryPointPos(dta[1]);
			}
		}
		else if (dta.Count() > 0)
		{
			ErrorEx("Wrong collision_data configuration of " + part_name + "; valid collision_data must contain exactly 2 strings (min/max)!",ErrorExSeverity.INFO);
		}
	}
	
	//outputs item fall detection box minMax from construction config and model p3d, in MODELSPACE coordinates
	protected void GetItemFallDetectionBoxData(string partName, out vector minMax[2])
	{
		ConstructionPart path = GetConstructionPart(partName);
		array<string> dta = path.GetItemFallingData();
		if (dta.Count() == 2)
		{
			if (m_EntityParent.MemoryPointExists(dta[0]))
			{
				minMax[0] = m_EntityParent.GetMemoryPointPos(dta[0]);
			}
			if (m_EntityParent.MemoryPointExists(dta[1]))
			{
				minMax[1] = m_EntityParent.GetMemoryPointPos(dta[1]);
			}
		}
		else if (dta.Count() > 0)
		{
			ErrorEx("Wrong collision_data configuration of " + partName + "; valid item_falling_data must contain exactly 2 strings (min/max)!",ErrorExSeverity.INFO);
		}
	}
	
	//returns center point of box defined by min/max values
	vector GetBoxCenter(vector min_max[2])
	{
		vector center;
		
		center[0] = (min_max[1][0] - min_max[0][0]) / 2;
		center[1] = (min_max[1][1] - min_max[0][1]) / 2;
		center[2] = (min_max[1][2] - min_max[0][2]) / 2;
		center = Vector(min_max[1][0] - center[0], min_max[1][1] - center[1], min_max[1][2] - center[2]); //offset to box center
		
		return center;
	}
	
	void GetTriggerExtents(vector min_max[2], out vector extents[2])
	{
		vector egdeLength = GetCollisionBoxSize(min_max);
		extents[0][0] = -egdeLength[0] / 2;		//min
		extents[0][1] = -egdeLength[1] / 2;
		extents[0][2] = -egdeLength[2] / 2;
		extents[1][0] = egdeLength[0] / 2;			//max
		extents[1][1] = egdeLength[1] / 2;
		extents[1][2] = egdeLength[2] / 2;
	}
	
	#ifdef DIAG_DEVELOPER 
	//Debug
	protected void DrawDebugCollisionBox(vector min_max[2], int color)
	{
		DestroyDebugCollisionBox();
		
		vector mat[4];
		m_EntityParent.GetTransform(mat);
		
		m_CollisionBox = Debug.DrawBox(min_max[0], min_max[1], color);
		m_CollisionBox.SetMatrix(mat);
	}
	
	protected void DestroyDebugCollisionBox()
	{
		if (m_CollisionBox)
		{
			m_CollisionBox.Destroy();
			m_CollisionBox = NULL;
		}		
	}
	
	protected void DrawDebugInsideTestBox(vector min, vector max, int color)
	{
		vector mat[4];
		m_EntityParent.GetTransform(mat);
		
		Shape box = Debug.DrawBox(min, max, color);
		box.SetMatrix(mat);
		m_InsideTestBoxes.Insert(box);
	}
	
	protected void DestroyDebugInsideTestBoxes()
	{
		foreach (Shape box : m_InsideTestBoxes)
		{
			box.Destroy();
			box = NULL;
		}
		m_InsideTestBoxes.Clear();
	}
	#endif
	//////////////////
	//Deprecated Code
	//////////////////
	[Obsolete("call BuildPartServerEx instead")]
	void BuildPartServer(notnull Man player, string part_name, int action_id)
	{
		if (LogManager.IsBaseBuildingLogEnable()) bsbDebugPrint("[bsb] Construction BuildPartServer | " + part_name);
		
		BuildPartServerEx(player,part_name,action_id);
	}
	
	[Obsolete("call DismantlePartServerEx instead")]
	void DismantlePartServer(notnull Man player, string part_name, int action_id)
	{
		if (LogManager.IsBaseBuildingLogEnable()) bsbDebugPrint("[bsb] Construction DismantlePartServer | " + part_name);
		
		DismantlePartServerEx(player,part_name,action_id);
	}

	[Obsolete("call IsCollidingEx instead")]
	bool IsColliding(string part_name)
	{
		if (CfgGameplayHandler.GetDisableIsCollidingCheck())
			return false;
		
		ConstructionPart part = GetConstructionPart(part_name);
		if (part && part.GetCollisionData().Count() == 2)
		{
			vector center;
			float absolute_ofset = 0.05; 	//we need to lift BBox even more, because it colliddes with house floors due to various reasons (probably geometry or float imperfections)
			vector edge_length;
			vector min_max[2];				//data used for creating trigger
			array<Object> excludedObjects = new array<Object>();
			array<Object> collidedObjects = new array<Object>();
			
			excludedObjects.Insert(m_EntityParent);
			
			//get min_max and center from config and memory points
			GetCollisionBoxData(part_name, min_max);
			
			center = GetBoxCenter(min_max);
			center = m_EntityParent.ModelToWorld(center);		//convert to world coordinates
			edge_length = GetCollisionBoxSize(min_max);
			
			if (g_Game.IsBoxCollidingGeometry(Vector(center[0], center[1] + absolute_ofset, center[2]), m_EntityParent.GetOrientation(), edge_length, ObjIntersectView, ObjIntersectGeom, excludedObjects, collidedObjects))
			{
				for (int i = 0; i < collidedObjects.Count(); ++i)
				{
					EntityAI entity = EntityAI.Cast(collidedObjects.Get(i));
					if (entity && !entity.IsIgnoredByConstruction())
						return true;
				}
			}
		}
		return false;
	}

	[Obsolete("no replacement")]
	void CreateCollisionTrigger(string part_name, vector min_max[2], vector center)
	{
		if (m_ConstructionBoxTrigger)
		{
			if (m_ConstructionBoxTrigger.GetPartName() == part_name)		//already created
			{
				return;
			}
			else
			{
				DestroyCollisionTrigger();
			}
		}	
		
		//get proper trigger extents (min<max)
		vector extents[2];
		GetTriggerExtents(min_max, extents);
		
		//create trigger
		m_ConstructionBoxTrigger = ConstructionBoxTrigger.Cast(g_Game.CreateObject("ConstructionBoxTrigger", center, false, false, false));
		m_ConstructionBoxTrigger.SetPosition(center);
		m_ConstructionBoxTrigger.SetOrientation(m_EntityParent.GetOrientation());	
		m_ConstructionBoxTrigger.SetExtents(extents[0], extents[1]);
		
		m_ConstructionBoxTrigger.SetPartName(part_name);
	}
	
	[Obsolete("no replacement")]
	void DestroyCollisionTrigger()
	{
		if (!m_ConstructionBoxTrigger)
			return;
		
		g_Game.ObjectDelete(m_ConstructionBoxTrigger);
		m_ConstructionBoxTrigger = NULL;
	}
	
	[Obsolete("no replacement")]
	bool IsTriggerColliding()
	{
		if (!m_ConstructionBoxTrigger)
			return false;
		
		return m_ConstructionBoxTrigger.IsColliding();
	}

	protected ConstructionBoxTrigger m_ConstructionBoxTrigger; //! Deprecated
}

class StaticConstructionMethods
{
	//! spawns material from any construction; 'player' parameter optional; 'main_part_name' is actually a PART_NAME!
	static void SpawnConstructionMaterialPiles(notnull EntityAI entity, Man player, string cfg_path, string main_part_name, string damagezone_name = "", bool is_base = false)
	{
		string partName = main_part_name; //'main_part_name' is actually a PART_NAME, using this to avoid further confustion
		int	child_count = g_Game.ConfigGetChildrenCount(cfg_path);
		
		for (int i = 0; i < child_count; ++i)
		{
			string child_name;
			g_Game.ConfigGetChildName(cfg_path, i, child_name);
			
			//get type, quantity from material
			string config_path;
			string type;
			string slot_name;
			config_path = cfg_path + " " + child_name + " " + "type";
			g_Game.ConfigGetText(config_path, type);
			config_path = cfg_path + " " + child_name + " " + "slot_name";
			g_Game.ConfigGetText(config_path, slot_name);
			config_path = cfg_path + " " + child_name + " " + "quantity";
			float quantity = g_Game.ConfigGetFloat(config_path);
			config_path = cfg_path + " " + child_name + " " + "lockable";
			bool lockable = g_Game.ConfigGetInt(config_path);
			config_path = cfg_path + " " + child_name + " " + "skipOnDismantle";
			bool skipOnDismantle = g_Game.ConfigGetInt(config_path);
			
			if (skipOnDismantle)
				continue;
			
			//receive material quantity
			ItemBase attachment = ItemBase.Cast(entity.FindAttachmentBySlotName(slot_name));
			int slot_id;
			
			//material still attached
			if (lockable)			//if lockable 
			{
				if (attachment)
				{
					InventoryLocation src = new InventoryLocation();
					attachment.GetInventory().GetCurrentInventoryLocation(src);
					if (LogManager.IsBaseBuildingLogEnable()) bsbDebugPrint("[bsb] " + Object.GetDebugName(entity) + " DropNonUsableMaterials UNlocking slot=" + src.GetSlot());
					entity.GetInventory().SetSlotLock(src.GetSlot() , false);
					
					//detach if base
					if (is_base)
					{
						if (g_Game.IsMultiplayer() && player)
						{
							InventoryLocation dst = new InventoryLocation();
							GameInventory.SetGroundPosByOwner(player, src.GetItem(), dst);
							player.ServerTakeToDst(src, dst);
						}
						else
						{
							entity.GetInventory().DropEntity(InventoryMode.PREDICTIVE, entity, attachment);
						}
					}
				}
			}
			else
			{
				float pile_health;
				float qty_coef;
				vector destination = entity.GetPosition();
				vector playerPosition;
				//placed on helper memory point, if available, or on player pos, if available, or on entity pos otherwise
				if (player)
				{
					playerPosition = player.GetPosition();
					destination = playerPosition;
				}
				
				if (entity.MemoryPointExists("" + partName + "_materials"))
				{
					destination = entity.GetMemoryPointPos("" + partName + "_materials");
					destination = g_Game.ObjectModelToWorld(entity,destination);
				}
				else if (entity.MemoryPointExists(partName))
				{
					destination = entity.GetMemoryPointPos(partName);
					destination = g_Game.ObjectModelToWorld(entity,destination);
				}
				pile_health = entity.GetHealth01(damagezone_name,"Health") * MiscGameplayFunctions.GetTypeMaxGlobalHealth(type);
				qty_coef =  1 - (entity.GetHealthLevel(damagezone_name) * Construction.DECONSTURCT_MATERIAL_LOSS) - Construction.DECONSTURCT_MATERIAL_LOSS;
				quantity *= qty_coef;
				quantity = Math.Max(Math.Floor(quantity),1);
				
				if (player)
				{
					vector posHead = MiscGameplayFunctions.GetPlayerHeadPosition(PlayerBase.Cast(player));
					MiscGameplayFunctions.CreateItemBasePilesDispersed(type,posHead,destination,UAItemsSpreadRadius.NARROW,quantity,pile_health,player);
				}
				else
				{
					MiscGameplayFunctions.CreateItemBasePiles(type,destination,quantity,pile_health,true);
				}
			}
		}
	}
}

//! Data structure for passing parameters (extendable, modable)
class CollisionCheckData
{
	ref array<Object> m_AdditionalExcludes;
	string m_PartName;
	int m_PrimaryGeometry;
	int m_SecondaryGeometry;
	
	void CollisionCheckData()
	{
		m_AdditionalExcludes = new array<Object>();
		m_PartName = "";
		m_PrimaryGeometry = ObjIntersectGeom;
		m_SecondaryGeometry = ObjIntersectView;
	}
}

class ConstructionBoxTrigger : ManTrigger
{
	string m_PartName;
	
	void SetPartName(string part_name)
	{
		m_PartName = part_name;
	}
	
	string GetPartName()
	{
		return m_PartName;
	}
	
	override protected void UpdateInsiders(int timeout)
	{
		super.UpdateInsiders(20);
	}
	
	bool IsColliding()
	{
		if (GetInsiders().Count() > 0)
		{
			return true;
		}
		
		return false;
	}
}
