////////////////////////////////////////////////////////////////////////////
//	Module 		: space_restriction.cpp
//	Created 	: 17.08.2004
//  Modified 	: 27.08.2004
//	Author		: Dmitriy Iassenev
//	Description : Space restriction
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "space_restriction.h"
#include "space_restriction_manager.h"
#include "ai_space.h"
#include "level_graph.h"
#include "space_restriction_base.h"

const float dependent_distance = 100.f;

template <bool a>
struct CMergeInOutPredicate {
	SpaceRestrictionHolder::CBaseRestrictionPtr m_out;
	SpaceRestrictionHolder::CBaseRestrictionPtr m_in;

	IC			CMergeInOutPredicate(SpaceRestrictionHolder::CBaseRestrictionPtr out, SpaceRestrictionHolder::CBaseRestrictionPtr in)
	{
		m_out						= out;
		m_in						= in;
	}

	IC	bool	operator()			(u32 level_vertex_id) const
	{
		if (!m_out || !m_in)
			return					(false);
		return						(a ? m_in->inside(level_vertex_id,false) : !m_out->inside(level_vertex_id,true));
	}
};

struct CRemoveMergedFreeInRestrictions {
	CSpaceRestriction::RESTRICTIONS	*m_restrictions;

	IC			CRemoveMergedFreeInRestrictions	(CSpaceRestriction::RESTRICTIONS &restrictions)
	{
		m_restrictions				= &restrictions;
	}

	IC	bool	operator()						(const CSpaceRestriction::CFreeInRestriction &free_in_restriction) const
	{
		return						(std::find(m_restrictions->begin(),m_restrictions->end(), free_in_restriction.m_restriction) != m_restrictions->end());
	}
};

bool CSpaceRestriction::accessible				(const Fvector &position, float radius)
{
	if (!initialized()) {
		initialize					();
		if (!initialized())
			return					(true);
	}

#pragma todo("Dima to Dima : optimize this place in case of slowdown by adding containment routines for sphere and OBB with sphere")
//	float							shift_radius = ai().level_graph().header().cell_size()*_sqrt(2.f);
	return							(
		(
			m_out_space_restriction ? 
			(
				m_out_space_restriction->inside(position,radius) && 
				(
//					m_out_space_restriction->inside(position,radius + shift_radius) || 
					!m_out_space_restriction->on_border(position)
				)
			) :
			true
		)
		&&
		(
			m_in_space_restriction ? 
			(
				!m_in_space_restriction->inside(position,radius) && 
				(
//					!m_in_space_restriction->inside(position,radius + shift_radius) || 
					!m_in_space_restriction->on_border(position)
				)
			) :
			true
		)
	);
}

bool CSpaceRestriction::accessible				(u32 level_vertex_id, float radius)
{
	if (!initialized()) {
		initialize					();
		if (!initialized())
			return					(true);
	}

	return							(
		(
			m_out_space_restriction ? 
			m_out_space_restriction->inside(level_vertex_id,false,radius) :
			true
		)
		&&
		(
			m_in_space_restriction ? 
			!m_in_space_restriction->inside(level_vertex_id,true,radius) :
			true
		)
	);
}

IC	bool CSpaceRestriction::intersects			(SpaceRestrictionHolder::CBaseRestrictionPtr bridge0, SpaceRestrictionHolder::CBaseRestrictionPtr bridge1)
{
	xr_vector<u32>::const_iterator	I = bridge1->border().begin();
	xr_vector<u32>::const_iterator	E = bridge1->border().end();
	for ( ; I != E; ++I)
		if (bridge0->inside(*I,true))
			return					(true);
	
	if (!bridge0->border().empty() && bridge1->inside(bridge0->border().front(),true))
		return						(true);

	m_temp.resize					(bridge0->border().size() + bridge1->border().size());
	xr_vector<u32>::iterator		J = set_intersection(
		bridge0->border().begin(),
		bridge0->border().end(),
		bridge1->border().begin(),
		bridge1->border().end(),
		m_temp.begin()
	);
	return							(J != m_temp.begin());
}

IC	bool CSpaceRestriction::intersects			(SpaceRestrictionHolder::CBaseRestrictionPtr bridge)
{
	if (!m_out_space_restriction)
		return						(false);

	return							(intersects(m_out_space_restriction,bridge));
}

void CSpaceRestriction::merge_in_out_restrictions	()
{
	xr_vector<u32>					temp_border;
	xr_vector<u32>::iterator		I;

	m_border						= m_out_space_restriction->border();
	I								= remove_if(m_border.begin(),m_border.end(),CMergeInOutPredicate<true>(m_out_space_restriction,m_in_space_restriction));
	m_border.erase					(I,m_border.end());

	if (m_in_space_restriction) {
		temp_border					= m_in_space_restriction->border();
		I							= remove_if(temp_border.begin(),temp_border.end(),CMergeInOutPredicate<false>(m_out_space_restriction,m_in_space_restriction));
		temp_border.erase			(I,temp_border.end());
		m_border.insert				(m_border.end(),temp_border.begin(),temp_border.end());
	}
	
	std::sort						(m_border.begin(),m_border.end());
	I								= unique(m_border.begin(),m_border.end());
	m_border.erase					(I,m_border.end());
}

CSpaceRestriction::CBaseRestrictionPtr CSpaceRestriction::merge	(CBaseRestrictionPtr bridge, const RESTRICTIONS &temp_restrictions) const
{
	u32								acc_length = xr_strlen(*bridge->name()) + 1;
	{
		RESTRICTIONS::const_iterator	I = temp_restrictions.begin();
		RESTRICTIONS::const_iterator	E = temp_restrictions.end();
		for ( ; I != E; ++I)
			acc_length					+= xr_strlen(*(*I)->name()) + 1;
	}
	
	LPSTR							S = (LPSTR)xr_malloc(acc_length*sizeof(char));
	S[0]							= 0;
	shared_str						temp = bridge->name();
	RESTRICTIONS::const_iterator	I = temp_restrictions.begin();
	RESTRICTIONS::const_iterator	E = temp_restrictions.end();
	for ( ; I != E; ++I)
		temp						= strconcat(S,*temp,",",*(*I)->name());

	xr_free							(S);

	return							(m_space_restriction_manager->restriction(temp));
}

void CSpaceRestriction::merge_free_in_retrictions	()
{
	string256								temp;
	for (u32 i=0, n=_GetItemCount(*m_in_restrictions); i<n ;++i) {
		SpaceRestrictionHolder::CBaseRestrictionPtr bridge = m_space_restriction_manager->restriction(shared_str(_GetItem(*m_in_restrictions,i,temp)));
		m_free_in_restrictions.push_back	(CFreeInRestriction(bridge,false));
	}

	RESTRICTIONS					temp_restrictions;
	for (bool ok = false; !ok; ) {
		ok							= true;
		temp_restrictions.clear		();
		
		FREE_IN_RESTRICTIONS::iterator	I = m_free_in_restrictions.begin(), J;
		FREE_IN_RESTRICTIONS::iterator	E = m_free_in_restrictions.end();
		for ( ; I != E; ++I) {
			for (J = I + 1; J != E; ++J)
				if (intersects((*I).m_restriction,(*J).m_restriction))
					temp_restrictions.push_back	((*J).m_restriction);

			if (!temp_restrictions.empty()) {
				J					= remove_if(m_free_in_restrictions.begin(),m_free_in_restrictions.end(),CRemoveMergedFreeInRestrictions(temp_restrictions));
				m_free_in_restrictions.erase	(J,m_free_in_restrictions.end());
				(*I).m_restriction	= merge((*I).m_restriction,temp_restrictions);
				ok					= false;
				break;
			}
		}
	}
}

void CSpaceRestriction::initialize					()
{
	VERIFY							(!m_initialized);
	m_out_space_restriction			= m_space_restriction_manager->restriction(m_out_restrictions);
	m_in_space_restriction			= m_space_restriction_manager->restriction(m_in_restrictions);
	
	if (!m_out_space_restriction && !m_in_space_restriction) {
		m_initialized				= true;
		return;
	}

	if (m_out_space_restriction && !m_out_space_restriction->initialized())
		m_out_space_restriction->initialize();

#ifdef DEBUG
	if (m_out_space_restriction) {
		if (!m_out_space_restriction->object().correct()) {
			Msg						("! BAD out restrictions combination :");
			Msg						("! %s",*m_out_space_restriction->name());
		}
	}
#endif


	if (m_in_space_restriction && !m_in_space_restriction->initialized())
		m_in_space_restriction->initialize();

	if ((m_out_space_restriction && !m_out_space_restriction->initialized()) || (m_in_space_restriction && !m_in_space_restriction->initialized()))
		return;

	if (m_out_space_restriction)
		merge_in_out_restrictions	();
	else
		merge_free_in_retrictions	();

	m_initialized					= true;
}

void CSpaceRestriction::remove_border			()
{
	if (!initialized())
		return;
	
	VERIFY							(m_applied);
	
	m_applied						= false;
	
	if (m_out_space_restriction) {
		ai().level_graph().clear_mask	(border());
		return;
	}

	FREE_IN_RESTRICTIONS::iterator	I = m_free_in_restrictions.begin();
	FREE_IN_RESTRICTIONS::iterator	E = m_free_in_restrictions.end();
	for ( ; I != E; ++I)
		if ((*I).m_enabled) {
			VERIFY							((*I).m_restriction);
			(*I).m_enabled					= false;
			ai().level_graph().clear_mask	((*I).m_restriction->border());
		}
}

u32	CSpaceRestriction::accessible_nearest		(const Fvector &position, Fvector &result)
{
	if (m_out_space_restriction)
		return						(m_out_space_restriction->accessible_nearest(this,position,result,true));

	u32								vertex_result = u32(-1);
	FREE_IN_RESTRICTIONS::const_iterator	I = m_free_in_restrictions.begin();
	FREE_IN_RESTRICTIONS::const_iterator	E = m_free_in_restrictions.end();
	for ( ; I != E; ++I)
		if ((*I).m_restriction->inside(position) || (*I).m_restriction->on_border(position)) {
			vertex_result			= (*I).m_restriction->accessible_nearest(position,result,false);
			break;
		}

	VERIFY							(ai().level_graph().valid_vertex_id(vertex_result));
	result							= ai().level_graph().vertex_position(vertex_result);
	VERIFY							(accessible(result,EPS_L));
	return							(vertex_result);
}

bool CSpaceRestriction::affect					(SpaceRestrictionHolder::CBaseRestrictionPtr bridge, const Fvector &start_position, float radius) const
{
	if (bridge->inside(start_position))
		return						(false);
	Fvector							position;
	bridge->accessible_nearest		(start_position,position,false);
	return							(start_position.distance_to(position) <= radius + dependent_distance);
}

bool CSpaceRestriction::affect					(SpaceRestrictionHolder::CBaseRestrictionPtr bridge, u32 start_vertex_id, float radius) const
{
	return							(affect(bridge,ai().level_graph().vertex_position(start_vertex_id),radius));
}

bool CSpaceRestriction::affect					(SpaceRestrictionHolder::CBaseRestrictionPtr bridge, const Fvector &start_position, const Fvector &dest_position) const
{
	return							(affect(bridge,start_position,0.f) || affect(bridge,dest_position,0.f));
}

bool CSpaceRestriction::affect					(SpaceRestrictionHolder::CBaseRestrictionPtr bridge, u32 start_vertex_id, u32 dest_vertex_id) const
{
	return							(affect(bridge,start_vertex_id,0.f) || affect(bridge,dest_vertex_id,0.f));
}