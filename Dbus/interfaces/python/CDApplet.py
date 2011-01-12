# This is a part of the external demo applet for Cairo-Dock
# Copyright : (C) 2010-2011 by Fabounet
# E-mail : fabounet@glx-dock.org
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
# http://www.gnu.org/licenses/licenses.html#GPL

# Base class for Cairo-Dock's applets.
# Make your own class derive from a CDApplet, and override the functions you need (the ones which don't start with an underscore).

####################
### dependancies ###
####################
import sys
import os.path
import ConfigParser
import gobject
import glib
import dbus
from dbus.mainloop.glib import DBusGMainLoop

DBusGMainLoop(set_as_default=True)

####################
### Applet class ###
####################
class CDApplet:
	
	BOTTOM = 0
	TOP    = 1
	RIGHT  = 2
	LEFT   = 3
	DOCK    = 0
	DESKLET = 1
	UPPER_LEFT  = 0
	LOWER_RIGHT = 1
	LOWER_LEFT  = 2
	UPPER_RIGHT = 3
	MIDDLE      = 4

	
	def __init__(self,applet_name=None):
		""" initialize the applet. Must be called by any class that inheritates from it.
		It defines the following:
		 - icon : our main icon
		 - sub_icons: our sub-icons
		 - config : a dictionnary where our configuration parameters are stored
		 - applet_name : name of our applet (the same as the folder where files are stored)
		 - conf_file : path to our config file (you will rarely need it)
		 """
		self.icon = None
		self.sub_icons = None
		self.config = {}
		if applet_name is None:
			self.applet_name = os.path.basename(os.path.abspath("."))  # the name of the applet must the same as the folder.
		else:
			self.applet_name = applet_name
		self.conf_file = os.path.expanduser("~/.config/cairo-dock/current_theme/plug-ins/"+self.applet_name+"/"+self.applet_name+".conf")  # path to the conf file of our applet.
		
		self._get_config()
		self._connect_to_dock()
	
	def run(self):
		""" start the applet and enter the main loop; we never get out of this function """
		self.begin()
		self.loop = gobject.MainLoop()
		self.loop.run()
		print ">>> applet '"+self.applet_name+"' terminated."
		sys.exit(0)
	
	##################################
	### callbacks on the main icon ###
	##################################
	def on_click(self,iState):
		""" action on click """
		pass
	
	def on_middle_click(self):
		""" action on middle-click """
		pass
	
	def on_build_menu(self):
		""" build our menu """
		pass
		
	def on_menu_select(self,iNumEntry):
		""" action on selecting an entry of our menu """
		pass
	
	def on_scroll(self,bScrollUp):
		""" action on scroll """
		pass
	
	def on_drop_data(self,cReceivedData):
		""" action on dropping something on our applet """
	
	def on_answer(self,answer):
		""" action on answering a dialog """
		pass
	
	def on_answer_dialog(self, button, answer):
		""" action on answering a dialog """
		pass
	
	def on_shortkey(self,cKey):
		""" action on pressing one of the shortkeys we bound beforehand """
		pass
	
	def on_change_focus(self,bIsActive):
		""" action when the window controlled by the applet takes or looses the focus """
		pass
	
	##################################
	### callbacks on the sub-icons ###
	##################################
	def on_click_sub_icon(self, iState, cIconID):
		""" action on click on one of our sub-icons"""
		pass
	
	###############################
	### callbacks on the applet ###
	###############################
	def begin(self):
		""" action when the applet is started """
		pass
	
	def end(self):
		""" action when the applet is terminated """
		pass
	
	def _on_stop(self):
		self.end()
		self.loop.quit()
	
	def reload(self):
		""" called when our applet is reloaded (config parameters have changed) """
		pass
	
	def _on_reload(self,bConfigHasChanged):
		if bConfigHasChanged:
			self._get_config()
			self.reload()
	
	def get_config(self,keyfile):
		""" get our parameters from the key-file """
		pass
	
	def _get_config(self):
		keyfile = ConfigParser.RawConfigParser()
		keyfile.read(self.conf_file)
		self.get_config(keyfile)
	
	def _connect_to_dock(self):
		# get our applet on the bus.
		print "self.applet_name:"+self.applet_name
		applet_path = "/org/cairodock/CairoDock/"+self.applet_name  # path where our object is stored on the bus.
		bus = dbus.SessionBus()
		try:
			applet_object = bus.get_object("org.cairodock.CairoDock", applet_path)
		except:
			print ">>> object '"+self.applet_name+"' can't be found on the bus, exit.\nMake sure that the 'Dbus' plug-in is activated in Cairo-Dock"
			sys.exit(2)
		self.icon = dbus.Interface(applet_object, "org.cairodock.CairoDock.applet")  # this object represents our icon inside the dock or a desklet.
		sub_icons_object = bus.get_object("org.cairodock.CairoDock", applet_path+"/sub_icons")
		self.sub_icons = dbus.Interface(sub_icons_object, "org.cairodock.CairoDock.subapplet")  # this object represents the list of icons contained in our sub-dock, or in our desklet. We'll add them one by one later, giving them a unique ID, which will be used to identify each of them.
		# connect to signals.
		self.icon.connect_to_signal("on_click", self.on_click)  # when the user left-clicks on our icon.
		self.icon.connect_to_signal("on_middle_click", self.on_middle_click)  # when the user middle-clicks on our icon.
		self.icon.connect_to_signal("on_build_menu", self.on_build_menu)  # when the user right-clicks on our applet (which builds the menu)
		self.icon.connect_to_signal("on_menu_select", self.on_menu_select)  # when the user selects an entry of this menu.
		self.icon.connect_to_signal("on_scroll", self.on_scroll)  # when the user scroll up or down on our icon.
		self.icon.connect_to_signal("on_drop_data", self.on_drop_data)  # when the user drops something on our icon.
		self.icon.connect_to_signal("on_answer", self.on_answer)  # when the user answer a question.
		self.icon.connect_to_signal("on_answer_dialog", self.on_answer_dialog)  # when the user answer a question.
		self.icon.connect_to_signal("on_shortkey", self.on_shortkey)  # when the user press the shortkey.
		self.icon.connect_to_signal("on_change_focus", self.on_change_focus)  # when the window's focus changes.
		self.icon.connect_to_signal("on_stop_module", self._on_stop)  # when the user deactivate our applet (or the DBus plug-in, or when the Cairo-Dock is stopped).
		self.icon.connect_to_signal("on_reload_module", self._on_reload)  # when the user changes something in our config, or when the desklet is resized (with no change in the config).
		self.sub_icons.connect_to_signal("on_click_sub_icon", self.on_click_sub_icon)  # when the user left-clicks on a sub-icon.