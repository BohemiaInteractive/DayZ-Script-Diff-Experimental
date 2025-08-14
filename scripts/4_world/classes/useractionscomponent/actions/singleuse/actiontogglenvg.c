class ActionToggleNVG : ActionBase
{
	override bool IsInstant()
	{
		return true;
	}
	
	override void CreateConditionComponents()  
	{	
		m_ConditionItem 	= new CCINone();
		m_ConditionTarget 	= new CCTNonRuined(UAMaxDistances.DEFAULT);
	}
	
	override typename GetInputType()
	{
		return ToggleNVGActionInput;
	}
	
	override bool HasTarget()
	{
		return true;
	}
	
	override bool UseMainItem()
	{
		return false;
	}
	
	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		array<Entity> nvgs = player.GetCachedEquipment().GetEntitiesByCategory(ECachedEquipmentItemCategory.NVG, ECachedEquipmentPlacement.ATTACHMENT);
		foreach (Entity nvg : nvgs)
			return nvg != null;
		
		return false;
	}

	override void Start(ActionData action_data)
	{
		super.Start(action_data);
		
		array<Entity> nvgs = action_data.m_Player.GetCachedEquipment().GetEntitiesByCategory(ECachedEquipmentItemCategory.NVG, ECachedEquipmentPlacement.ATTACHMENT);
		foreach (Entity nvg : nvgs)
		{
			NVGoggles goggles;
			if (NVGoggles.CastTo(goggles, nvg))
			{
				goggles.RotateGoggles(goggles.m_IsLowered);
				break;
			}
		}
	}
}
