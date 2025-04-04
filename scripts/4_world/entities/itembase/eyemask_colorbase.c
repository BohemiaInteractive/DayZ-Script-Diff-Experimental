class EyeMask_ColorBase : Glasses_Base 
{
	//================================================================
	// IGNITION ACTION
	//================================================================
	override bool HasFlammableMaterial()
	{
		return true;
	}
	
	override bool CanBeIgnitedBy(EntityAI igniter = null)
	{
		return !GetHierarchyParent();
	}
	
	override bool CanIgniteItem(EntityAI ignite_target = null)
	{
		return false;
	}
	
	override void OnIgnitedTarget(EntityAI target_item) {}
	
	override void OnIgnitedThis(EntityAI fire_source)
	{
		Fireplace.IgniteEntityAsFireplace(this, fire_source);
	}

	override bool IsThisIgnitionSuccessful(EntityAI item_source = null)
	{
		return Fireplace.CanIgniteEntityAsFireplace(this);
	}
	//================================================================
	
	override void SetActions()
	{
		super.SetActions();

		AddAction(ActionCreateIndoorFireplace);
		AddAction(ActionCreateIndoorOven);
		AddAction(ActionAttach);
		AddAction(ActionDetach);
	}
}

class EyeMask_NewYears : EyeMask_ColorBase {}
class EyeMask_Christmas : EyeMask_ColorBase {}
class EyeMask_Dead : EyeMask_ColorBase {}
class EyeMask_Yellow : EyeMask_ColorBase {}
class EyeMask_Red : EyeMask_ColorBase {}
class EyeMask_Blue : EyeMask_ColorBase {}
class EyeMask_Black : EyeMask_ColorBase {}
class EyeMask_Valentines : EyeMask_ColorBase {}