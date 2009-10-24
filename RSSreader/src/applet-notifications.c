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

//\________________ Add your name in the copyright file (and / or modify your name here)

#include <stdlib.h>
#include <string.h>

#include "applet-struct.h"
#include "applet-notifications.h"
#include "applet-draw.h"



static void _start_browser (GtkMenuItem *menu_item, CairoDockModuleInstance *myApplet)
{		
	GString *sCommand = g_string_new ("");		
	g_string_printf (sCommand, "%s %s", myConfig.cSpecificWebBrowser, myConfig.cUrl);	
	cd_debug ("RSSreader-debug : START_BROWSER---------------------->  sCommand = \"%s\"",sCommand->str);
	g_spawn_command_line_async (sCommand->str, NULL);
	g_string_free (sCommand, TRUE);
}

void _new_url_to_conf (CairoDockModuleInstance *myApplet, gchar *cNewURL)
{
	if (g_strstr_len (cNewURL, -1, "http") != NULL)  // On vérifie que l'élément glisser/copier commence bien par http
	{
		cd_debug ("RSSreader-debug : This seems to be a valid URL -> Let's continue...");
		myConfig.cUrl = g_strdup_printf ("%s", cNewURL);  // On modifie la variable de l'URL
		cairo_dock_update_conf_file (CD_APPLET_MY_CONF_FILE,G_TYPE_STRING, "Configuration", "url_rss_feed", myConfig.cUrl,G_TYPE_INVALID);  // On l'écrit dans le fichier de config
		cd_rssreader_upload_title_TASK (myApplet);  // On cherche un titre (en général = la ligne 0 du flux) 
		cd_rssreader_upload_feeds_TASK (myApplet); // On lance l'upload pour mettre à jour notre applet
		cd_applet_update_my_icon (myApplet, myIcon, myContainer); 
	}
	else
	{
		cd_debug ("RSSreader-debug : It doesn't seem to be a valid URL.");	
		cairo_dock_remove_dialog_if_any (myIcon);
		cairo_dock_show_temporary_dialog_with_icon (D_("It doesn't seem to be a valid URL."),
			myIcon,
			myContainer,
			2000, // Suffisant 
			MY_APPLET_SHARE_DATA_DIR"/"MY_APPLET_ICON_FILE);		
	}
}

void _paste_new_url_to_conf (GtkMenuItem *menu_item, CairoDockModuleInstance *myApplet)
{
	GtkClipboard *pClipBoardSelection = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);	
	gchar *cEntry = gtk_clipboard_wait_for_text (pClipBoardSelection);
	cd_debug ("RSSreader-debug : Paste from clipboard -> \"%s\"", cEntry);	
	_new_url_to_conf (myApplet, cEntry);
}

//\___________ Define here the action to be taken when the user left-clicks on your icon or on its subdock or your desklet. The icon and the container that were clicked are available through the macros CD_APPLET_CLICKED_ICON and CD_APPLET_CLICKED_CONTAINER. CD_APPLET_CLICKED_ICON may be NULL if the user clicked in the container but out of icons.
CD_APPLET_ON_CLICK_BEGIN
	cd_debug ("RSSreader-debug : CLIC");
	
	if (myDesklet)
			cd_applet_update_my_icon (myApplet, myIcon, myContainer); // On en profite pour faire un petit refresh ... utile dans certains cas ;)
		
	if (! myDesklet || myConfig.bLeftClicForDesklet)
	{
		cairo_dock_remove_dialog_if_any (myIcon);
		cairo_dock_show_temporary_dialog_with_icon (myData.cFeedLine[0],
			myIcon,
			myContainer,
			myConfig.iDialogsDuration,
			MY_APPLET_SHARE_DATA_DIR"/"MY_APPLET_ICON_FILE);
	}
CD_APPLET_ON_CLICK_END



CD_APPLET_ON_MIDDLE_CLICK_BEGIN
	cd_debug ("RSSreader-debug : MIDDLE-CLIC");
	myData.bUpdateIsManual = TRUE;	
	cd_rssreader_upload_feeds_TASK (myApplet);	
CD_APPLET_ON_MIDDLE_CLICK_END



CD_APPLET_ON_DROP_DATA_BEGIN
	cd_debug ("RSSreader-debug : \"%s\" was dropped", CD_APPLET_RECEIVED_DATA);	
	_new_url_to_conf (myApplet, (char *)CD_APPLET_RECEIVED_DATA);
CD_APPLET_ON_DROP_DATA_END



//\___________ Define here the entries you want to add to the menu when the user right-clicks on your icon or on its subdock or your desklet. The icon and the container that were clicked are available through the macros CD_APPLET_CLICKED_ICON and CD_APPLET_CLICKED_CONTAINER. CD_APPLET_CLICKED_ICON may be NULL if the user clicked in the container but out of icons. The menu where you can add your entries is available throught the macro CD_APPLET_MY_MENU; you can add sub-menu to it if you want.
CD_APPLET_ON_BUILD_MENU_BEGIN
	GtkWidget *pSubMenu = CD_APPLET_CREATE_MY_SUB_MENU ();
		CD_APPLET_ADD_ABOUT_IN_MENU (pSubMenu);
		
		
	CD_APPLET_ADD_IN_MENU_WITH_STOCK (D_("Paste a new RSS Url (drag'n'drop)"), GTK_STOCK_PASTE, _paste_new_url_to_conf, CD_APPLET_MY_MENU);	
	
	
	if (myConfig.cUrl != NULL) // On ajoute une entrée dans le menu SI il y a une url seulement
		CD_APPLET_ADD_IN_MENU_WITH_STOCK (D_("Open with your web browser"), GTK_STOCK_EXECUTE, _start_browser, CD_APPLET_MY_MENU);	
	
CD_APPLET_ON_BUILD_MENU_END
