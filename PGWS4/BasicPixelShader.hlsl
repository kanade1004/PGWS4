float4 BasicPS(float4 pos:POSITION) : SV_TARGET
{
	//�_�C�������h�ɋP�����������邽�߂�0.5f����1.0f�ɕύX���A��Z����
	return float4((float2(0.5f,1.0f) + pos.yx) * 1.0f,1.0f,1.0f);
}