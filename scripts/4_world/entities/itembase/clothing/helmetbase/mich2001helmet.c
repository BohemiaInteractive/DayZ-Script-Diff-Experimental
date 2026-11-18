class Mich2001Helmet extends HelmetBase
{
	override void SetActions()
	{
		super.SetActions();
		
		AddAction(ActionTurnOnHelmetFlashlight); //use default light actions instead?
		AddAction(ActionTurnOffHelmetFlashlight);
		AddAction(ActionToggleNVG);
	}
	
	//Debug menu Spawn Ground Special
	override void OnDebugSpawn()
	{
		EntityAI entity;
		if ( Class.CastTo(entity, this) )
		{
			entity.GetInventory().CreateInInventory( "NVGoggles" );
			entity.GetInventory().CreateInInventory( "UniversalLight" );
			entity.GetInventory().CreateInInventory( "Battery9V" );
			entity.GetInventory().CreateInInventory( "Battery9V" );
		}
	}
	
	override protected void InitGlobalExclusionValues()
	{
		super.InitGlobalExclusionValues();
		
		AddSingleExclusionValueGlobal(EAttExclusions.EXCLUSION_HEADSTRAP_0);
	}
	
	override void OnWasDetached(EntityAI parent, int slot_id)
	{
		super.OnWasDetached(parent, slot_id);
		PlayerBase player = PlayerBase.Cast(parent);
		
		UpdateNVGStatus(player, false, true);
	}
};