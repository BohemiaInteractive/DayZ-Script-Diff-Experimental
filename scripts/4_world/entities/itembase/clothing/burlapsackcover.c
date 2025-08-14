class BurlapSackCover extends HeadGear_Base
{
	protected PlayerBase m_Player;
		
	void ~BurlapSackCover()
	{
		if (m_Player)
		{
			OnRemovedFromHead(m_Player);
		}
	}
	
	override void EEItemLocationChanged(notnull InventoryLocation oldLoc, notnull InventoryLocation newLoc)
	{
		super.EEItemLocationChanged(oldLoc,newLoc);
		
		if (GetGame().IsDedicatedServer() && newLoc.GetType() == InventoryLocationType.GROUND)
		{
			EntityAI newItem = EntityAI.Cast(GetGame().CreateObjectEx("BurlapSack",newLoc.GetPos(),ECE_PLACE_ON_SURFACE,RF_DEFAULT));
			MiscGameplayFunctions.TransferItemProperties(this,newItem);
			DeleteSafe();
		}
	}
	
	override void OnWasAttached(EntityAI parent, int slot_id)
	{
		super.OnWasAttached(parent, slot_id);
		
		if (slot_id == InventorySlots.HEADGEAR)
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(HandleAttachedToHead, 0);
	}

	override bool CanPutInCargo(EntityAI parent)
	{
		if (!super.CanPutInCargo(parent))
			return false;
		
		if (parent && parent != this)
			return true;
		
		return false;
	}

	override bool CanDetachAttachment(EntityAI parent)
	{
		return false;
	}

	protected void OnRemovedFromHead(PlayerBase player)
	{
		if ((!GetGame().IsServer() || !GetGame().IsMultiplayer()) && PlayerBase.Cast(GetGame().GetPlayer()) == player) // Client side
		{
			PPERequesterBank.GetRequester(PPERequester_BurlapSackEffects).Stop();
			player.SetInventorySoftLock(false);
			player.SetMasterAttenuation("");
		}
		
		if (GetGame().IsServer() || GetGame().IsMultiplayer()) // Server side or single player
			SetInvisibleRecursive(false, player, {InventorySlots.MASK, InventorySlots.EYEWEAR});
	}
	
	override protected set<int> GetAttachmentExclusionInitSlotValue(int slotId)
	{
		set<int> ret = super.GetAttachmentExclusionInitSlotValue(slotId);
		if (slotId == InventorySlots.HEADGEAR)
		{
			ret.Insert(EAttExclusions.SHAVING_HEADGEAR_ATT_0);
		}
		return ret;
	}
	
	protected void HandleAttachedToHead()
	{
		Class.CastTo(m_Player, GetHierarchyRootPlayer());
		if (!m_Player)
			return;
		
		if (!GetGame().IsServer() || !GetGame().IsMultiplayer()) // Client side
		{
			if (m_Player && PlayerBase.Cast(GetGame().GetPlayer()) == m_Player)
			{
				PPERequesterBase ppeRB = PPERequesterBank.GetRequester(PPERequester_BurlapSackEffects);
				if (ppeRB)
					ppeRB.Start();
				
				m_Player.SetInventorySoftLock(true);
				m_Player.SetMasterAttenuation("BurlapSackAttenuation");
				
				if (GetGame().GetUIManager().IsMenuOpen(MENU_INVENTORY))
					GetGame().GetMission().HideInventory();
			}
		}
		
		if (GetGame().IsServer() || GetGame().IsMultiplayer()) // Server side or single player
			SetInvisibleRecursive(true, m_Player, {InventorySlots.MASK, InventorySlots.EYEWEAR});
	}
}
