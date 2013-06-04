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

/******************************************************************************
exemples : 
----------

dbus-send --session --dest=org.cairodock.CairoDock /org/cairodock/CairoDock org.cairodock.CairoDock.SetQuickInfo string:123 string:"class=firefox"

dbus-send --session --dest=org.cairodock.CairoDock /org/cairodock/CairoDock org.cairodock.CairoDock.Animate string:default int32:2 string:"class=firefox"

******************************************************************************/

#include <unistd.h>
#include <glib.h>

#include <cairo-dock.h>
#include <cairo-dock-module-instance-manager.h>
#include "interface-main-query.h"
#include "interface-main-methods.h"


#define nullify_argument(string) do {\
	if (string != NULL && (*string == '\0' || strcmp (string, "any") == 0 || strcmp (string, "none") == 0))\
		string = NULL; } while (0)

static gboolean dbus_deskletVisible = FALSE;

gboolean cd_dbus_main_reboot(dbusMainObject *pDbusCallback, GError **error)
{
	if (! myConfig.bEnableReboot)
		return FALSE;
	cairo_dock_load_current_theme ();
	return TRUE;
}

gboolean cd_dbus_main_quit (dbusMainObject *pDbusCallback, GError **error)
{
	if (! myConfig.bEnableQuit)
		return FALSE;
	gtk_main_quit ();
	return TRUE;
}

static void _show_hide_one_dock (const gchar *cDockName, CairoDock *pDock, gpointer data)
{
	if (pDock->iRefCount != 0)
		return ;
	gboolean bShow = GPOINTER_TO_INT (data);
	if (bShow)
	{
		///cairo_dock_pop_up (pDock);
		///if (pDock->bAutoHide)
			cairo_dock_emit_enter_signal (CAIRO_CONTAINER (pDock));
	}
	else
	{
		///cairo_dock_pop_down (pDock);  // ne fait rien s'il n'etait pas "popped".
		///if (pDock->bAutoHide)
			cairo_dock_emit_leave_signal (CAIRO_CONTAINER (pDock));
	}
}
gboolean cd_dbus_main_show_dock (dbusMainObject *pDbusCallback, gint iVisibiliy, GError **error)
{
	if (! myConfig.bEnableShowDock)
		return FALSE;
	
	if (g_pMainDock == NULL)
		return FALSE;
	
	gboolean bShow;
	switch (iVisibiliy)
	{
		case 0:  // hide
			bShow = FALSE;
		break;
		case 1:  // show
			bShow = TRUE;
		break;
		case 2:  // toggle
		default:
			bShow = (g_pMainDock->bIsBelow || (g_pMainDock->bAutoHide && g_pMainDock->fHideOffset == 1));
		break;		
	}
	
	if (bShow)
		cairo_dock_stop_quick_hide ();
	
	gldi_docks_foreach ((GHFunc) _show_hide_one_dock, GINT_TO_POINTER (bShow));
	
	if (! bShow)
		cairo_dock_quick_hide_all_docks ();
	
	return TRUE;
}

gboolean cd_dbus_main_show_desklet (dbusMainObject *pDbusCallback, gboolean *widgetLayer, GError **error)
{
	if (! myConfig.bEnableDesklets)
		return FALSE;
	if (dbus_deskletVisible)
	{
		gldi_desklets_set_visibility_to_default ();
	}
	else
	{
		gldi_desklets_set_visible (widgetLayer != NULL ? *widgetLayer : FALSE);
	}
	dbus_deskletVisible = !dbus_deskletVisible;
	return TRUE;
}


gboolean cd_dbus_main_reload_module (dbusMainObject *pDbusCallback, const gchar *cModuleName, GError **error)
{
	if (! myConfig.bEnableReloadModule)
		return FALSE;
	GldiModule *pModule = gldi_module_get (cModuleName);
	if (pModule != NULL)
	{
		gldi_module_reload (pModule, TRUE);  // TRUE <=> reload module conf file.
	}
	else
	{
		GldiManager *pManager = gldi_get_manager (cModuleName);
		if (pManager != NULL)
		{
			gldi_reload_manager (pManager, g_cConfFile);
		}
		else
		{
			cd_warning ("no module named '%s'", cModuleName);
			return FALSE;
		}
	}
	return TRUE;
}

gboolean cd_dbus_main_activate_module (dbusMainObject *pDbusCallback, const gchar *cModuleName, gboolean bActivate, GError **error)
{
	if (! myConfig.bEnableActivateModule)
		return FALSE;
	
	GldiModule *pModule = gldi_module_get (cModuleName);
	if (pModule == NULL)
	{
		if (gldi_get_manager (cModuleName) != NULL)
			cd_warning ("Internal modules can't be (de)activated.");
		else
			cd_warning ("no such module (%s)", cModuleName);
		return FALSE;
	}
	
	if (bActivate)
		gldi_module_activate (pModule);
	else
		gldi_module_deactivate (pModule);
	return TRUE;
}


  //////////////////
 /// PROPERTIES ///
//////////////////

gboolean cd_dbus_main_get_icon_properties (dbusMainObject *pDbusCallback, gchar *cIconQuery, GPtrArray **pIconAttributes, GError **error)
{
	GList *pList = cd_dbus_find_matching_icons (cIconQuery);  // if NULL, will just return an empty array.
	
	GPtrArray *pTab = g_ptr_array_new ();
	*pIconAttributes = pTab;
	
	GHashTable *h;
	GValue *v;
	Icon *pIcon;
	GldiContainer *pContainer;
	int iPosition;
	const gchar *cType;
	const gchar *cContainerName;
	const gchar *cDesktopFile;
	GList *ic;
	for (ic = pList; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		pContainer = cairo_dock_get_icon_container (pIcon);
		
		h = g_hash_table_new_full (g_str_hash,
			g_str_equal,
			g_free,  /// can we use const char here instead of duplicating each string ?...
			g_free);
		g_ptr_array_add (pTab, h);
		
		if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pIcon))
			cType = CD_TYPE_LAUNCHER;
		else if (CAIRO_DOCK_ICON_TYPE_IS_APPLI (pIcon))
			cType = CD_TYPE_APPLICATION;
		else if (CAIRO_DOCK_ICON_TYPE_IS_APPLET (pIcon))
			cType = CD_TYPE_APPLET;
		else if (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pIcon))
			cType = CD_TYPE_SEPARATOR;
		else if (CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pIcon))
			cType = CD_TYPE_ICON_CONTAINER;
		else if (CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (pIcon))
			cType = CD_TYPE_CLASS_CONTAINER;
		else
			cType = CD_TYPE_ICON_OTHER;
		v = g_new0 (GValue, 1);
		g_value_init (v, G_TYPE_STRING);
		g_value_set_string (v, cType);
		g_hash_table_insert (h, g_strdup ("type"), v);
		
		cDesktopFile = NULL;
		if (pIcon->cDesktopFileName != NULL)
			cDesktopFile = pIcon->cDesktopFileName;
		else if (CAIRO_DOCK_IS_APPLET (pIcon))
			cDesktopFile = pIcon->pModuleInstance->cConfFilePath;
		v = g_new0 (GValue, 1);
		g_value_init (v, G_TYPE_STRING);
		g_value_set_string (v, cDesktopFile);
		g_hash_table_insert (h, g_strdup ("config-file"), v);
		
		if (CAIRO_DOCK_IS_APPLET (pIcon))
		{
			v = g_new0 (GValue, 1);
			g_value_init (v, G_TYPE_STRING);
			g_value_set_string (v, pIcon->pModuleInstance->pModule->pVisitCard->cModuleName);
			g_hash_table_insert (h, g_strdup ("module"), v);
		}
		
		v = g_new0 (GValue, 1);
		g_value_init (v, G_TYPE_STRING);
		g_value_set_string (v, pIcon->cName);  /// g_value_set_static_string ?...
		g_hash_table_insert (h, g_strdup ("name"), v);
		
		v = g_new0 (GValue, 1);
		g_value_init (v, G_TYPE_STRING);
		g_value_set_string (v, pIcon->cCommand);
		g_hash_table_insert (h, g_strdup ("command"), v);
		
		v = g_new0 (GValue, 1);
		g_value_init (v, G_TYPE_STRING);
		g_value_set_string (v, pIcon->cClass);
		g_hash_table_insert (h, g_strdup ("class"), v);
		
		v = g_new0 (GValue, 1);
		g_value_init (v, G_TYPE_STRING);
		g_value_set_string (v, pIcon->cFileName);
		g_hash_table_insert (h, g_strdup ("icon"), v);
		
		v = g_new0 (GValue, 1);
		g_value_init (v, G_TYPE_STRING);
		g_value_set_string (v, pIcon->cQuickInfo);
		g_hash_table_insert (h, g_strdup ("quick-info"), v);
		
		v = g_new0 (GValue, 1);
		g_value_init (v, G_TYPE_UINT);
		g_value_set_uint (v, GPOINTER_TO_INT(pIcon->pAppli));  // not sure what we can do with it
		g_hash_table_insert (h, g_strdup ("Xid"), v);
		
		iPosition = -1;
		cContainerName = NULL;
		if (CAIRO_DOCK_IS_DOCK (pContainer))
		{
			CairoDock *pDock = CAIRO_DOCK (pContainer);
			iPosition = g_list_index (pDock->icons, pIcon);
			cContainerName = pIcon->cParentDockName;
		}
		else if (CAIRO_DOCK_IS_DESKLET (pContainer))
		{
			CairoDesklet *pDesklet = CAIRO_DESKLET (pContainer);
			if (pDesklet->pIcon == pIcon)
				iPosition = 0;
			else
				iPosition = g_list_index (pDesklet->icons, pIcon);
			if (CAIRO_DOCK_IS_APPLET (pDesklet->pIcon))
				cContainerName = pDesklet->pIcon->pModuleInstance->pModule->pVisitCard->cModuleName;
		}
		v = g_new0 (GValue, 1);
		g_value_init (v, G_TYPE_INT);
		g_value_set_int (v, iPosition);
		g_hash_table_insert (h, g_strdup ("position"), v);
		
		v = g_new0 (GValue, 1);
		g_value_init (v, G_TYPE_STRING);
		g_value_set_string (v, cContainerName);
		g_hash_table_insert (h, g_strdup ("container"), v);
		
		v = g_new0 (GValue, 1);
		g_value_init (v, G_TYPE_DOUBLE);
		g_value_set_double (v, pIcon->fOrder);
		g_hash_table_insert (h, g_strdup ("order"), v);
	}
	
	g_list_free (pList);
	return TRUE;
}

static void _set_container_props (GldiContainer *pContainer, GHashTable *h)
{
	GValue *v;
	int x, y, w, ht;
	if (pContainer->bIsHorizontal)
	{
		x = pContainer->iWindowPositionX;
		y = pContainer->iWindowPositionY;
		w = pContainer->iWidth;
		ht = pContainer->iHeight;
	}
	else
	{
		y = pContainer->iWindowPositionX;
		x = pContainer->iWindowPositionY;
		ht = pContainer->iWidth;
		w = pContainer->iHeight;
	}
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_INT);
	g_value_set_int (v, x);
	g_hash_table_insert (h, g_strdup ("x"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_INT);
	g_value_set_int (v, y);
	g_hash_table_insert (h, g_strdup ("y"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_INT);
	g_value_set_int (v, w);
	g_hash_table_insert (h, g_strdup ("width"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_INT);
	g_value_set_int (v, ht);
	g_hash_table_insert (h, g_strdup ("height"), v);
	
	CairoDockPositionType iScreenBorder = ((! pContainer->bIsHorizontal) << 1) | (! pContainer->bDirectionUp);
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_UINT);
	g_value_set_uint (v, iScreenBorder);
	g_hash_table_insert (h, g_strdup ("orientation"), v);
}
static void _insert_dock_props (const gchar *cDockName, CairoDock *pDock, GPtrArray *pTab)
{
	GHashTable *h = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		g_free,  /// can we use const char here instead of duplicating each string ?...
		g_free);
	g_ptr_array_add (pTab, h);
	
	GValue *v;
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, "Dock");
	g_hash_table_insert (h, g_strdup ("type"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, cDockName);
	g_hash_table_insert (h, g_strdup ("name"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_BOOLEAN);
	g_value_set_boolean (v, (pDock->iRefCount > 0));
	g_hash_table_insert (h, g_strdup ("is-sub-dock"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_INT);
	g_value_set_int (v, g_list_length (pDock->icons));
	g_hash_table_insert (h, g_strdup ("nb-icons"), v);
	
	_set_container_props (CAIRO_CONTAINER (pDock), h);
}
static gboolean _insert_desklet_props (CairoDesklet *pDesklet, GPtrArray *pTab)
{
	GHashTable *h = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		g_free,  /// can we use const char here instead of duplicating each string ?...
		g_free);
	g_ptr_array_add (pTab, h);
	
	GValue *v;
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, "Desklet");
	g_hash_table_insert (h, g_strdup ("type"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, CAIRO_DOCK_IS_APPLET (pDesklet->pIcon) ? pDesklet->pIcon->pModuleInstance->pModule->pVisitCard->cModuleName : "");
	g_hash_table_insert (h, g_strdup ("name"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_INT);
	g_value_set_int (v, 1 + g_list_length (pDesklet->icons));
	g_hash_table_insert (h, g_strdup ("nb-icons"), v);
	
	_set_container_props (CAIRO_CONTAINER (pDesklet), h);
	return FALSE;
}
static gboolean _check_desklet_name (CairoDesklet *pDesklet, const gchar *cName)
{
	if (CAIRO_DOCK_IS_APPLET (pDesklet->pIcon))
	{
		return (strcmp (cName, pDesklet->pIcon->pModuleInstance->pModule->pVisitCard->cModuleName) == 0);
	}
	return FALSE;
}
gboolean cd_dbus_main_get_container_properties (dbusMainObject *pDbusCallback, const gchar *cName, GPtrArray **pAttributes, GError **error)
{
	nullify_argument (cName);
	
	GPtrArray *pTab = g_ptr_array_new ();
	*pAttributes = pTab;
	
	if (cName == NULL)
	{
		gldi_docks_foreach ((GHFunc)_insert_dock_props, pTab);
		gldi_desklets_foreach ((GldiDeskletForeachFunc) _insert_desklet_props, pTab);
	}
	else
	{
		CairoDock *pDock = gldi_dock_get (cName);
		if (pDock != NULL)
		{
			_insert_dock_props (cName, pDock, pTab);
		}
		else
		{
			CairoDesklet *pDesklet = gldi_desklets_foreach ((GldiDeskletForeachFunc) _check_desklet_name, (gpointer)cName);
			if (pDesklet != NULL)
			{
				_insert_desklet_props (pDesklet, pTab);
			}
		}
	}
	
	return TRUE;
}

static gboolean _insert_module_props (GldiModule *pModule, GPtrArray *pTab)
{
	GHashTable *h = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		g_free,  /// can we use const char here instead of duplicating each string ?...
		g_free);
	g_ptr_array_add (pTab, h);
	
	GValue *v;
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, pModule->pVisitCard->cModuleName);
	g_hash_table_insert (h, g_strdup ("name"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_UINT);
	g_value_set_uint (v, pModule->pVisitCard->iContainerType);
	g_hash_table_insert (h, g_strdup ("type"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_UINT);
	g_value_set_uint (v, pModule->pVisitCard->iCategory);
	g_hash_table_insert (h, g_strdup ("category"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, pModule->pVisitCard->cTitle);
	g_hash_table_insert (h, g_strdup ("title"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, pModule->pVisitCard->cIconFilePath);
	g_hash_table_insert (h, g_strdup ("icon"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, pModule->pVisitCard->cPreviewFilePath);
	g_hash_table_insert (h, g_strdup ("preview"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, dgettext (pModule->pVisitCard->cGettextDomain, pModule->pVisitCard->cDescription));
	g_hash_table_insert (h, g_strdup ("description"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, pModule->pVisitCard->cAuthor);
	g_hash_table_insert (h, g_strdup ("author"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_BOOLEAN);
	g_value_set_boolean (v, pModule->pVisitCard->bMultiInstance);
	g_hash_table_insert (h, g_strdup ("is-multi-instance"), v);
	
	cd_debug ("list instances ...");
	gchar **pInstances = g_new0 (gchar*, g_list_length (pModule->pInstancesList)+1);
	GldiModuleInstance *pInstance;
	int i = 0;
	GList *mi;
	for (mi = pModule->pInstancesList; mi != NULL; mi = mi->next)
	{
		pInstance = mi->data;
		pInstances[i++] = g_strdup (pInstance->cConfFilePath);
	}
	cd_debug ("write instances ...");
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRV);
	g_value_set_boxed (v, pInstances);
	g_hash_table_insert (h, g_strdup ("instances"), v);
	cd_debug ("done.");
	return TRUE;  // continue
}
gboolean cd_dbus_main_get_module_properties (dbusMainObject *pDbusCallback, const gchar *cName, GPtrArray **pAttributes, GError **error)
{
	nullify_argument (cName);
	
	GPtrArray *pTab = g_ptr_array_new ();
	*pAttributes = pTab;
	
	if (cName == NULL)
	{
		gldi_module_foreach_in_alphabetical_order ((GCompareFunc) _insert_module_props, pTab);
	}
	else
	{
		GldiModule *pModule = gldi_module_get (cName);
		if (pModule != NULL)
		{
			_insert_module_props (pModule, pTab);
		}
	}
	return TRUE;
}

gboolean cd_dbus_main_add_launcher (dbusMainObject *pDbusCallback, const gchar *cDesktopFilePath, gdouble fOrder, const gchar *cDockName, gchar **cLauncherFile, GError **error)
{
	*cLauncherFile = NULL;
	if (! myConfig.bEnableCreateLauncher)
		return FALSE;
	nullify_argument (cDesktopFilePath);
	g_return_val_if_fail (cDesktopFilePath != NULL, FALSE);
	
	//\_______________ get the dock where to insert the icon.
	nullify_argument (cDockName);
	if (cDockName == NULL)
		cDockName = CAIRO_DOCK_MAIN_DOCK_NAME;
	
	CairoDock * pParentDock = gldi_dock_get (cDockName);
	if (pParentDock == NULL)
	{
		cd_warning ("dock %s does not exist", cDockName);
		pParentDock = g_pMainDock;
	}
	
	//\_______________ add a new icon in the current theme.
	if (fOrder < 0)
		fOrder = CAIRO_DOCK_LAST_ORDER;
	Icon *pNewIcon;
	if (strcmp (cDesktopFilePath, "separator.desktop") == 0)
		pNewIcon = gldi_separator_icon_add_new (pParentDock, fOrder);
	else if (strcmp (cDesktopFilePath, "container.desktop") == 0)
		pNewIcon = gldi_stack_icon_add_new (pParentDock, fOrder);
	else if (strcmp (cDesktopFilePath, "launcher.desktop") == 0)
		pNewIcon = gldi_launcher_add_new (NULL, pParentDock, fOrder);
	else
		pNewIcon = gldi_launcher_add_new (cDesktopFilePath, pParentDock, fOrder);
	if (pNewIcon != NULL)
	{
		*cLauncherFile = g_strdup (pNewIcon->cDesktopFileName);
		return TRUE;
	}
	else
		return FALSE;
}

gboolean cd_dbus_main_add_temporary_icon (dbusMainObject *pDbusCallback, GHashTable *hIconAttributes, GError **error)
{
	if (! myConfig.bEnableCreateLauncher)
		return FALSE;
	
	g_return_val_if_fail (hIconAttributes != NULL, FALSE);
	
	//\_______________ get the attributes.
	GValue *v;
	const gchar *cType = "Launcher";
	v = g_hash_table_lookup (hIconAttributes, "type");
	if (v && G_VALUE_HOLDS_STRING (v))
	{
		cType = g_value_get_string (v);
	}
	
	const gchar *cIcon = NULL;
	v = g_hash_table_lookup (hIconAttributes, "icon");
	if (v && G_VALUE_HOLDS_STRING (v))
	{
		cIcon = g_value_get_string (v);
	}
	
	const gchar *cName = NULL;
	v = g_hash_table_lookup (hIconAttributes, "name");
	if (!v)
		v = g_hash_table_lookup (hIconAttributes, "label");
	if (v && G_VALUE_HOLDS_STRING (v))
	{
		cName = g_value_get_string (v);
	}
	
	const gchar *cParentDockName = CAIRO_DOCK_MAIN_DOCK_NAME;
	v = g_hash_table_lookup (hIconAttributes, "container");
	if (v && G_VALUE_HOLDS_STRING (v))
	{
		cParentDockName = g_value_get_string (v);
	}
	
	const gchar *cQuickInfo = NULL;
	v = g_hash_table_lookup (hIconAttributes, "quick-info");
	if (v && G_VALUE_HOLDS_STRING (v))
	{
		cQuickInfo = g_value_get_string (v);
	}
	
	double fOrder = CAIRO_DOCK_LAST_ORDER;
	v = g_hash_table_lookup (hIconAttributes, "order");
	if (v && G_VALUE_HOLDS_DOUBLE (v))
	{
		fOrder = g_value_get_double (v);
	}
	
	int iPosition = -1;
	v = g_hash_table_lookup (hIconAttributes, "position");
	if (v && G_VALUE_HOLDS_INT (v))
	{
		iPosition = g_value_get_int (v);
	}
	
	const gchar *cCommand = NULL;
	v = g_hash_table_lookup (hIconAttributes, "command");
	if (v && G_VALUE_HOLDS_STRING (v))
	{
		cCommand = g_value_get_string (v);
	}
	
	const gchar *cClass = NULL;
	v = g_hash_table_lookup (hIconAttributes, "class");
	if (v && G_VALUE_HOLDS_STRING (v))
	{
		cClass = g_value_get_string (v);  // "none" to skip taskbar
	}
	
	CairoDock *pParentDock = gldi_dock_get (cParentDockName);
	if (pParentDock == NULL)
	{
		cd_warning ("dock %s does not exist", cParentDockName);
		pParentDock = g_pMainDock;
	}
	
	if (iPosition > -1)  // the position will overwrite the order if ever both are defined.
	{
		fOrder = 0;
		Icon *nth_icon;
		double nth_order = -1;
		int i;
		GList *ic;
		for (i = 0, ic = pParentDock->icons; i < iPosition && ic != NULL; i ++, ic = ic->next)
		{
			nth_icon = ic->data;
			nth_order = nth_icon->fOrder;
		}
		if (ic == NULL)  // not enough icons, place the new one just after the last one.
		{
			fOrder = nth_order + 1;
		}
		else
		{
			nth_icon = ic->data;
			fOrder = (nth_order + nth_icon->fOrder) / 2;
		}
	}
	
	//\_______________ create a corresponding icon.
	Icon *pIcon;
	if (cType == NULL || strcmp (cType, "Launcher") == 0)
	{
		pIcon = cairo_dock_create_dummy_launcher (g_strdup (cName),
			g_strdup (cIcon),
			g_strdup (cCommand),
			g_strdup (cQuickInfo),
			fOrder);
		pIcon->iTrueType = CAIRO_DOCK_ICON_TYPE_LAUNCHER;  // make it a launcher, since we have no control on it, so that the dock handles it as any launcher.
		if (cClass == NULL)
		{
			gchar *cGuessedClass = cairo_dock_guess_class (cCommand, NULL);
			pIcon->cClass = cairo_dock_register_class (cGuessedClass);
			g_free (cGuessedClass);
		}
		else if (strcmp (cClass, "none") != 0)
		{
			pIcon->cClass = cairo_dock_register_class (cClass);
			if (pIcon->cClass == NULL)  // if we couldn't find the class desktop file, set the class anyway, since it was explicitely specified; the method caller probably knows more than us what he's doing.
				pIcon->cClass = g_strdup (cClass);
		}
	}
	else if (strcmp (cType, "Container") == 0)
	{
		int iSubdockViewType = 0;
		if (!cIcon || strcmp (cIcon, "Box") == 0)
		{
			iSubdockViewType = 3;
			cIcon = NULL;
		}
		else if (strcmp (cIcon, "Stack") == 0)
		{
			iSubdockViewType = 2;
			cIcon = NULL;
		}
		else if (strcmp (cIcon, "Emblems") == 0)
		{
			iSubdockViewType = 1;
			cIcon = NULL;
		}
		gchar *cUniqueName = cairo_dock_get_unique_dock_name (cName);
		pIcon = cairo_dock_create_dummy_launcher (cUniqueName,
			g_strdup (cIcon),
			NULL,
			NULL,
			fOrder);
		pIcon->iTrueType = CAIRO_DOCK_ICON_TYPE_CONTAINER;
		cairo_dock_set_subdock_content_renderer (pIcon, iSubdockViewType);
		pIcon->pSubDock = gldi_subdock_new (pIcon->cName, NULL, pParentDock, NULL);  // NULL <=> default sub-docks view.
	}
	else if (strcmp (cType, "Separator") == 0)
	{
		pIcon = gldi_separator_icon_new (NULL, NULL);
	}
	else
	{
		cd_warning ("type '%s' invalid", cType);
		return FALSE;
	}
	pIcon->cParentDockName = g_strdup (cParentDockName);
	
	//\_______________ load it inside the dock.
	cairo_dock_insert_icon_in_dock (pIcon, pParentDock, CAIRO_DOCK_ANIMATE_ICON);
	
	if (pIcon->cClass != NULL)
	{
		cairo_dock_inhibite_class (pIcon->cClass, pIcon);
	}
	
	return TRUE;
}

/* Not used
static void _find_launcher_in_dock (Icon *pIcon, CairoDock *pDock, gpointer *data)
{
	gchar *cDesktopFile = data[0];
	Icon **pFoundIcon = data[1];
	if ((pIcon->cDesktopFileName && g_ascii_strncasecmp (cDesktopFile, pIcon->cDesktopFileName, strlen (cDesktopFile)) == 0)
	|| (pIcon->cCommand && g_ascii_strncasecmp (cDesktopFile, pIcon->cCommand, strlen (cDesktopFile)) == 0))
	{
		*pFoundIcon = pIcon;
	}
}
* 
static Icon *cd_dbus_find_launcher (const gchar *cDesktopFile)
{
	Icon *pIcon = NULL;
	gpointer data[2];
	data[0] = (gpointer) cDesktopFile;
	data[1] = &pIcon;
	cairo_dock_foreach_icons_in_docks ((CairoDockForeachIconFunc) _find_launcher_in_dock, data);
	return pIcon;
}
*/
gboolean cd_dbus_main_reload_icon (dbusMainObject *pDbusCallback, gchar *cIconQuery, GError **error)
{
	if (! myConfig.bEnableTweakingLauncher)
		return FALSE;
	GList *pList = cd_dbus_find_matching_icons (cIconQuery);
	if (pList == NULL)
		return FALSE;
	
	Icon *pIcon;
	GldiContainer *pContainer;
	GList *ic;
	for (ic = pList; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		
		if ((CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pIcon)
			|| CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pIcon)
			|| CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pIcon))
		&& pIcon->cDesktopFileName != NULL)  // user icon.
		{
			cairo_dock_reload_launcher (pIcon);
		}
		else if (CAIRO_DOCK_IS_APPLET (pIcon))
		{
			gldi_module_instance_reload (pIcon->pModuleInstance, TRUE);  // TRUE <=> reload config.
		}
		else  // for appli icons, reload their image (custom image for instance).
		{
			pContainer = cairo_dock_get_icon_container (pIcon);
			if (pContainer == NULL)
				continue;
			
			cairo_dock_reload_icon_image (pIcon, pContainer);
			cairo_dock_redraw_icon (pIcon);
		}
	}
	
	return TRUE;
}

static gboolean _on_icon_deleted (GList *ic, Icon *pIcon)
{
	ic->data = NULL;
	return GLDI_NOTIFICATION_LET_PASS;
}
gboolean cd_dbus_main_remove_icon (dbusMainObject *pDbusCallback, gchar *cIconQuery, GError **error)
{
	if (! myConfig.bEnableTweakingLauncher)
		return FALSE;
	
	GList *pList = cd_dbus_find_matching_icons (cIconQuery);
	if (pList == NULL)
		return FALSE;
	
	// first connect to the "delete" signal, to not destroy 2 times an icon (case of an icon in a sub-dock that is destroyed just before).
	Icon *pIcon;
	GldiContainer *pContainer;
	GList *ic;
	for (ic = pList; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		gldi_object_register_notification (pIcon,
			NOTIFICATION_DESTROY,
			(GldiNotificationFunc) _on_icon_deleted,
			GLDI_RUN_FIRST, ic);
	}
	
	// delete all the matching icons, including their sub-icons.
	for (ic = pList; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (pIcon == NULL)  // icon has been destroyed just before.
			continue;
		
		gldi_object_remove_notification (pIcon,
			NOTIFICATION_DESTROY,
			(GldiNotificationFunc) _on_icon_deleted,
			ic);  // remove it now, since maybe this icon won't be deleted.
		
		pContainer = cairo_dock_get_icon_container (pIcon);
		if (pContainer == NULL)
			continue;
		
		if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pIcon)
		|| CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pIcon)
		|| CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pIcon))  // case of a launcher/separator/sub-dock inside a dock.
		{
			if (pIcon->pSubDock != NULL)  // on detruit le sous-dock et ce qu'il contient.
			{
				gldi_object_unref (GLDI_OBJECT(pIcon->pSubDock));
				pIcon->pSubDock = NULL;
			}

			cairo_dock_trigger_icon_removal_from_dock (pIcon);
		}
		else if (CAIRO_DOCK_IS_APPLET (pIcon))  // case of an applet inside a dock or a desklet.
		{
			gldi_object_delete (GLDI_OBJECT(pIcon->pModuleInstance));
		}  // don't remove appli icons, as they would anyway be re-created automatically by the applications-manager.
	}
	
	g_list_free (pList);
	return TRUE;
}


  ///////////////////
 /// SET ON ICON ///
///////////////////

gboolean cd_dbus_main_set_quick_info (dbusMainObject *pDbusCallback, const gchar *cQuickInfo, gchar *cIconQuery, GError **error)
{
	if (! myConfig.bEnableSetQuickInfo)
		return FALSE;
	
	GList *pList = cd_dbus_find_matching_icons (cIconQuery);
	if (pList == NULL)
		return TRUE;
	
	nullify_argument (cQuickInfo);
	
	Icon *pIcon;
	GldiContainer *pContainer;
	GList *ic;
	for (ic = pList; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		pContainer = cairo_dock_get_icon_container (pIcon);
		if (pContainer == NULL)
			continue;
		
		gldi_icon_set_quick_info (pIcon, cQuickInfo);
		cairo_dock_redraw_icon (pIcon);
	}
	
	g_list_free (pList);
	return TRUE;
}

gboolean cd_dbus_main_set_label (dbusMainObject *pDbusCallback, const gchar *cLabel, gchar *cIconQuery, GError **error)
{
	if (! myConfig.bEnableSetLabel)
		return FALSE;
	
	GList *pList = cd_dbus_find_matching_icons (cIconQuery);
	if (pList == NULL)
		return TRUE;
	
	nullify_argument (cLabel);
	
	Icon *pIcon;
	GldiContainer *pContainer;
	GList *ic;
	for (ic = pList; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		pContainer = cairo_dock_get_icon_container (pIcon);
		if (pContainer == NULL)
			continue;
		
		gldi_icon_set_name (pIcon, cLabel);
	}
	
	g_list_free (pList);
	return TRUE;
}

gboolean cd_dbus_main_set_icon (dbusMainObject *pDbusCallback, const gchar *cImage, gchar *cIconQuery, GError **error)
{
	if (! myConfig.bEnableSetIcon)
		return FALSE;
	
	GList *pList = cd_dbus_find_matching_icons (cIconQuery);
	if (pList == NULL)
		return TRUE;
	
	Icon *pIcon;
	GldiContainer *pContainer;
	GList *ic;
	for (ic = pList; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (pIcon->image.pSurface == NULL)
			continue;
		
		pContainer = cairo_dock_get_icon_container (pIcon);
		if (pContainer == NULL)
			continue;
		
		cairo_t *pIconContext = cairo_create (pIcon->image.pSurface);
		cairo_dock_set_image_on_icon (pIconContext, cImage, pIcon, pContainer);
		cairo_destroy (pIconContext);
		cairo_dock_redraw_icon (pIcon);
	}
	
	g_list_free (pList);
	return TRUE;
}

gboolean cd_dbus_main_set_emblem (dbusMainObject *pDbusCallback, const gchar *cImage, gint iPosition, gchar *cIconQuery, GError **error)
{
	if (! myConfig.bEnableSetIcon)
		return FALSE;
	
	GList *pList = cd_dbus_find_matching_icons (cIconQuery);
	if (pList == NULL)
		return TRUE;
	
	Icon *pIcon;
	GldiContainer *pContainer;
	GList *ic;
	for (ic = pList; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (pIcon->image.pSurface == NULL)
			continue;
		
		pContainer = cairo_dock_get_icon_container (pIcon);
		if (pContainer == NULL)
			continue;
		
		if (cImage == NULL || *cImage == '\0' || strcmp (cImage, "none") == 0)
		{
			cairo_dock_remove_overlay_at_position (pIcon, iPosition < CAIRO_OVERLAY_NB_POSITIONS ? iPosition : iPosition - CAIRO_OVERLAY_NB_POSITIONS, myApplet);  // for ease of use, handle both case similarily.
		}
		else
		{
			if (iPosition >= CAIRO_OVERLAY_NB_POSITIONS)  // [N; 2N-1] => print the overlay
				cairo_dock_print_overlay_on_icon_from_image (pIcon, pContainer, cImage, iPosition - CAIRO_OVERLAY_NB_POSITIONS);
			else  // [0, N-1] => add it
				cairo_dock_add_overlay_from_image (pIcon, cImage, iPosition, myApplet);  // use 'myApplet' to identify the overlays set by the Dbus plug-in (since the plug-in can't be deactivated, 'myApplet' is constant).
		}
		
		cairo_dock_redraw_icon (pIcon);
	}
	
	g_list_free (pList);
	return TRUE;
}

gboolean cd_dbus_main_animate (dbusMainObject *pDbusCallback, const gchar *cAnimation, gint iNbRounds, gchar *cIconQuery, GError **error)
{
	if (! myConfig.bEnableAnimateIcon || cAnimation == NULL)
		return FALSE;
	
	GList *pList = cd_dbus_find_matching_icons (cIconQuery);
	if (pList == NULL)
		return TRUE;
	
	Icon *pIcon;
	GldiContainer *pContainer;
	GList *ic;
	for (ic = pList; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		pContainer = cairo_dock_get_icon_container (pIcon);
		if (! CAIRO_DOCK_IS_DOCK (pContainer))
			continue;
		gldi_icon_request_animation (pIcon, cAnimation, iNbRounds);
	}
	
	g_list_free (pList);
	return TRUE;
}

gboolean cd_dbus_main_demands_attention (dbusMainObject *pDbusCallback, gboolean bStart, const gchar *cAnimation, gchar *cIconQuery, GError **error)
{
	if (! myConfig.bEnableAnimateIcon)
		return FALSE;
	
	GList *pList = cd_dbus_find_matching_icons (cIconQuery);
	if (pList == NULL)
		return TRUE;
	
	Icon *pIcon;
	GldiContainer *pContainer;
	GList *ic;
	for (ic = pList; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		pContainer = cairo_dock_get_icon_container (pIcon);
		if (! CAIRO_DOCK_IS_DOCK (pContainer))
			continue;
		
		if (bStart)
		{
			gldi_icon_request_attention (pIcon, cAnimation, 0);  // 0 <=> non-stop.
		}
		else if (pIcon->bIsDemandingAttention)
		{
			gldi_icon_stop_attention (pIcon);
		}
	}
	
	g_list_free (pList);
	return TRUE;
}

gboolean cd_dbus_main_show_dialog (dbusMainObject *pDbusCallback, const gchar *message, gint iDuration, gchar *cIconQuery, GError **error)
{
	if (! myConfig.bEnablePopUp)
		return FALSE;
	g_return_val_if_fail (message != NULL, FALSE);
	
	GList *pList = cd_dbus_find_matching_icons (cIconQuery);
	
	Icon *pIcon;
	GldiContainer *pContainer;
	GList *ic;
	for (ic = pList; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		pContainer = cairo_dock_get_icon_container (pIcon);
		if (! CAIRO_DOCK_IS_DOCK (pContainer))
			continue;
		gldi_dialog_show_temporary_with_icon (message, pIcon, pContainer, 1000 * iDuration, "same icon");
		break;  // only show 1 dialog.
	}
	
	if (ic == NULL)  // empty list, or didn't find a valid icon.
		gldi_dialog_show_general_message (message, 1000 * iDuration);
	
	g_list_free (pList);
	return TRUE;
}


#ifdef DBUSMENU_GTK_FOUND
static void _on_menu_destroyed (GtkWidget *menu, CDIconData *pData)
{
	//g_print ("\n+++ %s ()\n\n", __func__);
	if (pData != NULL && pData->menu_items_list != NULL)
	{
		GList *mi;
		for (mi = pData->menu_items_list; mi != NULL; mi = mi->next)
		{
			DbusmenuMenuitem *pDbusMenuItem = mi->data;
			GtkMenuItem *pMenuItem = dbusmenu_gtkclient_menuitem_get (pData->client, pDbusMenuItem);
			if (gtk_widget_get_parent (GTK_WIDGET (pMenuItem)) != NULL)  // it might not be in the menu if it has been added after the menu was built.
				gtk_container_remove (GTK_CONTAINER (menu), GTK_WIDGET (pMenuItem));
		}
	}
}
static gboolean cd_dbus_main_emit_on_build_menu (gpointer data, Icon *pClickedIcon, GldiContainer *pClickedContainer, GtkWidget *pMenu)
{
	if (pClickedIcon == NULL)
		return GLDI_NOTIFICATION_LET_PASS;
	
	CDIconData *pData = CD_APPLET_GET_MY_ICON_DATA (pClickedIcon);
	if (pData != NULL && pData->menu_items_list != NULL)
	{
		GList *mi;
		for (mi = pData->menu_items_list; mi != NULL; mi = mi->next)
		{
			DbusmenuMenuitem *pDbusMenuItem = mi->data;
			GtkMenuItem *pMenuItem = dbusmenu_gtkclient_menuitem_get (pData->client, pDbusMenuItem);
			gtk_menu_shell_append (GTK_MENU_SHELL(pMenu), GTK_WIDGET (pMenuItem));
			gtk_widget_show (GTK_WIDGET (pMenuItem));
		}
		
		// when the menu is destroyed, the menu-items are destroyed too (even if we set a reference on them, their content will be unvalidated); so we must remove them from the menu before that happens.
		g_signal_connect (G_OBJECT (pMenu),
			"destroy",
			G_CALLBACK (_on_menu_destroyed),
			pData);
	}
	return GLDI_NOTIFICATION_LET_PASS;
}


static void root_child_added (DbusmenuMenuitem * root, DbusmenuMenuitem * child, guint position, CDIconData *pData)
{
	cd_debug ("%s (%d)", __func__, position);
	pData->menu_items_list = g_list_insert (pData->menu_items_list, child, position);  // simply add it to the list, the associated menu-item will appear in the menu the next time the user right-clicks (we don't bother to refresh the menu in real-time).
}

static void root_child_moved (DbusmenuMenuitem * root, DbusmenuMenuitem * child, guint newposition, guint oldposition, CDIconData *pData)
{
	cd_debug ("%s (%d -> %d)", __func__, oldposition, newposition);
	GList *mi = g_list_nth (pData->menu_items_list, oldposition);
	pData->menu_items_list = g_list_remove_link (pData->menu_items_list, mi);
	pData->menu_items_list = g_list_insert (pData->menu_items_list, child, newposition);  // same remark
}

static void root_child_delete (DbusmenuMenuitem * root, DbusmenuMenuitem * child, CDIconData *pData)
{
	cd_debug ("%s ()", __func__);
	pData->menu_items_list = g_list_remove (pData->menu_items_list, child);  // same remark
}

static void root_changed (DbusmenuGtkClient * client, DbusmenuMenuitem * newroot, CDIconData *pData)
{
	cd_debug ("%s (%p", __func__, newroot);
	if (newroot == NULL)
	{
		return;
	}
	
	// get the current childs
	GList * child = NULL;
	for (child = dbusmenu_menuitem_get_children(newroot); child != NULL; child = g_list_next(child))
	{
		pData->menu_items_list = g_list_append (pData->menu_items_list, child->data);
	}
	
	// watch for any new/updated/removed child
	g_signal_connect(G_OBJECT(newroot), DBUSMENU_MENUITEM_SIGNAL_CHILD_ADDED, G_CALLBACK(root_child_added), pData);
	g_signal_connect(G_OBJECT(newroot), DBUSMENU_MENUITEM_SIGNAL_CHILD_MOVED, G_CALLBACK(root_child_moved), pData);
	g_signal_connect(G_OBJECT(newroot), DBUSMENU_MENUITEM_SIGNAL_CHILD_REMOVED, G_CALLBACK(root_child_delete), pData);
}

gboolean cd_dbus_main_set_menu (dbusMainObject *pDbusCallback, const gchar *cBusName, const gchar *cMenuPath, gchar *cIconQuery, GError **error)
{
	GList *pList = cd_dbus_find_matching_icons (cIconQuery);
	if (pList == NULL)
		return TRUE;
	
	cd_debug ("%s (%s , %s)", __func__, cBusName, cMenuPath);
	static gboolean s_bInit = FALSE;
	if (! s_bInit)  // register for right-click events once.
	{
		s_bInit = TRUE;
		gldi_object_register_notification (&myContainersMgr,
			NOTIFICATION_BUILD_ICON_MENU,
			(GldiNotificationFunc) cd_dbus_main_emit_on_build_menu,
			GLDI_RUN_FIRST,
			NULL);
	}
	
	if (cBusName && *cBusName == '\0')  // nullify empty object and path
		cBusName = NULL;
	if (cMenuPath && *cMenuPath == '\0')
		cMenuPath = NULL;
	
	Icon *pIcon;
	GList *ic;
	for (ic = pList; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		CDIconData *pData = CD_APPLET_GET_MY_ICON_DATA (pIcon);
		if (pData == NULL)
		{
			pData = g_new0 (CDIconData, 1);
			CD_APPLET_SET_MY_ICON_DATA (pIcon, pData);
		}
		
		if (! cairo_dock_strings_differ (pData->cMenuPath, cMenuPath)
		&& ! cairo_dock_strings_differ (pData->cBusName, cBusName))
			continue;  // same menu -> nothing to do
		
		// remove any previous menu
		if (pData->cBusName)
		{
			cd_debug ("menu %s (%s) is removed", pData->cBusName, pData->cMenuPath);
			g_free (pData->cBusName);
			g_free (pData->cMenuPath);
			
			g_list_free (pData->menu_items_list);
			pData->menu_items_list = NULL;
			
			g_object_unref (pData->client);
			pData->client = NULL;
		}
		
		// remember the current menu
		pData->cBusName = g_strdup (cBusName);
		pData->cMenuPath = g_strdup (cMenuPath);
		
		// if a menu is set, build the client and wait for the root child to appear on our side of the bus.
		if (cBusName && cMenuPath && *cMenuPath != '\0')
		{
			cd_debug ("new menu %s (%s)", cBusName, cMenuPath);
			pData->client = dbusmenu_gtkclient_new(pData->cBusName, pData->cMenuPath);
			g_signal_connect(G_OBJECT(pData->client), DBUSMENU_GTKCLIENT_SIGNAL_ROOT_CHANGED, G_CALLBACK(root_changed), pData);
		}
	}
	
	g_list_free (pList);
	return TRUE;
}
#else
gboolean cd_dbus_main_set_menu (dbusMainObject *pDbusCallback, const gchar *cBusName, const gchar *cMenuPath, gchar *cIconQuery, GError **error)
{
	g_set_error (error, 1, 1, "Cairo-Dock has not been compiled with DbusMenu support, so The 'SetMenu' method won't work.");
	return FALSE;
}
#endif


gboolean cd_dbus_main_set_progress (dbusMainObject *dbusMainObject, double fPercent, gchar *cIconQuery, GError **error)
{
	GList *pList = cd_dbus_find_matching_icons (cIconQuery);
	if (pList == NULL)
		return TRUE;
	
	Icon *pIcon;
	GList *ic;
	for (ic = pList; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		
		if (cairo_dock_get_icon_data_renderer (pIcon) == NULL)
		{
			CairoProgressBarAttribute attr;
			memset (&attr, 0, sizeof (CairoProgressBarAttribute));
			CairoDataRendererAttribute *pRenderAttr = CAIRO_DATA_RENDERER_ATTRIBUTE (&attr);
			pRenderAttr->cModelName = "progressbar";
			cairo_dock_add_new_data_renderer_on_icon (pIcon, pIcon->pContainer, pRenderAttr);
		}
		
		if (fPercent < 0)
			fPercent = CAIRO_DATA_RENDERER_UNDEF_VALUE;
		cairo_dock_render_new_data_on_icon (pIcon, pIcon->pContainer, NULL, &fPercent);
	}
	g_list_free (pList);
	return TRUE;
}


/**
Add -> config-file:
activate module: type=Module, module=module-name
instanciate module: type=Module-Instance, module=module-name
dock: type=Dock
launcher: type=Launcher, desktop-file, position/order, container [, name, icon, command, class]
icon-container: type=Icon-Container, name, icon, position/order, container
separator: type=Separator, position/order, container

Reload/Remove:
module: module=module-name
module instance: config-file=conf-file
icon: icon-query
dock: config-file=conf-file, container=dock-name

GetProperties -> dict
module=module-name
container=container-name
icon query
*/

  ///////////
 /// ADD ///
///////////

gboolean cd_dbus_main_add (dbusMainObject *pDbusCallback, GHashTable *pProperties, gchar **cConfigFile, GError **error)
{
	GValue *v;
	const gchar *cType = "";
	v = g_hash_table_lookup (pProperties, "type");
	if (v && G_VALUE_HOLDS_STRING (v))
		cType = g_value_get_string (v);
	CDMainType iType = cd_dbus_get_main_type (cType, -1);
	
	switch (iType)
	{
		case CD_MAIN_TYPE_ICON:
		{
			// get the dock
			const gchar *cDockName = NULL;
			v = g_hash_table_lookup (pProperties, "container");
			if (v && G_VALUE_HOLDS_STRING (v))
				cDockName = g_value_get_string (v);
			if (cDockName == NULL)
				cDockName = CAIRO_DOCK_MAIN_DOCK_NAME;
			CairoDock *pParentDock = gldi_dock_get (cDockName);
			if (pParentDock == NULL)
			{
				cd_warning ("dock %s does not exist", cDockName);
				pParentDock = g_pMainDock;
			}
			
			// get the order
			double fOrder = 0;
			v = g_hash_table_lookup (pProperties, "order");
			if (v)
			{
				if (G_VALUE_HOLDS_DOUBLE (v))
					fOrder = g_value_get_double (v);
				else if (G_VALUE_HOLDS_INT (v))
					fOrder = g_value_get_int (v);
			}
			if (fOrder < 0)
				fOrder = CAIRO_DOCK_LAST_ORDER;
			
			Icon *pNewIcon = NULL;
			if (strcmp (cType, CD_TYPE_LAUNCHER) == 0)
			{
				const gchar *cDesktopFile = NULL;
				v = g_hash_table_lookup (pProperties, "config-file");
				if (v && G_VALUE_HOLDS_STRING (v))
					cDesktopFile = g_value_get_string (v);
				
				g_print ("add %s\n", cDesktopFile);
				if (cDesktopFile != NULL)
				{
					pNewIcon = gldi_launcher_add_new (cDesktopFile, pParentDock, fOrder);
				}
				else
				{
					pNewIcon = gldi_launcher_add_new (NULL, pParentDock, fOrder);
					
					// get additional properties
					const gchar *cName = NULL;
					v = g_hash_table_lookup (pProperties, "name");
					if (v && G_VALUE_HOLDS_STRING (v))
						cName = g_value_get_string (v);
					const gchar *cIcon = NULL;
					v = g_hash_table_lookup (pProperties, "icon");
					if (v && G_VALUE_HOLDS_STRING (v))
						cIcon = g_value_get_string (v);
					const gchar *cCommand = NULL;
					v = g_hash_table_lookup (pProperties, "command");
					if (v && G_VALUE_HOLDS_STRING (v))
						cCommand = g_value_get_string (v);
					const gchar *cClass = NULL;
					v = g_hash_table_lookup (pProperties, "class");
					if (v && G_VALUE_HOLDS_STRING (v))
						cClass = g_value_get_string (v);
					
					// open the conf-file and set the fields.
					gchar *cConfFilePath = (*pNewIcon->cDesktopFileName == '/' ? g_strdup (pNewIcon->cDesktopFileName) : g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, pNewIcon->cDesktopFileName));
					GKeyFile *pKeyFile = cairo_dock_open_key_file (cConfFilePath);
					
					if (cName)
						g_key_file_set_string (pKeyFile, "Desktop Entry", "Name", cName);
					
					if (cIcon)
						g_key_file_set_string (pKeyFile, "Desktop Entry", "Icon", cIcon);
					
					if (cCommand)
						g_key_file_set_string (pKeyFile, "Desktop Entry", "Exec", cCommand);
					
					if (cClass)
						g_key_file_set_string (pKeyFile, "Desktop Entry", "StartupWMClass", cClass);
					
					cairo_dock_write_keys_to_file (pKeyFile, cConfFilePath);
					
					g_key_file_free (pKeyFile);
					g_free (cConfFilePath);
					
					cairo_dock_reload_launcher (pNewIcon);
				}
			}
			else if (strcmp (cType, CD_TYPE_SEPARATOR) == 0)
			{
				pNewIcon = gldi_separator_icon_add_new (pParentDock, fOrder);
			}
			else if (strcmp (cType, CD_TYPE_ICON_CONTAINER) == 0)
			{
				pNewIcon = gldi_stack_icon_add_new (pParentDock, fOrder);
				
				// get additional properties
				const gchar *cName = NULL;
				v = g_hash_table_lookup (pProperties, "name");
				if (v && G_VALUE_HOLDS_STRING (v))
					cName = g_value_get_string (v);
				const gchar *cIcon = NULL;
				v = g_hash_table_lookup (pProperties, "icon");
				if (v && G_VALUE_HOLDS_STRING (v))
					cIcon = g_value_get_string (v);
				
				// open the conf-file and set the fields.
				gchar *cConfFilePath = (*pNewIcon->cDesktopFileName == '/' ? g_strdup (pNewIcon->cDesktopFileName) : g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, pNewIcon->cDesktopFileName));
				GKeyFile *pKeyFile = cairo_dock_open_key_file (cConfFilePath);
				
				if (cName)
					g_key_file_set_string (pKeyFile, "Desktop Entry", "Name", cName);
				
				if (cIcon)
					g_key_file_set_string (pKeyFile, "Desktop Entry", "Icon", cIcon);
				
				cairo_dock_write_keys_to_file (pKeyFile, cConfFilePath);
				
				g_key_file_free (pKeyFile);
				g_free (cConfFilePath);
				
				cairo_dock_reload_launcher (pNewIcon);
			}
			else
			{
				g_set_error (error, 1, 1, "can't add an icon of type '%s'", cType);
				return FALSE;
			}
			if (pNewIcon != NULL && pNewIcon->cDesktopFileName != NULL)
				*cConfigFile = (*pNewIcon->cDesktopFileName == '/' ? g_strdup (pNewIcon->cDesktopFileName) : g_strdup_printf ("%s/%s", g_cCurrentLaunchersPath, pNewIcon->cDesktopFileName));
		}
		break;
		
		case CD_MAIN_TYPE_CONTAINER:
		{
			if (strcmp (cType, CD_TYPE_DOCK) == 0)
			{
				gchar *cDockName = gldi_dock_add_conf_file ();
				CairoDock *pDock = gldi_dock_new (cDockName);
				if (!pDock)
					return FALSE;
				gldi_dock_reload (pDock);
				*cConfigFile = g_strdup_printf ("%s/%s.conf", g_cCurrentThemePath, cDockName);
				g_free (cDockName);
			}
			else
			{
				g_set_error (error, 1, 1, "can't add a desklet, add a module-instance instead");
				return FALSE;
			}
		}
		break;
		case CD_MAIN_TYPE_MODULE:
		{
			const gchar *cModuleName = NULL;
			v = g_hash_table_lookup (pProperties, "module");
			if (v && G_VALUE_HOLDS_STRING (v))
				cModuleName = g_value_get_string (v);
			
			GldiModule *pModule = gldi_module_get (cModuleName);
			if (pModule == NULL)
			{
				g_set_error (error, 1, 1, "no such module (%s)", cModuleName);
				return FALSE;
			}
			if (pModule->pInstancesList == NULL)
				gldi_module_activate (pModule);
		}
		break;
		case CD_MAIN_TYPE_MODULE_INSTANCE:
		{
			const gchar *cModuleName = NULL;
			v = g_hash_table_lookup (pProperties, "module");
			if (v && G_VALUE_HOLDS_STRING (v))
				cModuleName = g_value_get_string (v);
			
			GldiModule *pModule = gldi_module_get (cModuleName);
			if (pModule == NULL)
			{
				g_set_error (error, 1, 1, "no such module (%s)", cModuleName);
				return FALSE;
			}
			
			if (pModule->pInstancesList == NULL)
			{
				gldi_module_activate (pModule);
			}
			else if (pModule->pVisitCard->bMultiInstance)
			{
				gldi_module_add_instance (pModule);
			}
			if (pModule->pInstancesList)
			{
				GldiModuleInstance *pModuleInstance = pModule->pInstancesList->data;  // prepend
				*cConfigFile = g_strdup (pModuleInstance->cConfFilePath);
			}
		}
		break;
		default:
			g_set_error (error, 1, 1, "Unknown type (%s)", cType);
			return FALSE;
		break;
	}
	
	return TRUE;
}


  //////////////
 /// RELOAD ///
//////////////

gboolean cd_dbus_main_reload (dbusMainObject *pDbusCallback, gchar *cQuery, GError **error)
{
	GList *pObjects = cd_dbus_find_matching_objects (cQuery);
	GList *o;
	GldiObject *obj;
	for (o = pObjects; o != NULL; o = o->next)
	{
		obj = o->data;
		if (CAIRO_DOCK_IS_ICON (obj))
		{
			Icon *pIcon = (Icon*)obj;
			if ((CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pIcon)
				|| CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pIcon)
				|| CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pIcon))
			&& pIcon->cDesktopFileName != NULL)  // user icon.
			{
				cairo_dock_reload_launcher (pIcon);
			}
			else if (CAIRO_DOCK_IS_APPLET (pIcon))
			{
				gldi_module_instance_reload (pIcon->pModuleInstance, TRUE);  // TRUE <=> reload config.
			}
			else  // for appli icons, reload their image (custom image for instance).
			{
				GldiContainer *pContainer = cairo_dock_get_icon_container (pIcon);
				if (pContainer == NULL)
					continue;
				
				cairo_dock_reload_icon_image (pIcon, pContainer);
				cairo_dock_redraw_icon (pIcon);
			}
		}
		else if (CAIRO_DOCK_IS_CONTAINER (obj))
		{
			if (CAIRO_DOCK_IS_DOCK (obj))
			{
				CairoDock *pDock = CAIRO_DOCK (obj);
				if (!pDock->bIsMainDock)  // pour l'instant le main dock n'a pas de fichier de conf
				{
					gldi_dock_reload (pDock);
				}
			}
			else if (CAIRO_DOCK_IS_DESKLET (obj))
			{
				CairoDesklet *pDesklet = CAIRO_DESKLET (obj);
				Icon *pIcon = pDesklet->pIcon;
				g_return_val_if_fail (CAIRO_DOCK_IS_APPLET (pIcon), FALSE);
				gldi_module_instance_reload (pIcon->pModuleInstance, TRUE);  // TRUE <=> reload config.
			}
		}
		else if (CAIRO_DOCK_IS_MODULE (obj))
		{
			gldi_module_reload ((GldiModule*)obj, TRUE);  // TRUE <=> reload module conf file.
		}
		else if (CAIRO_DOCK_IS_MANAGER (obj))
		{
			gldi_reload_manager ((GldiManager*)obj, g_cConfFile);
		}
		else if (CAIRO_DOCK_IS_MODULE_INSTANCE (obj))
		{
			gldi_module_instance_reload ((GldiModuleInstance*)obj, TRUE);  // TRUE <=> reload config.
		}
	}
	g_list_free (pObjects);
	return TRUE;
}


  //////////////
 /// REMOVE ///
//////////////

static gboolean _on_object_deleted (GList *o, G_GNUC_UNUSED GldiObject *obj)
{
	o->data = NULL;
	return GLDI_NOTIFICATION_LET_PASS;
}
gboolean cd_dbus_main_remove (dbusMainObject *pDbusCallback, gchar *cQuery, GError **error)
{
	GList *pObjects = cd_dbus_find_matching_objects (cQuery);
	
	// first connect to the "delete" signal, to not destroy 2 times an icon (case of an icon in a sub-dock that is destroyed just before).
	GldiObject *obj;
	GList *o;
	for (o = pObjects; o != NULL; o = o->next)
	{
		obj = o->data;
		gldi_object_register_notification (obj,
			NOTIFICATION_DESTROY,
			(GldiNotificationFunc) _on_object_deleted,
			GLDI_RUN_FIRST, o);
	}
	
	for (o = pObjects; o != NULL; o = o->next)
	{
		obj = o->data;
		if (! obj)
			continue;
		if (CAIRO_DOCK_IS_ICON (obj))
		{
			Icon *pIcon = (Icon*)obj;
			if ((CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pIcon)
				|| CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pIcon)
				|| CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pIcon))
			&& pIcon->cDesktopFileName != NULL)  // user icon.
			{
				if (CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pIcon)
				&& pIcon->pSubDock != NULL)  // remove the sub-dock's content as well
				{
					cairo_dock_remove_icons_from_dock (pIcon->pSubDock, NULL);
					gldi_object_unref (GLDI_OBJECT(pIcon->pSubDock));
					pIcon->pSubDock = NULL;
				}
				cairo_dock_trigger_icon_removal_from_dock (pIcon);
			}
			else if (CAIRO_DOCK_IS_APPLET (pIcon))
			{
				gldi_module_instance_reload (pIcon->pModuleInstance, TRUE);  // TRUE <=> reload config.
			}  // don't remove appli icons
		}
		else if (CAIRO_DOCK_IS_CONTAINER (obj))
		{
			if (CAIRO_DOCK_IS_DOCK (obj))
			{
				CairoDock *pDock = CAIRO_DOCK (obj);
				cairo_dock_remove_icons_from_dock (pDock, NULL);
				
				gldi_object_unref (GLDI_OBJECT(pDock));
			}
			else if (CAIRO_DOCK_IS_DESKLET (obj))
			{
				CairoDesklet *pDesklet = CAIRO_DESKLET (obj);
				Icon *pIcon = pDesklet->pIcon;
				g_return_val_if_fail (CAIRO_DOCK_IS_APPLET (pIcon), FALSE);
				gldi_object_delete (GLDI_OBJECT(pIcon->pModuleInstance));
			}
		}
		else if (CAIRO_DOCK_IS_MODULE (obj))
		{
			GldiModule *pModule = (GldiModule *)obj;
			gldi_module_deactivate (pModule);
		}
		else if (CAIRO_DOCK_IS_MANAGER (obj))
		{
			cd_warning ("can't remove a manager");
		}
		else if (CAIRO_DOCK_IS_MODULE_INSTANCE (obj))
		{
			GldiModuleInstance *pModuleInstance = (GldiModuleInstance *)obj;
			g_print ("remove instance %s\n", pModuleInstance->cConfFilePath);
			gldi_object_delete (GLDI_OBJECT(pModuleInstance));
		}
	}
	g_list_free (pObjects);
	return TRUE;
}


  //////////////////
 /// PROPERTIES ///
//////////////////

static void _add_icon_properties (Icon *pIcon, GPtrArray *pTab)
{
	GldiContainer *pContainer = cairo_dock_get_icon_container (pIcon);
	
	GHashTable *h = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		g_free,  /// can we use const char here instead of duplicating each string ?...
		g_free);
	g_ptr_array_add (pTab, h);
	
	GValue *v;
	int iPosition;
	const gchar *cType;
	const gchar *cContainerName;
	const gchar *cDesktopFile;
	
	if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pIcon))
		cType = CD_TYPE_LAUNCHER;
	else if (CAIRO_DOCK_ICON_TYPE_IS_APPLI (pIcon))
		cType = CD_TYPE_APPLICATION;
	else if (CAIRO_DOCK_ICON_TYPE_IS_APPLET (pIcon))
		cType = CD_TYPE_APPLET;
	else if (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pIcon))
		cType = CD_TYPE_SEPARATOR;
	else if (CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pIcon))
		cType = CD_TYPE_ICON_CONTAINER;
	else if (CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (pIcon))
		cType = CD_TYPE_CLASS_CONTAINER;
	else
		cType = CD_TYPE_ICON_OTHER;
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, cType);
	g_hash_table_insert (h, g_strdup ("type"), v);
	
	cDesktopFile = "";
	if (pIcon->cDesktopFileName != NULL)
		cDesktopFile = pIcon->cDesktopFileName;
	else if (CAIRO_DOCK_IS_APPLET (pIcon))
		cDesktopFile = pIcon->pModuleInstance->cConfFilePath;
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, cDesktopFile);
	g_hash_table_insert (h, g_strdup ("config-file"), v);
	
	if (CAIRO_DOCK_IS_APPLET (pIcon))
	{
		v = g_new0 (GValue, 1);
		g_value_init (v, G_TYPE_STRING);
		g_value_set_string (v, pIcon->pModuleInstance->pModule->pVisitCard->cModuleName);
		g_hash_table_insert (h, g_strdup ("module"), v);
	}
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, pIcon->cName);  /// g_value_set_static_string ?...
	g_hash_table_insert (h, g_strdup ("name"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, pIcon->cCommand);
	g_hash_table_insert (h, g_strdup ("command"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, pIcon->cClass);
	g_hash_table_insert (h, g_strdup ("class"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, pIcon->cFileName);
	g_hash_table_insert (h, g_strdup ("icon"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, pIcon->cQuickInfo);
	g_hash_table_insert (h, g_strdup ("quick-info"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_UINT);
	g_value_set_uint (v, GPOINTER_TO_INT(pIcon->pAppli));
	g_hash_table_insert (h, g_strdup ("Xid"), v);
	
	iPosition = -1;
	cContainerName = "";
	if (CAIRO_DOCK_IS_DOCK (pContainer))
	{
		CairoDock *pDock = CAIRO_DOCK (pContainer);
		iPosition = g_list_index (pDock->icons, pIcon);
		cContainerName = pIcon->cParentDockName;
	}
	else if (CAIRO_DOCK_IS_DESKLET (pContainer))
	{
		CairoDesklet *pDesklet = CAIRO_DESKLET (pContainer);
		if (pDesklet->pIcon == pIcon)
			iPosition = 0;
		else
			iPosition = g_list_index (pDesklet->icons, pIcon);
		if (CAIRO_DOCK_IS_APPLET (pDesklet->pIcon))
			cContainerName = pDesklet->pIcon->pModuleInstance->pModule->pVisitCard->cModuleName;
	}
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_INT);
	g_value_set_int (v, iPosition);
	g_hash_table_insert (h, g_strdup ("position"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, cContainerName);
	g_hash_table_insert (h, g_strdup ("container"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_DOUBLE);
	g_value_set_double (v, pIcon->fOrder);
	g_hash_table_insert (h, g_strdup ("order"), v);
}

static void _add_module_properties (GldiModule *pModule, GPtrArray *pTab)
{
	GHashTable *h = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		g_free,  /// can we use const char here instead of duplicating each string ?...
		g_free);
	g_ptr_array_add (pTab, h);
	
	GValue *v;
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, "Module");
	g_hash_table_insert (h, g_strdup ("type"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, pModule->pVisitCard->cModuleName);
	g_hash_table_insert (h, g_strdup ("name"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_UINT);
	g_value_set_uint (v, pModule->pVisitCard->iContainerType);
	g_hash_table_insert (h, g_strdup ("module-type"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_UINT);
	g_value_set_uint (v, pModule->pVisitCard->iCategory);
	g_hash_table_insert (h, g_strdup ("category"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, pModule->pVisitCard->cTitle);
	g_hash_table_insert (h, g_strdup ("title"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, pModule->pVisitCard->cIconFilePath);
	g_hash_table_insert (h, g_strdup ("icon"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, pModule->pVisitCard->cPreviewFilePath);
	g_hash_table_insert (h, g_strdup ("preview"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, dgettext (pModule->pVisitCard->cGettextDomain, pModule->pVisitCard->cDescription));
	g_hash_table_insert (h, g_strdup ("description"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, pModule->pVisitCard->cAuthor);
	g_hash_table_insert (h, g_strdup ("author"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_BOOLEAN);
	g_value_set_boolean (v, pModule->pVisitCard->bMultiInstance);
	g_hash_table_insert (h, g_strdup ("is-multi-instance"), v);
	
	cd_debug ("list instances ...");
	gchar **pInstances = g_new0 (gchar*, g_list_length (pModule->pInstancesList)+1);
	GldiModuleInstance *pInstance;
	int i = 0;
	GList *mi;
	for (mi = pModule->pInstancesList; mi != NULL; mi = mi->next)
	{
		pInstance = mi->data;
		pInstances[i++] = g_strdup (pInstance->cConfFilePath);
	}
	cd_debug ("write instances ...");
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRV);
	g_value_set_boxed (v, pInstances);
	g_hash_table_insert (h, g_strdup ("instances"), v);
	cd_debug ("done.");
}

static void _add_manager_properties (GldiManager *pManager, GPtrArray *pTab)
{
	GHashTable *h = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		g_free,  /// can we use const char here instead of duplicating each string ?...
		g_free);
	g_ptr_array_add (pTab, h);
	
	GValue *v;
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, CD_TYPE_MANAGER);
	g_hash_table_insert (h, g_strdup ("type"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, pManager->cModuleName);
	g_hash_table_insert (h, g_strdup ("name"), v);
}

static void _set_container_properties (GldiContainer *pContainer, GHashTable *h)
{
	GValue *v;
	int x, y, w, ht;
	if (pContainer->bIsHorizontal)
	{
		x = pContainer->iWindowPositionX;
		y = pContainer->iWindowPositionY;
		w = pContainer->iWidth;
		ht = pContainer->iHeight;
	}
	else
	{
		y = pContainer->iWindowPositionX;
		x = pContainer->iWindowPositionY;
		ht = pContainer->iWidth;
		w = pContainer->iHeight;
	}
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_INT);
	g_value_set_int (v, x);
	g_hash_table_insert (h, g_strdup ("x"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_INT);
	g_value_set_int (v, y);
	g_hash_table_insert (h, g_strdup ("y"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_INT);
	g_value_set_int (v, w);
	g_hash_table_insert (h, g_strdup ("width"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_INT);
	g_value_set_int (v, ht);
	g_hash_table_insert (h, g_strdup ("height"), v);
	
	CairoDockPositionType iScreenBorder = ((! pContainer->bIsHorizontal) << 1) | (! pContainer->bDirectionUp);
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_UINT);
	g_value_set_uint (v, iScreenBorder);
	g_hash_table_insert (h, g_strdup ("orientation"), v);
}

static void _add_dock_properties (CairoDock *pDock, GPtrArray *pTab)
{
	GHashTable *h = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		g_free,  /// can we use const char here instead of duplicating each string ?...
		g_free);
	g_ptr_array_add (pTab, h);
	
	GValue *v;
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, CD_TYPE_DOCK);
	g_hash_table_insert (h, g_strdup ("type"), v);
	
	const gchar *cDockName = gldi_dock_get_name (pDock);
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, cDockName);
	g_hash_table_insert (h, g_strdup ("name"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_BOOLEAN);
	g_value_set_boolean (v, (pDock->iRefCount > 0));
	g_hash_table_insert (h, g_strdup ("is-sub-dock"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_INT);
	g_value_set_int (v, g_list_length (pDock->icons));
	g_hash_table_insert (h, g_strdup ("nb-icons"), v);
	
	_set_container_properties (CAIRO_CONTAINER (pDock), h);
}

static void _add_desklet_properties (CairoDesklet *pDesklet, GPtrArray *pTab)
{
	GHashTable *h = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		g_free,  /// can we use const char here instead of duplicating each string ?...
		g_free);
	g_ptr_array_add (pTab, h);
	
	GValue *v;
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, CD_TYPE_DESKLET);
	g_hash_table_insert (h, g_strdup ("type"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, CAIRO_DOCK_IS_APPLET (pDesklet->pIcon) ? pDesklet->pIcon->pModuleInstance->pModule->pVisitCard->cModuleName : "");
	g_hash_table_insert (h, g_strdup ("name"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_INT);
	g_value_set_int (v, 1 + g_list_length (pDesklet->icons));
	g_hash_table_insert (h, g_strdup ("nb-icons"), v);
	
	_set_container_properties (CAIRO_CONTAINER (pDesklet), h);
}

static void _add_module_instance_properties (GldiModuleInstance *pModuleInstance, GPtrArray *pTab)
{
	GHashTable *h = g_hash_table_new_full (g_str_hash,
		g_str_equal,
		g_free,  /// can we use const char here instead of duplicating each string ?...
		g_free);
	g_ptr_array_add (pTab, h);
	
	GValue *v;
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, CD_TYPE_MODULE_INSTANCE);
	g_hash_table_insert (h, g_strdup ("type"), v);
	
	v = g_new0 (GValue, 1);
	g_value_init (v, G_TYPE_STRING);
	g_value_set_string (v, pModuleInstance->pModule->pVisitCard->cModuleName);
	g_hash_table_insert (h, g_strdup ("name"), v);
}

gboolean cd_dbus_main_get_properties (dbusMainObject *pDbusCallback, gchar *cQuery, GPtrArray **pAttributes, GError **error)
{
	GPtrArray *pTab = g_ptr_array_new ();
	*pAttributes = pTab;
	
	GList *pObjects = cd_dbus_find_matching_objects (cQuery);
	GList *o;
	GldiObject *obj;
	for (o = pObjects; o != NULL; o = o->next)
	{
		obj = o->data;
		if (CAIRO_DOCK_IS_ICON (obj))
		{
			Icon *pIcon = (Icon*)obj;
			_add_icon_properties (pIcon, pTab);
		}
		else if (CAIRO_DOCK_IS_CONTAINER (obj))
		{
			if (CAIRO_DOCK_IS_DOCK (obj))
			{
				CairoDock *pDock = CAIRO_DOCK (obj);
				_add_dock_properties (pDock, pTab);
			}
			else if (CAIRO_DOCK_IS_DESKLET (obj))
			{
				CairoDesklet *pDesklet = CAIRO_DESKLET (obj);
				_add_desklet_properties (pDesklet, pTab);
			}
		}
		else if (CAIRO_DOCK_IS_MODULE (obj))
		{
			GldiModule *pModule = (GldiModule *)obj;
			_add_module_properties (pModule, pTab);
		}
		else if (CAIRO_DOCK_IS_MANAGER (obj))
		{
			GldiManager *pManager = (GldiManager *)obj;
			_add_manager_properties (pManager, pTab);
		}
		else if (CAIRO_DOCK_IS_MODULE_INSTANCE (obj))
		{
			GldiModuleInstance *pModuleInstance = (GldiModuleInstance *)obj;
			_add_module_instance_properties (pModuleInstance, pTab);
		}
	}
	g_list_free (pObjects);
	return TRUE;
}
