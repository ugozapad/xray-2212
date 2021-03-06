#include "stdafx.h"
#include "build.h"
#include "ogf_face.h"
#include "std_classes.h"
#include "fs.h"
#include "fmesh.h"
#include "xrOcclusion.h"
#include "nvMeshMender.h"

using namespace std;

void set_status(char* N, int id, int f, int v)
{
	string1024 status_str;

	sprintf	(status_str,"Model #%4d [F:%5d, V:%5d]: %s...",id,f,v,N);
	Status	(status_str);
	clMsg	(status_str);
}

BOOL OGF_Vertex::similar(OGF* ogf, OGF_Vertex& V)
{
	const float ntb		= _cos	(deg2rad(5.f));
	if (!P.similar(V.P)) 		return FALSE;
	if (!N.similar(V.N)) 		return FALSE;
	if (!T.similar(V.T)) 		return FALSE;
	if (!B.similar(V.B)) 		return FALSE;
	
	R_ASSERT(UV.size()==V.UV.size());
	for (u32 i=0; i<V.UV.size(); i++) {
		OGF_Texture *T = &*ogf->textures.begin()+i;
		b_texture	*B = T->pSurface;
		float		eu = 1.f/float(B->dwWidth );
		float		ev = 1.f/float(B->dwHeight);
		if (!UV[i].similar(V.UV[i],eu,ev)) return FALSE;
	}
	return TRUE;
}

u16 OGF::_BuildVertex	(OGF_Vertex& V1)
{
	try {
		for (itOGF_V it=vertices.begin(); it!=vertices.end(); it++)
		{
			if (it->similar(this,V1)) return u16(it-vertices.begin());
		}
	} catch (...) { clMsg("* ERROR: OGF::_BuildVertex");	}

	vertices.push_back	(V1);
	return (u32)vertices.size()-1;
}

void OGF::_BuildFace	(OGF_Vertex& V1, OGF_Vertex& V2, OGF_Vertex& V3)
{
	OGF_Face F;
	u32	VertCount = (u32)vertices.size();
	F.v[0]	= _BuildVertex(V1);
	F.v[1]	= _BuildVertex(V2);
	F.v[2]	= _BuildVertex(V3);
	if (!F.Degenerate()) {
		for (itOGF_F I=faces.begin(); I!=faces.end(); I++)		if (I->Equal(F)) return;
		faces.push_back(F);
	} else {
		if (vertices.size()>VertCount) 
			vertices.erase(vertices.begin()+VertCount,vertices.end());
	}
}

void OGF::Optimize()
{
	// Real optimization
//	set_status("Optimizing UV",treeID,faces.size(),vertices.size());

	// Detect relevant number of UV pairs
	R_ASSERT			(vertices.size());
	dwRelevantUV		= vertices.front().UV.size();
	dwRelevantUVMASK	= 0;
	for (u32 t=0; t<dwRelevantUV; t++) dwRelevantUVMASK |= 1<<t;

	Shader_xrLC*	SH	= pBuild->shaders.Get(pBuild->materials[material].reserved);
	if (!SH->flags.bOptimizeUV)		return;

	// Optimize texture coordinates
	// 1. Calc bounds
	Fvector2 Tdelta[8];
	for (u32 i=0; i<8; i++)
	{
		if (0==(dwRelevantUVMASK&(1<<i))) continue;	// skip unneeded UV
		Fvector2 Tmin,Tmax;
		Tmin.set(flt_max,flt_max);
		Tmax.set(flt_min,flt_min);
		for (u32 j=0; j<vertices.size(); j++)
		{
			OGF_Vertex& V = vertices[j];
			Tmin.min(V.UV[i]);
			Tmax.max(V.UV[i]);
		}
		Tdelta[i].x = floorf((Tmax.x-Tmin.x)/2+Tmin.x);
		Tdelta[i].y = floorf((Tmax.y-Tmin.y)/2+Tmin.y);
	}

	// 2. Recalc UV mapping
	for (u32 i=0; i<vertices.size(); i++)
	{
		svector<Fvector2,2>& UV = vertices[i].UV;
		for (int j=0; j<2; j++)
		{
			if (dwRelevantUVMASK&(1<<j))	UV[j].sub(Tdelta[j]);
			// else							UV[j].set(0,0 );
		}
	}
}

/*
void OGF::CreateOccluder()
{
	ORM_Vertex* pV = ORM_Begin(vertices.size(),faces.size()*3,(u16*)faces.begin());
	for (u32 i=0; i<vertices.size(); i++)
	{
		OGF_Vertex& V = vertices[i];
		pV->x = V.P.x;
		pV->y = V.P.y;
		pV->z = V.P.z;
		pV->dummycolor = 0xffffffff;
		pV++;
	}
	ORM_End(C,R);
}
*/
// Calculate T&B
void OGF::CalculateTB()
{
	u32 v_count_reserve			= 3*iFloor(float(vertices.size())*1.33f);
	u32 i_count_reserve			= 3*faces.size();

	// Declare inputs
	xr_vector<NVMeshMender::VertexAttribute> 			input;
	input.push_back	(NVMeshMender::VertexAttribute());	// pos
	input.push_back	(NVMeshMender::VertexAttribute());	// norm
	input.push_back	(NVMeshMender::VertexAttribute());	// tex0
	input.push_back	(NVMeshMender::VertexAttribute());	// color
	input.push_back	(NVMeshMender::VertexAttribute());	// *** faces

	input[0].Name_= "position";	xr_vector<float>&	i_position	= input[0].floatVector_;	i_position.reserve	(v_count_reserve);
	input[1].Name_= "normal";	xr_vector<float>&	i_normal	= input[1].floatVector_;	i_normal.reserve	(v_count_reserve);
	input[2].Name_= "tex0";		xr_vector<float>&	i_tc		= input[2].floatVector_;	i_tc.reserve		(v_count_reserve);
	input[3].Name_= "clr";		xr_vector<float>&	i_color		= input[3].floatVector_;	i_normal.reserve	(v_count_reserve);
	input[4].Name_= "indices";	xr_vector<int>&		i_indices	= input[4].intVector_;		i_indices.reserve	(i_count_reserve);

	// Declare outputs
	xr_vector<NVMeshMender::VertexAttribute> 			output;
	output.push_back(NVMeshMender::VertexAttribute());	// position, needed for mender
	output.push_back(NVMeshMender::VertexAttribute());	// normal
	output.push_back(NVMeshMender::VertexAttribute());	// tangent
	output.push_back(NVMeshMender::VertexAttribute());	// binormal
	output.push_back(NVMeshMender::VertexAttribute());	// tex0
	output.push_back(NVMeshMender::VertexAttribute());	// color
	output.push_back(NVMeshMender::VertexAttribute());	// *** faces

	output[0].Name_= "position";
	output[1].Name_= "normal";
	output[2].Name_= "tangent";	
	output[3].Name_= "binormal";
	output[4].Name_= "tex0";	
	output[5].Name_= "clr";	
	output[6].Name_= "indices";	

	// fill inputs (verts&indices)
	for (itOGF_V vert_it=vertices.begin(); vert_it!=vertices.end(); vert_it++){
		OGF_Vertex	&iV = *vert_it;
		i_position.push_back(iV.P.x);		i_position.push_back(iV.P.y);		i_position.push_back(iV.P.z);
		i_normal.push_back	(iV.N.x);  		i_normal.push_back	(iV.N.y);		i_normal.push_back	(iV.N.z);
		i_color.push_back	(iV.Color._x);	i_color.push_back	(iV.Color._y);	i_color.push_back	(iV.Color._z);
		i_tc.push_back		(iV.UV[0].x);	i_tc.push_back		(iV.UV[0].y);	i_tc.push_back		(0);
	}
	for (itOGF_F face_it=faces.begin(); face_it!=faces.end(); face_it++){
		OGF_Face	&iF = *face_it;
		i_indices.push_back	(iF.v[0]);
		i_indices.push_back	(iF.v[1]);
		i_indices.push_back	(iF.v[2]);
	}

	// Perform munge
	NVMeshMender mender;
	if (!mender.Munge(
		input,										// input attributes
		output,										// outputs attributes
		deg2rad(75.f),								// tangent space smooth angle
		0,											// no texture matrix applied to my texture coordinates
		NVMeshMender::FixTangents,					// fix degenerate bases & texture mirroring
		NVMeshMender::DontFixCylindricalTexGen,		// handle cylindrically mapped textures via vertex duplication
		NVMeshMender::DontWeightNormalsByFaceSize	// weigh vertex normals by the triangle's size
		))
	{
		Debug.fatal	("NVMeshMender failed (%s)",mender.GetLastError().c_str());
	}

	// Bind declarators
	xr_vector<float>&	o_position	= output[0].floatVector_;	R_ASSERT(output[0].Name_=="position");
	xr_vector<float>&	o_normal	= output[1].floatVector_;	R_ASSERT(output[1].Name_=="normal");
	xr_vector<float>&	o_tangent	= output[2].floatVector_;	R_ASSERT(output[2].Name_=="tangent");
	xr_vector<float>&	o_binormal	= output[3].floatVector_;	R_ASSERT(output[3].Name_=="binormal");
	xr_vector<float>&	o_tc		= output[4].floatVector_;	R_ASSERT(output[4].Name_=="tex0");
	xr_vector<float>&	o_color		= output[5].floatVector_;	R_ASSERT(output[5].Name_=="clr");
	xr_vector<int>&		o_indices	= output[6].intVector_;		R_ASSERT(output[6].Name_=="indices");

	// verify
	R_ASSERT		(3*faces.size()	== o_indices.size());
	u32 v_cnt		= o_position.size();
	R_ASSERT		(0==v_cnt%3);
	R_ASSERT		(v_cnt == o_normal.size());
	R_ASSERT		(v_cnt == o_tangent.size());
	R_ASSERT		(v_cnt == o_binormal.size());
	R_ASSERT		(v_cnt == o_tc.size());
	R_ASSERT		(v_cnt == o_color.size());
	v_cnt			/= 3;

	// retriving data
	u32 o_idx		= 0;
	for (face_it=faces.begin(); face_it!=faces.end(); face_it++){
		OGF_Face	&iF = *face_it;
		iF.v[0]		= o_indices[o_idx++];
		iF.v[1]		= o_indices[o_idx++];
		iF.v[2]		= o_indices[o_idx++];
	}
	vertices.clear	(); vertices.resize(v_cnt);
	for (u32 v_idx=0; v_idx!=v_cnt; v_idx++){
		OGF_Vertex	&iV = vertices[v_idx];
		iV.P.set	(o_position[v_idx*3+0],	o_position[v_idx*3+1],	o_position[v_idx*3+2]);
		iV.N.set	(o_normal[v_idx*3+0],	o_normal[v_idx*3+1],	o_normal[v_idx*3+2]);
		iV.T.set	(o_tangent[v_idx*3+0],	o_tangent[v_idx*3+1],	o_tangent[v_idx*3+2]);
		iV.B.set	(o_binormal[v_idx*3+0],	o_binormal[v_idx*3+1],	o_binormal[v_idx*3+2]);
		iV.UV.resize(1);
		iV.UV[0].set(o_tc[v_idx*3+0],		o_tc[v_idx*3+1]);
		iV.Color._set(o_color[v_idx*3+0],	o_color[v_idx*3+1],		o_color[v_idx*3+2]);
	}
}

// Make Progressive
xrCriticalSection	progressive_cs;
void OGF::MakeProgressive()
{
	progressive_cs.Enter();
	// prepare progressive geom
	VIPM_Init			();
	for (u32 v_idx=0;  v_idx<vertices.size(); v_idx++)
		VIPM_AppendVertex(vertices[v_idx].P,vertices[v_idx].UV[0]);
	for (u32 f_idx=0; f_idx<faces.size(); f_idx++)
		VIPM_AppendFace(faces[f_idx].v[0],faces[f_idx].v[1],faces[f_idx].v[2]);
	// Convert
	VIPM_Result* VR		= VIPM_Convert(u32(-1),1.f,1);
	if (VR->swr_records.size()>0){
		// Permute vertices
		vertices_saved			= vertices;
		for(u32 i=0; i<vertices.size(); i++)
			vertices[VR->permute_verts[i]]=vertices_saved[i];
		// Fill indices
		faces_saved				= faces;
		faces.resize			(VR->indices.size()/3);
		for (u32 f_idx=0; f_idx<faces.size(); f_idx++){
			faces[f_idx].v[0]	= VR->indices[f_idx*3+0];
			faces[f_idx].v[1]	= VR->indices[f_idx*3+1];
			faces[f_idx].v[2]	= VR->indices[f_idx*3+2];
		}
		// Fill SWR
		m_SWI.count				= VR->swr_records.size();
		m_SWI.sw				= xr_alloc<FSlideWindow>(m_SWI.count);
		for (u32 swr_idx=0; swr_idx!=m_SWI.count; swr_idx++){
			FSlideWindow& dst	= m_SWI.sw[swr_idx];
			VIPM_SWR& src		= VR->swr_records[swr_idx];
			dst.num_tris		= src.num_tris;
			dst.num_verts		= src.num_verts;
			dst.offset			= src.offset;
		}
	}
	// cleanup
	VIPM_Destroy		();
	progressive_cs.Leave();

/*
	if (faces.size()>c_PM_LowVertLimit) 
	{
//		set_status("CLODing",treeID,faces.size(),vertices.size());
		vertices_saved	= vertices;
		faces_saved		= faces;

		// Options
		PM_Init	(dwRelevantUV,dwRelevantUVMASK,3, 
			g_params.m_pm_uv, g_params.m_pm_pos, g_params.m_pm_curv, 
			g_params.m_pm_borderH_angle, g_params.m_pm_borderH_distance, 
			g_params.m_pm_heuristic);

		// Transfer vertices
		for(itOGF_V iV=vertices.begin();iV!=vertices.end();iV++) {
			PM_CreateVertex(
				iV->P.x,iV->P.y,iV->P.z,
				u32(iV - vertices.begin()),
				(P_UV*)iV->UV.begin()
				);
		}

		// Convert
		PM_Result R;
		I_Current	= PM_Convert((u16*)&*faces.begin(),(u32)faces.size()*3,&R);

		if (I_Current>=0) 
		{
			// Permute vertices
			vecOGF_V temp_list = vertices;
			
			// Perform permutation
			for(u32 i=0; i<temp_list.size(); i++)
				vertices[R.permutePTR[i]]=temp_list[i];
			
			// Copy results
			pmap_vsplit.resize(R.splitSIZE);
			CopyMemory(&*pmap_vsplit.begin(),R.splitPTR,R.splitSIZE*sizeof(Vsplit));

			pmap_faces.resize(R.facefixSIZE);
			CopyMemory(&*pmap_faces.begin(),R.facefixPTR,R.facefixSIZE*sizeof(u16));

			dwMinVerts = R.minVertices;
		}
	}
*/
}

void OGF_Base::Save	(IWriter &fs)
{
}

// Represent a node as HierrarhyVisual
void OGF_Node::Save	(IWriter &fs)
{
	OGF_Base::Save		(fs);

	// Header
	fs.open_chunk		(OGF_HEADER);
	ogf_header H;
	H.format_version	= xrOGF_FormatVersion;
	H.type				= MT_HIERRARHY;
	H.shader_id			= 0;
	H.bb.min			= bbox.min;
	H.bb.max			= bbox.max;
	H.bs.c				= C;
	H.bs.r				= R;
	fs.w				(&H,sizeof(H));
	fs.close_chunk		();

	// Children
	fs.open_chunk		(OGF_CHILDREN_L);
	fs.w_u32			((u32)chields.size());
	fs.w				(&*chields.begin(),(u32)chields.size()*sizeof(u32));
	fs.close_chunk		();
}

extern u16	RegisterShader	(LPCSTR T);

void OGF_LOD::Save		(IWriter &fs)
{
	OGF_Base::Save		(fs);

	// Header
	ogf_header			H;
	string1024			sid;
	strconcat			(sid,
		pBuild->shader_render[pBuild->materials[lod_Material].shader].name,
		"/",
		pBuild->textures[pBuild->materials[lod_Material].surfidx].name
		);
	fs.open_chunk		(OGF_HEADER);
	H.format_version	= xrOGF_FormatVersion;
	H.type				= MT_LOD;
	H.shader_id			= RegisterShader(sid);
	H.bb.min			= bbox.min;
	H.bb.max			= bbox.max;
	H.bs.c				= C;
	H.bs.r				= R;
	fs.w				(&H,sizeof(H));
	fs.close_chunk		();

	// Chields
	fs.open_chunk		(OGF_CHILDREN_L);
	fs.w_u32			((u32)chields.size());
	fs.w				(&*chields.begin(),(u32)chields.size()*sizeof(u32));
	fs.close_chunk		();

	// Lod-def
	fs.open_chunk		(OGF_LODDEF2);
	fs.w				(lod_faces,sizeof(lod_faces));
	fs.close_chunk		();
}
