//! holder object for entity type-level parts info
class ConstructionDataTypeHolder : ConstructionDataTypeHolderBasic
{
	protected ref map<string, ref ConstructionPartTypeData> m_ConstructionPartTypeMap;
	protected ref array<ConstructionPartTypeData> m_ConstructionPartTypes;
	protected string m_EntityTypeName;
	
	//! Called manually after spawning from typename
	override void InitPartsTypeDataHolder(string name, string cfgPath)
	{
		super.InitPartsTypeDataHolder(name, cfgPath);
		
		m_EntityTypeName = name;
		m_ConstructionPartTypeMap = new map<string, ref ConstructionPartTypeData>();
		m_ConstructionPartTypes = new array<ConstructionPartTypeData>();
		
		//main parts
		for (int i = 0; i < g_Game.ConfigGetChildrenCount(cfgPath); ++i)
		{
			string mainPartName;
			g_Game.ConfigGetChildName(cfgPath, i, mainPartName);
			string mainPartPath = cfgPath + " " + mainPartName;
			
			//parts
			for (int j = 0; j < g_Game.ConfigGetChildrenCount(mainPartPath); ++j)
			{
				string partName = "";
				g_Game.ConfigGetChildName(mainPartPath, j, partName);
				string partPath = mainPartPath + " " + partName;
				ConstructionPartTypeData typeDataObject;
				
				//check creation of a custom part data type first
				string customTypeStr;
				string customPartPath = partPath + " custom_part_type";
				if (g_Game.ConfigIsExisting(customPartPath) && g_Game.ConfigGetText(customPartPath, customTypeStr))
				{
					customTypeStr = customTypeStr + "TypeData";
					Class.CastTo(typeDataObject, customTypeStr.ToType().Spawn());
				}
				
				//default typeDataObject creation
				if (!typeDataObject)
					typeDataObject = new ConstructionPartTypeData();
				
				PartTypeInitData initData = new PartTypeInitData();
				initData.m_PartPath = partPath;
				initData.m_PartName = partName;
				initData.m_MainPartName = mainPartName;
				typeDataObject.InitPartTypeData(initData);
				
				//general behavior for the entire construction
				if (g_Game.ConfigIsExisting(cfgPath + " can_decay_general"))
					typeDataObject.SetCanDecayConfig(g_Game.ConfigGetInt(cfgPath + " can_decay_general"));
				
				m_ConstructionPartTypeMap.Insert(partName, typeDataObject);
				m_ConstructionPartTypes.Insert(typeDataObject);
			}
		}
	}
	
	map<string, ref ConstructionPartTypeData> GetConstructionPartTypesMap()
	{
		return m_ConstructionPartTypeMap;
	}
	
	array<ConstructionPartTypeData> GetConstructionPartTypeDataArray()
	{
		return m_ConstructionPartTypes;
	}
	
	string GetOwnerEntityTypeName()
	{
		return m_EntityTypeName;
	}
}