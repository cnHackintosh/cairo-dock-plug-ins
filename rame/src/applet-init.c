#include "stdlib.h"

#include "applet-config.h"
#include "applet-notifications.h"
#include "applet-struct.h"
#include "applet-init.h"
#include "applet-rame.h"

CD_APPLET_DEFINITION ("ram-meter", 1, 6, 2, CAIRO_DOCK_CATEGORY_ACCESSORY);

CD_APPLET_INIT_BEGIN
	if (myDesklet != NULL) {
		CD_APPLET_SET_DESKLET_RENDERER ("Simple");
	}
	
	//Initialisation de la jauge
	double fMaxScale = cairo_dock_get_max_scale (myContainer);
	fMaxScale = cairo_dock_get_max_scale (myContainer);
	myData.pGauge = cairo_dock_load_gauge (myDrawContext, myConfig.cGThemePath, myIcon->fWidth * fMaxScale, myIcon->fHeight * fMaxScale);
	if (myConfig.cFilligranImagePath != NULL)
		cairo_dock_add_filligran_on_gauge (myDrawContext, myData.pGauge, myConfig.cFilligranImagePath, myConfig.fAlpha);
	
	cairo_dock_render_gauge (myDrawContext, myContainer, myIcon, myData.pGauge, 0.);
	
	//Initialisation du timer de mesure.
	myData.pMeasureTimer = cairo_dock_new_measure_timer (myConfig.iCheckInterval,
		NULL,
		(CairoDockReadTimerFunc) cd_rame_read_data,
		(CairoDockUpdateTimerFunc) cd_rame_update_from_data,
		myApplet);
	myData.bAcquisitionOK = TRUE;
	cairo_dock_launch_measure (myData.pMeasureTimer);
	
	CD_APPLET_REGISTER_FOR_CLICK_EVENT
	CD_APPLET_REGISTER_FOR_BUILD_MENU_EVENT
CD_APPLET_INIT_END


CD_APPLET_STOP_BEGIN
	//\_______________ On se desabonne de nos notifications.
	CD_APPLET_UNREGISTER_FOR_CLICK_EVENT
	CD_APPLET_UNREGISTER_FOR_BUILD_MENU_EVENT
CD_APPLET_STOP_END


CD_APPLET_RELOAD_BEGIN
	//\_______________ On recharge les donnees qui ont pu changer.
	if (myDesklet != NULL) {
		CD_APPLET_SET_DESKLET_RENDERER ("Simple");
	}
	
	double fMaxScale = cairo_dock_get_max_scale (myContainer);
	
	if (CD_APPLET_MY_CONFIG_CHANGED) {
		cairo_dock_free_gauge(myData.pGauge);
		myData.pGauge = cairo_dock_load_gauge(myDrawContext,myConfig.cGThemePath,myIcon->fWidth * fMaxScale,myIcon->fHeight * fMaxScale);
		if (myConfig.cFilligranImagePath != NULL)
			cairo_dock_add_filligran_on_gauge (myDrawContext, myData.pGauge, myConfig.cFilligranImagePath, myConfig.fAlpha);
		
		if (myConfig.iInfoDisplay != CAIRO_DOCK_INFO_ON_ICON)
		{
			CD_APPLET_SET_QUICK_INFO_ON_MY_ICON_PRINTF (NULL)
		}
		if (myConfig.iInfoDisplay != CAIRO_DOCK_INFO_ON_LABEL)
		{
			CD_APPLET_SET_NAME_FOR_MY_ICON (myConfig.defaultTitle)
		}
		
		myData.fPrevRamPercent = 0;  // on force le redessin.
		cairo_dock_relaunch_measure_immediately (myData.pMeasureTimer, myConfig.iCheckInterval);
		
		if (cairo_dock_measure_is_active (myData.pTopMeasureTimer))
		{
			cd_rame_clean_all_processes ();
			cairo_dock_stop_measure_timer (myData.pTopMeasureTimer);
			g_free (myData.pTopList);
			myData.pTopList = NULL;
			g_free (myData.pPreviousTopList);
			myData.pPreviousTopList = NULL;
			cairo_dock_launch_measure (myData.pTopMeasureTimer);
		}
	}
	else {  // on redessine juste l'icone.
		cairo_dock_reload_gauge (myDrawContext, myData.pGauge, myIcon->fWidth * fMaxScale,myIcon->fHeight * fMaxScale);
		if (myConfig.cFilligranImagePath != NULL)
			cairo_dock_add_filligran_on_gauge (myDrawContext, myData.pGauge, myConfig.cFilligranImagePath, myConfig.fAlpha);
		
		CairoDockLabelDescription *pOldLabelDescription = myConfig.pTopTextDescription;  // on recupere le nouveau style des etiquettes en cas de changement de la config du dock.
		myConfig.pTopTextDescription = cairo_dock_duplicate_label_description (&g_dialogTextDescription);
		memcpy (myConfig.pTopTextDescription->fColorStart, pOldLabelDescription->fColorStart, 3*sizeof (double));
		memcpy (myConfig.pTopTextDescription->fColorStop, pOldLabelDescription->fColorStop, 3*sizeof (double));
		myConfig.pTopTextDescription->bVerticalPattern = TRUE;
		cairo_dock_free_label_description (pOldLabelDescription);
		
		myData.fPrevRamPercent = 0;  // on force le redessin.
		cd_rame_update_from_data (myApplet);
	}
CD_APPLET_RELOAD_END
