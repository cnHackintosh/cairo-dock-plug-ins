/**
* This file is a part of the Cairo-Dock project
*
* Copyright : (C) see the 'copyright' file.
* E-mail    : see the 'copyright' file.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 3
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <string.h>

#include "applet-struct.h"
#include "applet-read-data.h"
#include "applet-load-icons.h"

char *cMonthsWeeks[19] = { N_("Monday") , N_("Tuesday") , N_("Wednesday") , N_("Thursday") , N_("Friday") , N_("Saturday") , N_("Sunday") , N_("Jan") , N_("Feb") , N_("Mar") , N_("Apr") , N_("May") ,N_("Jun") , N_("Jui") , N_("Aug") , N_("Sep") , N_("Oct") , N_("Nov") , N_("Dec") };

#define _add_icon(i, j)\
	if (myData.days[i].cName != NULL)\
	{\
		pIcon = g_new0 (Icon, 1);\
		pIcon->cName = g_strdup_printf ("%s", myData.days[i].cName);\
		pIcon->cFileName = g_strdup_printf ("%s/%s.png", myConfig.cThemePath, myData.days[i].part[j].cIconNumber);\
		if (! g_file_test (pIcon->cFileName, G_FILE_TEST_EXISTS))\
		{\
			g_free (pIcon->cFileName);\
			pIcon->cFileName = g_strdup_printf ("%s/%s.svg", myConfig.cThemePath, myData.days[i].part[j].cIconNumber);\
		}\
		if (myConfig.bDisplayTemperature)\
			pIcon->cQuickInfo = g_strdup_printf ("%s/%s", _display (myData.days[i].cTempMin), _display (myData.days[i].cTempMax));\
		pIcon->fOrder = 2*i+j;\
		pIcon->fScale = 1.;\
		pIcon->fAlpha = 1.;\
		pIcon->fWidthFactor = 1.;\
		pIcon->fHeightFactor = 1.;\
		pIcon->cCommand = g_strdup ("none");\
		cd_debug (" + %s (%s , %s)", pIcon->cName, myData.days[i].part[j].cWeatherDescription, pIcon->cFileName);\
		pIconList = g_list_append (pIconList, pIcon);\
	}

static GList * _list_icons (CairoDockModuleInstance *myApplet)
{
	GList *pIconList = NULL;
	
	Icon *pIcon;
	int i;
	for (i = 0; i < myConfig.iNbDays; i ++)
	{
		_add_icon (i, 0);
		
		if (myConfig.bDisplayNights)
		{
			_add_icon (i, 1);
		}
	}
	
	return pIconList;
}


static void _weather_draw_current_conditions (CairoDockModuleInstance *myApplet)
{
	g_return_if_fail (myDrawContext != NULL);
	if (myConfig.bCurrentConditions || myData.bErrorRetrievingData)
	{
		cd_message ("  chargement de l'icone meteo (%x)", myApplet);
		if (myConfig.bDisplayTemperature && myData.currentConditions.cTemp != NULL)
		{
			CD_APPLET_SET_QUICK_INFO_ON_MY_ICON_PRINTF ("%s%s", myData.currentConditions.cTemp, myData.units.cTemp);
		}
		else
		{
			CD_APPLET_SET_QUICK_INFO_ON_MY_ICON (NULL);
		}
		
		g_free (myIcon->cFileName);
		if (myData.bErrorRetrievingData)
		{
			myIcon->cFileName = g_strdup_printf ("%s/na.png", myConfig.cThemePath);
			if (! g_file_test (myIcon->cFileName, G_FILE_TEST_EXISTS))
			{
				g_free (myIcon->cFileName);
				myIcon->cFileName = g_strdup_printf ("%s/na.svg", myConfig.cThemePath);
				if (! g_file_test (myIcon->cFileName, G_FILE_TEST_EXISTS))
				{
					g_free (myIcon->cFileName);
					myIcon->cFileName = g_strdup (MY_APPLET_SHARE_DATA_DIR"/broken.png");
				}
			}
		}
		else
		{
			myIcon->cFileName = g_strdup_printf ("%s/%s.png", myConfig.cThemePath, myData.currentConditions.cIconNumber);
			if (! g_file_test (myIcon->cFileName, G_FILE_TEST_EXISTS))
			{
				g_free (myIcon->cFileName);
				myIcon->cFileName = g_strdup_printf ("%s/%s.svg", myConfig.cThemePath, myData.currentConditions.cIconNumber);
			}
		}
		CD_APPLET_SET_IMAGE_ON_MY_ICON (myIcon->cFileName);
	}
	else
	{
		CD_APPLET_SET_DEFAULT_IMAGE_ON_MY_ICON_IF_NONE;
	}
}

gboolean cd_weather_update_from_data (CairoDockModuleInstance *myApplet)
{
	g_return_val_if_fail (myIcon != NULL, FALSE);  // paranoia
	CD_APPLET_ENTER;
	
	//\_______________________ On etablit le nom de l'icone.
	if ((myIcon->cName == NULL || myData.bSetName) && myDock)
	{
		myData.bSetName = (myData.cLocation == NULL);  // cas ou l'applet demarre avant l'etablissesment de la connexion.
		CD_APPLET_SET_NAME_FOR_MY_ICON (myData.cLocation != NULL ? myData.cLocation : WEATHER_DEFAULT_NAME);
	}
	
	//\_______________________ On cree la liste des icones de prevision.
	GList *pIconList = _list_icons (myApplet);  // ne nous appartiendra plus, donc ne pas desallouer.
	
	//\_______________________ On efface l'ancienne liste.
	CD_APPLET_DELETE_MY_ICONS_LIST;
	
	//\_______________________ On charge la nouvelle liste.
	gpointer pConfig[2] = {GINT_TO_POINTER (myConfig.bDesklet3D), GINT_TO_POINTER (FALSE)};
	CD_APPLET_LOAD_MY_ICONS_LIST (pIconList, myConfig.cRenderer, "Caroussel", pConfig);
	
	//\_______________________ On recharge l'icone principale.
	_weather_draw_current_conditions (myApplet);  // ne lance pas le redraw.
	CD_APPLET_REDRAW_MY_ICON;
	
	myData.bErrorRetrievingData = myData.bErrorInThread;
	if (myData.bErrorRetrievingData && myData.pTask->iPeriod > 20)
	{
		cd_message ("no data, will re-try in 20s");
		cairo_dock_change_task_frequency (myData.pTask, 20);  // on re-essaiera dans 20s.
	}
	else if (myData.pTask->iPeriod != myConfig.iCheckInterval)
	{
		cd_message ("revert to normal frequency");
		cairo_dock_change_task_frequency (myData.pTask, myConfig.iCheckInterval);
	}
	
	CD_APPLET_LEAVE (TRUE);
	//return TRUE;
}
