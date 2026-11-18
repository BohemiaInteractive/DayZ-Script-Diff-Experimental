class CraftBoltsActionData : ActionData
{
	bool m_IsKnifeInHands;
	Ammunition_Base m_ResultEntity;
}

class ActionCraftBoltsCB : ActionContinuousBaseCB
{
	private const float TIME_TO_CRAFT = 3.0;
	
	override void CreateActionComponent()
	{
		m_ActionData.m_ActionComponent = new CAContinuousRepeat(TIME_TO_CRAFT);
	}
};

class ActionCraftBolts: ActionContinuousBase
{
	const ref set<string> CUTTING_TYPES = new set<string>();
	
	// deprecated
	protected bool 				m_IsKnifeInHands;
	protected Ammunition_Base 	m_ResultEntity;
	
	void InitCuttingTypes()
	{
		CUTTING_TYPES.Clear();
		CUTTING_TYPES.Insert("Sickle");
		CUTTING_TYPES.Insert("KukriKnife");
		CUTTING_TYPES.Insert("FangeKnife");
		CUTTING_TYPES.Insert("KitchenKnife");
		CUTTING_TYPES.Insert("SteakKnife");
		CUTTING_TYPES.Insert("StoneKnife");
		CUTTING_TYPES.Insert("Cleaver");
		CUTTING_TYPES.Insert("CombatKnife");
		CUTTING_TYPES.Insert("Jambiya");
		CUTTING_TYPES.Insert("HuntingKnife");
		CUTTING_TYPES.Insert("Machete");
		CUTTING_TYPES.Insert("CrudeMachete");
		CUTTING_TYPES.Insert("OrientalMachete");
		CUTTING_TYPES.Insert("WoodAxe");
		CUTTING_TYPES.Insert("Hatchet");
		CUTTING_TYPES.Insert("FirefighterAxe");
		CUTTING_TYPES.Insert("AK_Bayonet");
		CUTTING_TYPES.Insert("M9A1_Bayonet");
		CUTTING_TYPES.Insert("SKS_Bayonet");
		CUTTING_TYPES.Insert("BoneKnife");
		CUTTING_TYPES.Insert("Sword");
		CUTTING_TYPES.Insert("Scimitar");
		CUTTING_TYPES.Insert("DonerKnife");
		CUTTING_TYPES.Insert("Akinaka");
		CUTTING_TYPES.Insert("BrickTrowel");
		CUTTING_TYPES.Insert("P1907_Bayonet");
	}
	
	void ActionCraftBolts()
	{
		m_CallbackClass 	= ActionCraftBoltsCB;
		m_CommandUID 		= DayZPlayerConstants.CMD_ACTIONFB_CRAFTING;
		m_FullBody 			= true;
		m_StanceMask		= DayZPlayerConstants.STANCEMASK_CROUCH;
		m_SpecialtyWeight 	= UASoftSkillsWeight.ROUGH_HIGH;
		
		m_Text = "#STR_CraftBolt0";

		InitCuttingTypes();
	}
	
	override void CreateConditionComponents()  
	{		
		m_ConditionItem 	= new CCINonRuined();
		m_ConditionTarget 	= new CCTNonRuined();
	}
	
	protected bool IsCuttingType(Object item)
	{
		return CUTTING_TYPES.Find(item.ClassName()) >= 0;
	}
	
	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (IsCuttingType(item))
		{
			//knife in hands
			if (target.GetObject())
				return (target.GetObject().ClassName() == "WoodenStick");
		}
		else if (target.GetObject())
		{
			// material in hands
			return IsCuttingType(target.GetObject());
		}

		return false;
	}

	override ActionData CreateActionData()
	{
		CraftBoltsActionData action_data = new CraftBoltsActionData;
		return action_data;
	}

	override void OnStartServer(ActionData action_data)
	{
		super.OnStartServer(action_data);
		
		CraftBoltsActionData craftData = CraftBoltsActionData.Cast(action_data);
		craftData.m_IsKnifeInHands = IsCuttingType(action_data.m_MainItem);
		craftData.m_ResultEntity = null;
	}
	
	override void OnFinishProgressServer(ActionData action_data)
	{
		CraftBoltsActionData craftData = CraftBoltsActionData.Cast(action_data);
		ItemBase knife;
		ItemBase material;

		bool added = false;
		
		if (craftData.m_IsKnifeInHands)
		{
			//knife in hands
			knife = action_data.m_MainItem;
			material = ItemBase.Cast(action_data.m_Target.GetObject());
		}
		else
		{
			// material in hands
			material = action_data.m_MainItem;
			knife = ItemBase.Cast(action_data.m_Target.GetObject());
		}
		
		if (!material || !knife)
			return;
		
		if (craftData.m_ResultEntity)
		{
			if (craftData.m_ResultEntity.GetAmmoCount() < craftData.m_ResultEntity.GetAmmoMax())
			{
				craftData.m_ResultEntity.ServerAddAmmoCount(1);
				added = true;
			}
		}
		
		if (!added)
		{
			craftData.m_ResultEntity = Ammunition_Base.Cast(action_data.m_Player.SpawnEntityOnGroundRaycastDispersed("Ammo_ImprovisedBolt_1"));
			craftData.m_ResultEntity.SetHealth("", "", material.GetHealth("", ""));
			craftData.m_ResultEntity.ServerSetAmmoCount(1);
		}
		
		material.AddQuantity(-1);
		knife.AddHealth("","",-3);
	}
};
