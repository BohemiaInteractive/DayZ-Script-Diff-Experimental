class ConstructionPartInsiderBoxData
{
	const int PARAM_COUNT = 2;
	
	vector m_Min;
	vector m_Max;
}

class ConstructionPartExcluderBoxData
{
	const int PARAM_COUNT = 3;
	
	vector m_Min;
	vector m_Max;
	vector m_Angle; //! Only yaw supported for now (L/R orientation)
}
