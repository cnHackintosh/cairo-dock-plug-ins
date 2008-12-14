
#ifndef __APPLET_WOBBLY__
#define  __APPLET_WOBBLY__


#include <cairo-dock.h>


void cd_animations_init_wobbly (CDAnimationData *pData);

gboolean cd_animations_update_wobbly (CDAnimationData *pData);

void cd_animations_draw_wobbly_icon (Icon *pIcon, CairoDock *pDock, CDAnimationData *pData);

#endif