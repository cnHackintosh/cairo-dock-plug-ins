#!/usr/bin/ruby

# This is a part of the external Ruby Battery applet for Cairo-Dock
#
# Author: Eduardo Mucelli Rezende Oliveira
# E-mail: edumucelli@gmail.com or eduardom@dcc.ufmg.br
#
# This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.

# This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.

# This applet monitors the battery through acpi module. It is possbile to show a dialog message
#    containing the baterry status, charge, and temperature by left-clicking on the icon.
#    Also, through the configuration panel it is possible to set the icon's label, 
#	 show the % of carge as quick info, and activate an alert message to be shown when the charge is critically low.

require 'rubygems'
require 'dbus'
require 'parseconfig'

class CDApplet
	attr_accessor :conf_file, :applet_name, :icon, :sub_icons, :config, :bus
	
	def initialize
		self.applet_name = File.basename(Dir.getwd)
		self.conf_file = File.expand_path("~/.config/cairo-dock/current_theme/plug-ins/#{self.applet_name}/#{self.applet_name}.conf")
		self.config = {}
		self.bus = nil
		self.icon = nil
		self.sub_icons = nil
		
		self._get_config()
		self._connect_to_dock()
	end
	
	def run
		self.start()
		loop = DBus::Main.new
		loop << self.bus
		loop.run
		puts ">>> applet '#{self.applet_name}' terminated."
		exit
	end
	
	##################################
	### callbacks on the main icon ###
	##################################
	
	def on_click iState
		### action on click
	end
	
	def on_middle_click
		puts ">>> on_middle_click"
		### action on middle-click
	end
	
	def on_build_menu
		### build our menu
	end
	
	def on_menu_select(iNumEntry)
		### action on selecting an entry of our menu """
	end
	
	def on_scroll(bScrollUp)
		### action on scroll
	end

	def on_drop_data(cReceivedData)
		### action on dropping something on our applet
	end
	
	def on_answer(answer)
		### action on answering a dialog
	end

	def on_answer_dialog(button, answer)
		### action on answering a dialog
	end

	def on_shortkey(cKey)
		### action on pressing one of the shortkeys we bound beforehand
	end
	
	def on_change_focus(bIsActive)
		### action when the window controlled by the applet takes or looses the focus
	end
	
	##################################
	### callbacks on the sub-icons ###
	##################################
	def on_click_sub_icon(iState, cIconID)
		### action on click on one of our sub-icons
	end
	
	###############################
	### callbacks on the applet ###
	###############################
	
	def start
		### action when the applet is started
	end
	
	def stop
		### action when the applet is terminated
	end
	
	def _on_stop
		self.stop()
		exit
	end
	
	def reload
		### called when our applet is reloaded (config parameters have changed)
	end
	
	def _on_reload bConfigHasChanged
		if bConfigHasChanged
			self._get_config()
			self.reload()
		end
	end
	
	def _get_config
		keyfile = ParseConfig.new(self.conf_file)
		self.get_config(keyfile)
	end
	
	def get_config keyfile
		### get our parameters from the key-file
	end
	
	def _connect_to_dock
		# get the icon object on the bus
		self.bus = DBus::SessionBus.instance
		applet_path = "/org/cairodock/CairoDock/#{self.applet_name}" # path where our object is stored on the bus
		applet_service = bus.service("org.cairodock.CairoDock")
		begin
			applet_object = applet_service.object(applet_path)
			applet_object.introspect
			applet_object.default_iface = 'org.cairodock.CairoDock.applet'
		rescue
			puts ">>> object '#{self.applet_name}' can't be found on the bus, exit.\nMake sure that the 'Dbus' plug-in is activated in Cairo-Dock"
			exit
		end
		self.icon = applet_object
		
		# get the sub-icons object on the bus
		applet_sub_icons_object = applet_service.object("#{applet_path}/sub_icons")
		applet_sub_icons_object.introspect
		applet_sub_icons_object.default_iface = 'org.cairodock.CairoDock.subapplet'
		self.sub_icons = applet_sub_icons_object
		
		# now connect to the signals
		self.icon.on_signal("on_click") do |iState|
			self.on_click iState
		end
		self.icon.on_signal("on_middle_click") do
			self.on_middle_click
		end
		self.icon.on_signal("on_build_menu") do
			self.on_build_menu
		end
		self.icon.on_signal("on_menu_select") do |iNumEntry|
			self.on_menu_select iNumEntry
		end
		self.icon.on_signal("on_scroll") do |bScrollUp|
			self.on_scroll bScrollUp
		end
		self.icon.on_signal("on_drop_data") do |cReceivedData|
			self.on_drop_data cReceivedData
		end
		self.icon.on_signal("on_answer") do |answer|
			self.on_answer answer
		end
		self.icon.on_signal("on_answer_dialog") do |button, answer|
			self.on_answer_dialog button, answer
		end
		self.icon.on_signal("on_shortkey") do |cKey|
			self.on_shortkey cKey
		end
		self.icon.on_signal("on_change_focus") do |bIsActive|
			self.on_change_focus bIsActive
		end
		
		self.icon.on_signal("on_stop_module") do
			self._on_stop
		end
		
		self.icon.on_signal("on_reload_module") do |bConfigHasChanged|
			self._on_reload bConfigHasChanged
		end
		
		self.sub_icons.on_signal("on_click_sub_icon") do |iState, sub_icon_id|
			self.on_click_sub_icon iState sub_icon_id
		end
	end
end
