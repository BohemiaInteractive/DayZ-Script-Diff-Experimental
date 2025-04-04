class OvenIndoor extends FireplaceBase
{
	protected float 				m_SmokePosX;
	protected float 				m_SmokePosY;
	protected float 				m_SmokePosZ;
	protected int					m_FirePointIndex = 1;	//limited to 1 decimal place (1-9)
	
	static const string OVENPOINT_ACTION_SELECTION	= "oven_action";
	static const string OVENPOINT_FIRE_POSITION 	= "oven_point";
	static const string OVENPOINT_PLACE_ROT 		= "oven_rot";
	static const string OVENPOINT_SMOKE_POSITION 	= "oven_smoke";
	
	void OvenIndoor()
	{
		//Particles - default for FireplaceBase
		PARTICLE_FIRE_START 	= ParticleList.OVEN_FIRE_START;
		PARTICLE_SMALL_FIRE 	= ParticleList.OVEN_SMALL_FIRE;
		PARTICLE_NORMAL_FIRE	= ParticleList.OVEN_NORMAL_FIRE;
		PARTICLE_SMALL_SMOKE 	= ParticleList.HOUSE_SMALL_SMOKE;
		PARTICLE_NORMAL_SMOKE	= ParticleList.HOUSE_NORMAL_SMOKE;
		PARTICLE_FIRE_END 		= ParticleList.OVEN_FIRE_END;
		PARTICLE_STEAM_END		= ParticleList.BARREL_FIRE_STEAM_2END;
		
		//register sync variables
		RegisterNetSyncVariableFloat( "m_SmokePosX", 0, 0, 2 );
		RegisterNetSyncVariableFloat( "m_SmokePosY", 0, 0, 2 );
		RegisterNetSyncVariableFloat( "m_SmokePosZ", 0, 0, 2 );
		RegisterNetSyncVariableInt( "m_FirePointIndex", 0, 9 );
		
		m_LightDistance = 50;
		SetRoofAbove(true);
		
		m_UTSSettings.m_EnableOnTemperatureControl		= true;
		m_UTSSettings.m_ActiveTemperatureThreshold 		= 250.0;
		m_UTSSettings.m_InactiveTemperatureThreshold 	= 975.0;
	}
	
	//================================================================
	// ONSTORESAVE/LOAD/AFTERLOAD
	//================================================================
	override void OnStoreSave( ParamsWriteContext ctx )
	{   
		super.OnStoreSave( ctx );

		//fire point name
		ctx.Write( m_FirePointIndex );
		
		//smoke position
		ctx.Write( m_SmokePosX );
		ctx.Write( m_SmokePosY );
		ctx.Write( m_SmokePosZ );
	}

	override bool OnStoreLoad( ParamsReadContext ctx, int version )
	{
		if ( !super.OnStoreLoad( ctx, version ) )
			return false;

		//--- Fireplace Indoor data ---
		//fire point name
		if ( !ctx.Read( m_FirePointIndex ) )
		{
			m_FirePointIndex = 1;		//set default
			return false;
		}
		
		//smoke position
		if ( !ctx.Read( m_SmokePosX ) )
		{
			m_SmokePosX = 0;		//set default
			return false;
		}
		if ( !ctx.Read( m_SmokePosY ) )
		{
			m_SmokePosY = 0;		//set default
			return false;
		}
		if ( !ctx.Read( m_SmokePosZ ) )
		{
			m_SmokePosZ = 0;		//set default
			return false;
		}
		//---

		return true;
	}
		
	//================================================================
	// FIRE POINT (HOUSE)
	// LIMITED TO 1 DECIMAL PLACE (0-9)
	//================================================================
	static int GetFirePointIndex( string action_selection )
	{
		int index_location = action_selection.Length() - 1;
		return action_selection.Substring( index_location, 1 ).ToInt();
	}
	
	void SetFirePointIndex( int fire_point_index )
	{
		m_FirePointIndex = fire_point_index;
	}
	
	static bool CanPlaceFireplaceInSelectedSpot( Object building, int fire_point_index, out vector fire_point_pos_world, out vector fire_point_rot_world )
	{
		//Get fire point index position
		vector fire_point_pos = building.GetSelectionPositionMS( OVENPOINT_FIRE_POSITION + fire_point_index.ToString() );
		vector fire_point_rot = building.GetSelectionPositionMS( OVENPOINT_PLACE_ROT + fire_point_index.ToString() );
		fire_point_pos_world = building.ModelToWorld( fire_point_pos );
		fire_point_rot_world = building.ModelToWorld( fire_point_rot );
		
		//check if there is any FireplaceIndoor objects near selected fire point
		ref array<Object> nearest_objects = new array<Object>;
		ref array<CargoBase> proxy_cargos = new array<CargoBase>;
		GetGame().GetObjectsAtPosition3D( fire_point_pos_world, 1, nearest_objects, proxy_cargos );

		for ( int i = 0; i < nearest_objects.Count(); ++i )
		{
			Object object = nearest_objects.Get( i );
			
			if ( object.IsInherited( OvenIndoor ) )
			{
				return false;
			}
		}
		
		return true;
	}
	
	void SetSmokePointPosition( vector smoke_point_pos )
	{
		m_SmokePosX = smoke_point_pos[0];
		m_SmokePosY = smoke_point_pos[1];
		m_SmokePosZ = smoke_point_pos[2];
	}
	
	//================================================================
	// PARTICLES
	//================================================================
	override protected vector GetSmokeEffectPosition()
	{
		return Vector( m_SmokePosX, m_SmokePosY, m_SmokePosZ );
	}
	
	//small smoke
	override void ParticleSmallSmokeStart()
	{
		PlayParticle( m_ParticleSmallSmoke, PARTICLE_SMALL_SMOKE, GetSmokeEffectPosition(), true );
	}
	
	//normal smoke
	override void ParticleNormalSmokeStart()
	{
		PlayParticle( m_ParticleNormalSmoke, PARTICLE_NORMAL_SMOKE, GetSmokeEffectPosition(), true );
	}

	//================================================================
	// STATE
	//================================================================
	override bool IsIndoorOven()
	{
		return true;
	}

	override bool CanReleaseAttachment(EntityAI attachment)
	{
		if (!super.CanReleaseAttachment(attachment))
		{
			return false;
		}
		
		ItemBase item = ItemBase.Cast(attachment);
		if (IsKindling(item) || IsFuel(item))
		{
			return !IsBurning();
		}
		
		return true;
	}

	override void EEItemAttached(EntityAI item, string slot_name)
	{
		super.EEItemAttached(item, slot_name);
		
		ItemBase item_base = ItemBase.Cast(item);
		
		if (IsKindling(item_base) || IsFuel(item_base))
		{
			AddToFireConsumables(item_base);
		}
		
		// direct cooking/smoking slots
		bool edible_base_attached = false;
		switch (slot_name)
		{
		case "DirectCookingA":
			m_DirectCookingSlots[0] = item_base;
			edible_base_attached = true;
		break;
		case "SmokingA":
			m_SmokingSlots[0] = item_base;
			edible_base_attached = true;
		break;
		case "SmokingB":
			m_SmokingSlots[1] = item_base;
			edible_base_attached = true;
		break;
		}
		
		RefreshFireplaceVisuals();
	}

	override void EEItemDetached(EntityAI item, string slot_name) 
	{
		super.EEItemDetached(item, slot_name);
		
		ItemBase item_base = ItemBase.Cast(item);
		
		if (IsKindling(item_base) || IsFuel(item_base))
		{
			RemoveFromFireConsumables(GetFireConsumableByItem(item_base));
		}
		
		CheckForDestroy();

		// direct cooking/smoking slots
		switch (slot_name)
		{
		case "DirectCookingA":
			m_DirectCookingSlots[0] = null;
		break;
		case "SmokingA":
			m_SmokingSlots[0] = null;
		break;

		case "SmokingB":
			m_SmokingSlots[1] = null;
		break;
		}

		// cookware-specifics (remove audio visuals and clear references)
		if (item_base.IsCookware())
		{
			ClearCookingEquipment(item_base);
			item_base.RemoveAudioVisualsOnClient();	
		}
		
		if (item_base.IsLiquidContainer()) //boiling bottle effects stop
			item_base.RemoveAudioVisualsOnClient();
		
		RefreshFireplaceVisuals();
	}
		
	//================================================================
	// CONDITIONS
	//================================================================
	//this into/outo parent.Cargo
	override bool CanPutInCargo( EntityAI parent )
	{
		return false;
	}

	override bool CanRemoveFromCargo( EntityAI parent )
	{
		return true;
	}

	//hands
	override bool CanPutIntoHands( EntityAI parent )
	{
		return false;
	}
	
	override bool CanRemoveFromHands( EntityAI parent ) 
	{
		return false;
	}

	// Item-to-item fire distribution
	override bool HasFlammableMaterial()
	{
		return true;
	}
	
	override bool CanBeIgnitedBy( EntityAI igniter = NULL )
	{
		if ( HasAnyKindling() && !GetHierarchyParent() )
		{
			return true;
		}
			
		return false;
	}
	
	override bool CanIgniteItem( EntityAI ignite_target = NULL )
	{
		if ( IsBurning() )
		{
			return true;
		}
		
		return false;
	}
	
	override bool IsIgnited()
	{
		return IsBurning();
	}
	
	override void OnIgnitedTarget( EntityAI target_item )
	{
	}
	
	override void OnIgnitedThis( EntityAI fire_source )
	{	
		//start fire
		StartFire(); 
	}
	
	override bool IsThisIgnitionSuccessful( EntityAI item_source = NULL )
	{
		//check kindling
		if ( !HasAnyKindling() )
		{
			return false;
		}
		
		//check wetness
		if ( IsWet() )
		{
			return false;
		}
		
		return true;	
	}
}
