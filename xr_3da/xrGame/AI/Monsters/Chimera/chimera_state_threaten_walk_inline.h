#pragma once

#define TEMPLATE_SPECIALIZATION template <\
	typename _Object\
>
#define CStateChimeraThreatenWalkAbstract CStateChimeraThreatenWalk<_Object>

TEMPLATE_SPECIALIZATION
void CStateChimeraThreatenWalkAbstract::initialize()
{
	inherited::initialize();

	object->SetUpperState	();

	data.point				= object->EnemyMan.get_enemy_position	();
	data.vertex				= object->EnemyMan.get_enemy_vertex		();

	data.action.action		= ACT_WALK_FWD;

	data.accelerated		= true;
	data.braking			= false;
	data.accel_type 		= eAT_Calm;

	data.completion_dist	= 2.f;
	data.action.sound_type	= MonsterSpace::eMonsterSoundIdle;
	data.action.sound_delay = object->get_sd()->m_dwIdleSndDelay;
	data.time_to_rebuild	= 1500;
}


TEMPLATE_SPECIALIZATION
void CStateChimeraThreatenWalkAbstract::execute()
{
	data.point				= object->EnemyMan.get_enemy_position	();
	data.vertex				= object->EnemyMan.get_enemy_vertex		();

	inherited::execute();

#ifdef DEBUG
	if (psAI_Flags.test(aiMonsterDebug)) {
		object->HDebug->M_Add(0,"Threaten :: Walk", D3DCOLOR_XRGB(255,0,0));
	}
#endif
}

#define DISTANCE_TO_ENEMY		5.f

TEMPLATE_SPECIALIZATION
bool CStateChimeraThreatenWalkAbstract::check_completion()
{	
	if (inherited::check_completion()) return true;

	float dist_to_enemy = object->EnemyMan.get_enemy_position().distance_to(object->Position());
	if (dist_to_enemy < DISTANCE_TO_ENEMY) return true;

	return false;
}

#define MAX_DISTANCE_TO_ENEMY	8.f

TEMPLATE_SPECIALIZATION
bool CStateChimeraThreatenWalkAbstract::check_start_conditions()
{
	float dist_to_enemy = object->EnemyMan.get_enemy_position().distance_to(object->Position());
	if (dist_to_enemy < MAX_DISTANCE_TO_ENEMY) return true;
	return false;
}

#undef TEMPLATE_SPECIALIZATION
#undef CStateChimeraThreatenWalkAbstract

