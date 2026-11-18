class ActionWashHeadItemContinuousCB : ActionContinuousBaseCB
{
	override void CreateActionComponent()
	{
		m_ActionData.m_ActionComponent = new CAContinuousRepeat(UATimeSpent.WASH_FACE_ITEM);
	}
}

class ActionWashHeadItemContinuous : ActionWashHeadWettingClothesBase
{
	void ActionWashHeadItemContinuous()
	{
		m_CallbackClass 	= ActionWashHeadItemContinuousCB;
		m_CommandUID		= DayZPlayerConstants.CMD_ACTIONMOD_WASHFACE;
		m_FullBody			= false;
		m_StanceMask 		= DayZPlayerConstants.STANCEMASK_CROUCH | DayZPlayerConstants.STANCEMASK_ERECT;

		m_Text = "#STR_Wash_Head";
	}
	
	override void CreateConditionComponents()  
	{	
		m_ConditionItem 	= new CCINotRuinedAndEmpty();
		m_ConditionTarget 	= new CCTSelf();
	}

	override bool HasTarget()
	{
		return false;
	}
	
	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if ( item.GetLiquidType() != LIQUID_GASOLINE)
		{
			return true;
		}
		return false;
	}

	override protected float GetEyeIrritationReduction() { return 16; }
	override protected bool IsFromContainer() { return true; }

	override void OnFinishProgressServer(ActionData action_data)
	{
		super.OnFinishProgressServer(action_data);

		action_data.m_MainItem.AddQuantity(-50, false);
		if (action_data.m_MainItem.GetTemperature() > PlayerConstants.WASHING_DAMAGE_TEMP_THRESHOLD)
			action_data.m_Player.ProcessDirectDamage(DamageType.CUSTOM, action_data.m_MainItem, "", "EnviroDmg", "0 0 0", PlayerConstants.WASHING_DAMAGE_PER_WASH);
	}
}
