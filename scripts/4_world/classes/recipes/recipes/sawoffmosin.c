class SawoffMosin extends RecipeBase
{	
	override void Init()
	{
		m_Name = "#STR_sawoffbarrel0";
		m_IsInstaRecipe = false;				// should this recipe be performed instantly without animation
		m_AnimationLength = 2;					// animation length in relative time units

		//conditions
		m_MinDamageIngredient[0]	= -1;		// -1 = disable check
		m_MaxDamageIngredient[0]	= 3;		// -1 = disable check
		m_MinQuantityIngredient[0]	= -1;		// -1 = disable check
		m_MaxQuantityIngredient[0]	= -1;		// -1 = disable check

		m_MinDamageIngredient[1]	= -1;		// -1 = disable check
		m_MaxDamageIngredient[1]	= 3;		// -1 = disable check
		m_MinQuantityIngredient[1]	= -1;		// -1 = disable check
		m_MaxQuantityIngredient[1]	= -1;		// -1 = disable check

		//----------------------------------------------------------------------------------------------------------------------
		//INGREDIENTS
		//ingredient 1
		InsertIngredient(0,"Mosin9130");					//you can insert multiple ingredients this way

		m_IngredientAddHealth[0]		= 0;				// 0 = do nothing
		m_IngredientSetHealth[0]		= -1;				// -1 = do nothing
		m_IngredientAddQuantity[0]		= 0;				// 0 = do nothing
		m_IngredientDestroy[0]			= false;			// true = destroy, false = do nothing

		//ingredient 2
		InsertIngredient(1,"Hacksaw", DayZPlayerConstants.CMD_ACTIONFB_SPLITTING_FIREWOOD, true);

		m_IngredientAddHealth[1]		= -10;
		m_IngredientSetHealth[1]		= -1;
		m_IngredientAddQuantity[1]		= 0;
		m_IngredientDestroy[1]			= false;

		//----------------------------------------------------------------------------------------------------------------------
		//RESULTS
	}

	//final check for recipe's validity
	override bool CanDo(ItemBase ingredients[], PlayerBase player)
	{
		return true;
	}

	//gets called upon recipe's completion
	override void Do(ItemBase ingredients[], PlayerBase player,array<ItemBase> results, float specialty_weight)
	{
		// texture variants: "Mosin9130_Desert" -> "SawedoffMosin9130_Desert", falls back to the default sawn-off if that class doesn't exist
		string baseType = "Mosin9130";
		string sawnType = "SawedoffMosin9130";
		string type = ingredients[0].GetType();
		string result = sawnType + type.Substring(baseType.Length(), type.Length() - baseType.Length());
		if (!g_Game.ConfigIsExisting("CfgWeapons " + result))
		{
			result = sawnType;
		}

		MiscGameplayFunctions.TurnItemIntoItemEx(player, new TurnItemIntoItemLambda(ingredients[0], result, player));
	}
};
