#include "stdafx.h"
#include "map_location.h"
#include "level.h"
#include "ai_space.h"
#include "game_graph.h"
#include "xrServer.h"
#include "xrServer_Objects_ALife.h"

//////////////////////////////////////////////////////////////////////////

SMapLocation::SMapLocation():animation(&icon_color)
{
	info_portion_id				= NO_INFO_INDEX;
	level_name					= NULL;
	x							= y = 0;
	name						= NULL;
	text						= NULL;
	attached_to_object			= false;
	object_id					= 0xffff;
	marker_show					= false;
	big_icon					= false;
	global_map_text				= false;
	icon_color					= 0xffffffff;
	dynamic_manifestation		= false;
	type_flags.zero				();
	animation.Cyclic			(false);
	m_iconsShader				= NULL;
}

SMapLocation::SMapLocation(const SMapLocation& map_location)
{
	*this  = map_location;
	animation.SetColorToModify(&icon_color);
}

//////////////////////////////////////////////////////////////////////////

shared_str SMapLocation::LevelName()
{
	if(!attached_to_object)
		return level_name;
	else
	{
		if(ai().get_alife() && ai().get_game_graph())		
		{
			CSE_Abstract* E = Level().Server->game->get_entity_from_eid(object_id); //VERIFY(E);
			if(!E){
				Msg("! Critical: SMapLocation binded to non-existent object id=%d",object_id);
				type_flags.zero				();
				return level_name;
			}
			
			CSE_ALifeObject* AO = smart_cast<CSE_ALifeObject*>(E);
			if(AO)	
				return  ai().game_graph().header().level(ai().game_graph().vertex(AO->m_tGraphID)->level_id()).name();
			else	
				return Level().name();
		}
		else
			return Level().name();
	}
}

//////////////////////////////////////////////////////////////////////////

void SMapLocation::UpdateAnimation()
{
	animation.Update();
}

//////////////////////////////////////////////////////////////////////////

void SMapLocation::SetColorAnimation(const shared_str &animationName)
{
	animation.SetColorAnimation(animationName);
	animation.Reset();
}

//////////////////////////////////////////////////////////////////////////

ref_shader&	SMapLocation::GetShader(){
	if (xr_strlen(shader_name) > 0)
        if(!m_iconsShader)
		{
			m_iconsShader.create("hud\\default", *shader_name);
		}

	return m_iconsShader;
}