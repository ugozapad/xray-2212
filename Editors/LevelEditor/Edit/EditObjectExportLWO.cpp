#include "stdafx.h"
#pragma hdrstop

#include "EditObject.h"
#include "EditMesh.h"
#include "FS2.h"

int FindLPCSTR(LPCSTRVec& vec, LPCSTR key){
	for (LPCSTRIt it=vec.begin(); it!=vec.end(); it++)
		if (0==strcmp(*it,key)) return it-vec.begin();
	return -1;
}

bool CEditableObject::ExportLWO(LPCSTR fname)
{
	CLWMemoryStream* F = xr_new<CLWMemoryStream>();

	LPCSTRVec images;

	F->begin_save();
		// tags
		F->open_chunk(ID_TAGS);
			for (SurfaceIt s_it=m_Surfaces.begin(); s_it!=m_Surfaces.end(); s_it++){
				CSurface* S=*s_it;
				F->w_stringZ(S->_Name());
				S->tag = s_it-m_Surfaces.begin();
				if (FindLPCSTR(images,S->_Texture())<0) images.push_back(S->_Texture());
			}
		F->close_chunk();
		// images
		for (LPCSTRIt im_it=images.begin(); im_it!=images.end(); im_it++){
			F->open_chunk(ID_CLIP);
				F->w_u32(im_it-images.begin());
				F->open_subchunk(ID_STIL);
					F->w_stringZ(*im_it);
				F->close_subchunk();
			F->close_chunk	();
		}
		// surfaces
		for (s_it=m_Surfaces.begin(); s_it!=m_Surfaces.end(); s_it++){
			CSurface* S=*s_it;
			int im_idx=FindLPCSTR(images,S->_Texture());
			R_ASSERT(im_idx>=0);
			LPCSTR vm_name=S->_VMap();
			F->Wsurface(S->_Name(),S->m_Flags.is(CSurface::sf2Sided),im_idx,(vm_name&&vm_name[0])?vm_name:"Texture",S->_ShaderName(),S->_ShaderXRLCName());
		}
		// meshes/layers
		for (EditMeshIt mesh_it=FirstMesh(); mesh_it!=LastMesh(); mesh_it++){
			CEditableMesh* MESH=*mesh_it;
			F->w_layer(mesh_it-FirstMesh(),MESH->GetName());
			// bounding box
			F->open_chunk(ID_BBOX);
				F->w_vector(MESH->m_Box.min);
				F->w_vector(MESH->m_Box.max);
			F->close_chunk();
			// points
			F->open_chunk(ID_PNTS);
				for (FvectorIt point_it=MESH->m_Points.begin(); point_it!=MESH->m_Points.end(); point_it++)
					F->w_vector(*point_it);
			F->close_chunk();
			// polygons
			F->open_chunk(ID_POLS);
				F->w_u32(ID_FACE);
				for (FaceIt f_it=MESH->m_Faces.begin(); f_it!=MESH->m_Faces.end(); f_it++)
					F->w_face3(f_it->pv[0].pindex,f_it->pv[1].pindex,f_it->pv[2].pindex);
			F->close_chunk();
			// surf<->face
			F->open_chunk(ID_PTAG);
				F->w_u32(ID_SURF);
				for (SurfFacesPairIt sf_it=MESH->m_SurfFaces.begin(); sf_it!=MESH->m_SurfFaces.end(); sf_it++){
					IntVec& lst			= sf_it->second;
					for (IntIt i_it=lst.begin(); i_it!=lst.end(); i_it++){
						F->w_vx	(*i_it);
						F->w_u16(WORD(sf_it->first->tag));
					}
				}
			F->close_chunk();
			// vmap&vmad
			for (VMapIt vm_it=MESH->m_VMaps.begin(); vm_it!=MESH->m_VMaps.end(); vm_it++){
				st_VMap* vmap = *vm_it;
				F->begin_vmap(vmap->polymap, (vmap->type==vmtUV)?ID_TXUV:ID_WGHT, vmap->dim, vmap->name.c_str());
					if (vmap->polymap)	for (int k=0; k<vmap->size(); k++) F->w_vmad(vmap->vindices[k],vmap->pindices[k],vmap->dim,vmap->getVMdata(k));
					else				for (int k=0; k<vmap->size(); k++) F->w_vmap(vmap->vindices[k],vmap->dim,vmap->getVMdata(k));
				F->end_vmap();
			}
		}
	F->end_save(fname);

	xr_delete(F);

	return true;
}