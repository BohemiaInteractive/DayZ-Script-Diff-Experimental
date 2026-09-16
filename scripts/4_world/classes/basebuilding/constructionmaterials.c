//! instanced data holder for single construction material
class ConstructionMaterialData
{
	bool m_MaterialLockable;
	bool m_SkipOnRepair = false; //does not require the material to repair
	bool m_SkipOnDismantle = false; //does not return the material on dismantle
	int m_MaterialSlotID;
	float m_MaterialQuantity;
	string m_MaterialType;
	string m_MaterialSlotName;
	
	
	void ConstructionMaterialData(string cfgPath)
	{
		Init(cfgPath);
	}
	
	protected void Init(string cfgPath)
	{
		m_MaterialLockable = false;
		m_MaterialSlotID = InventorySlots.INVALID;
		m_MaterialQuantity = -1;
		m_MaterialType = "";
		m_MaterialSlotName = "";
		
		GetGame().ConfigGetText(cfgPath + " type", m_MaterialType);
		GetGame().ConfigGetText(cfgPath + " slot_name", m_MaterialSlotName);
		m_MaterialSlotID = InventorySlots.GetSlotIdFromString(m_MaterialSlotName);
		m_MaterialQuantity = GetGame().ConfigGetFloat(cfgPath + " quantity");
		m_MaterialLockable = GetGame().ConfigGetInt(cfgPath + " lockable");
		m_SkipOnRepair = GetGame().ConfigGetInt(cfgPath + " skipOnRepair");
		m_SkipOnDismantle = GetGame().ConfigGetInt(cfgPath + " skipOnDismantle");
	}
}
