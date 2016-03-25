#   Razer device QT configuration tool
#
#   Copyright (C) 2007-2014 Michael Buesch <m@bues.ch>
#
#   This program is free software; you can redistribute it and/or
#   modify it under the terms of the GNU General Public License
#   as published by the Free Software Foundation; either version 2
#   of the License, or (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.

import sys
from PySide.QtCore import *
from PySide.QtGui import *
from functools import partial
from pyrazer import *

try:
	PYRAZER_SETUP_PY == True
except:
	print("ERROR: Found an old 'pyrazer' module.")
	print("You should uninstall razercfg from the system (see README)")
	print("and re-install it properly.")
	sys.exit(1)

razer = None

class Wrapper(object):
	def __init__(self, obj):
		self.obj = obj

	def __eq__(self, other):
		return self.obj == other.obj

	def __ne__(self, other):
		return self.obj != other.obj

class WrappedComboBox(QComboBox):
	def addItem(self, text, dataObj=None):
		QComboBox.addItem(self, text, Wrapper(dataObj))

	def itemData(self, index):
		return QComboBox.itemData(self, index).obj

	def findData(self, dataObj):
		for i in range(0, self.count()):
			if self.itemData(i) == dataObj:
				return i
		return -1

class OneButtonConfig(QWidget):
	def __init__(self, id, name, supportedFunctions, buttonConfWidget):
		QWidget.__init__(self, buttonConfWidget)
		self.setContentsMargins(QMargins())
		self.buttonConfWidget = buttonConfWidget

		self.id = id
		self.name = name

		self.setLayout(QHBoxLayout(self))
		self.layout().setContentsMargins(QMargins())

		self.nameLabel = QLabel(name, self)
		self.layout().addWidget(self.nameLabel)
		self.layout().addStretch()
		l = QLabel(self.tr("button is assigned to function"), self)
		self.layout().addWidget(l)

		self.funcCombo = WrappedComboBox(self)
		for func in supportedFunctions:
			self.funcCombo.addItem(func[1], func[0])
		curFunc = razer.getButtonFunction(buttonConfWidget.profileWidget.mouseWidget.mouse,
						  buttonConfWidget.profileWidget.profileId,
						  id)
		self.initialFunc = curFunc[0]
		index = self.funcCombo.findData(curFunc[0])
		if index >= 0:
			self.funcCombo.setCurrentIndex(index)
		self.layout().addWidget(self.funcCombo)

	def getId(self):
		return self.id

	def getSelectedFunction(self):
		index = self.funcCombo.currentIndex()
		return self.funcCombo.itemData(index)

	def getInitialFunction(self):
		return self.initialFunc

class ButtonConfDialog(QDialog):
	def __init__(self, profileWidget):
		QDialog.__init__(self, profileWidget)
		self.profileWidget = profileWidget
		self.setWindowTitle(self.tr("Configure buttons"))

		self.setLayout(QVBoxLayout(self))

		h = QHBoxLayout()
		l = QLabel(self.tr("Physical button"), self)
		h.addWidget(l)
		h.addStretch()
		l = QLabel(self.tr("Assigned function"), self)
		h.addWidget(l)
		self.layout().addLayout(h)

		funcs = razer.getSupportedButtonFunctions(profileWidget.mouseWidget.mouse)
		self.buttons = []
		for b in razer.getSupportedButtons(profileWidget.mouseWidget.mouse):
			button = OneButtonConfig(b[0], b[1], funcs, self)
			self.layout().addWidget(button)
			self.buttons.append(button)

		h = QHBoxLayout()
		self.applyButton = QPushButton(self.tr("Apply"), self)
		self.connect(self.applyButton, SIGNAL("clicked()"), self.apply)
		h.addWidget(self.applyButton)
		self.cancelButton = QPushButton(self.tr("Cancel"), self)
		self.connect(self.cancelButton, SIGNAL("clicked()"), self.cancel)
		h.addWidget(self.cancelButton)
		self.layout().addLayout(h)

	def cancel(self):
		self.done(0)

	def apply(self):
		for button in self.buttons:
			func = button.getSelectedFunction()
			if func != button.getInitialFunction():
				razer.setButtonFunction(self.profileWidget.mouseWidget.mouse,
							self.profileWidget.profileId,
							button.getId(),
							func)
		self.done(1)

class OneLedConfig(QWidget):
	def __init__(self, ledsWidget, led):
		QWidget.__init__(self, ledsWidget)
		self.setContentsMargins(QMargins())
		self.ledsWidget = ledsWidget
		self.led = led

		self.setLayout(QGridLayout(self))
		self.layout().setContentsMargins(QMargins())

		self.stateCb = QCheckBox(led.name + " LED", self)
		self.layout().addWidget(self.stateCb, 0, 0)
		self.stateCb.setCheckState(Qt.Checked if led.state else Qt.Unchecked)

		if led.supported_modes:
			self.modeComBox = WrappedComboBox(self)
			self.layout().addWidget(self.modeComBox, 0, 1)
			for mode in led.supported_modes:
				self.modeComBox.addItem(mode.toString(), mode.val)

			self.connect(self.modeComBox, SIGNAL("currentIndexChanged(int)"),
					self.modeChanged)

			index = self.modeComBox.findData(led.mode.val)
			if index >= 0:
				self.modeComBox.setCurrentIndex(index)


		if led.color is not None and led.canChangeColor:
			self.colorPb = QPushButton(self.tr("change color..."), self)
			self.layout().addWidget(self.colorPb, 0, 2)

			self.connect(self.colorPb, SIGNAL("released()"),
				     self.colorChangePressed)

		self.connect(self.stateCb, SIGNAL("stateChanged(int)"),
			     self.toggled)

	def toggled(self, state):
		self.led.state = bool(state)
		razer.setLed(self.ledsWidget.mouseWidget.mouse, self.led)

	def colorChangePressed(self):
		c = QColor(self.led.color.r, self.led.color.g, self.led.color.b)
		c = QColorDialog.getColor(c, self, self.led.name + self.tr(" color"))
		if not c.isValid():
			return
		self.led.color.r = c.red()
		self.led.color.g = c.green()
		self.led.color.b = c.blue()
		razer.setLed(self.ledsWidget.mouseWidget.mouse, self.led)

	def modeChanged(self, currentIndex):
		self.led.mode.val = self.modeComBox.itemData(currentIndex)
		razer.setLed(self.ledsWidget.mouseWidget.mouse, self.led)

class LedsWidget(QGroupBox):
	def __init__(self, parent, mouseWidget):
		QGroupBox.__init__(self, "LEDs", parent)
		self.mouseWidget = mouseWidget

		self.setLayout(QVBoxLayout(self))
		self.leds = []

	def clear(self):
		for led in self.leds:
			led.deleteLater()
		self.leds = []
		self.setEnabled(False)
		self.hide()

	def add(self, led):
		oneLed = OneLedConfig(self, led)
		self.layout().addWidget(oneLed)
		self.leds.append(oneLed)
		self.setEnabled(True)
		self.show()

	def updateContent(self, profileId=Razer.PROFILE_INVALID):
		for led in razer.getLeds(self.mouseWidget.mouse, profileId):
			self.add(led)
			self.show()

#TODO profile name
class MouseProfileWidget(QWidget):
	def __init__(self, mouseWidget, profileId):
		QWidget.__init__(self, mouseWidget)
		self.mouseWidget = mouseWidget
		self.profileId = profileId

		minfo = razer.getMouseInfo(mouseWidget.mouse)

		self.setLayout(QGridLayout(self))
		yoff = 0

		self.profileActive = QRadioButton(self.tr("Profile active"), self)
		self.connect(self.profileActive, SIGNAL("toggled(bool)"), self.activeChanged)
		self.layout().addWidget(self.profileActive, yoff, 0)
		yoff += 1

		self.freqSel = None
		if minfo & Razer.MOUSEINFOFLG_PROFILE_FREQ:
			self.freqSel = MouseScanFreqWidget(self, mouseWidget, profileId)
			self.layout().addWidget(self.freqSel, yoff, 0, 1, 2)
			yoff += 1

		self.resSel = []
		axes = self.__getIndependentAxes()
		for axis in axes:
			axisName = axis[1] + " " if axis[1] else ""
			self.layout().addWidget(QLabel(self.tr("%sScan resolution:" % axisName), self), yoff, 0)
			resSel = WrappedComboBox(self)
			self.connect(resSel, SIGNAL("currentIndexChanged(int)"), self.resChanged)
			self.layout().addWidget(resSel, yoff, 1)
			self.resSel.append(resSel)
			yoff += 1
		self.resIndependent = QCheckBox(self.tr("Independent resolutions"), self)
		self.connect(self.resIndependent, SIGNAL("stateChanged(int)"), self.resIndependentChanged)
		self.layout().addWidget(self.resIndependent, yoff, 1)
		yoff += 1
		if len(axes) <= 1:
			self.resIndependent.hide()

		funcs = razer.getSupportedButtonFunctions(self.mouseWidget.mouse)
		if funcs:
			self.buttonConf = QPushButton(self.tr("Configure buttons"), self)
			self.connect(self.buttonConf, SIGNAL("clicked(bool)"), self.showButtonConf)
			self.layout().addWidget(self.buttonConf, yoff, 0, 1, 2)
			yoff += 1

		if minfo & Razer.MOUSEINFOFLG_PROFNAMEMUTABLE:
			self.buttonName = QPushButton(self.tr("Change profile name"), self)
			self.connect(self.buttonName, SIGNAL("clicked(bool)"), self.nameChange)
			self.layout().addWidget(self.buttonName, yoff, 0, 1, 2)
			yoff += 1

		self.dpimappings = MouseDpiMappingsWidget(self, mouseWidget)
		self.layout().addWidget(self.dpimappings, yoff, 0, 1, 2)
		yoff += 1

		self.leds = LedsWidget(self, mouseWidget)
		self.layout().addWidget(self.leds, yoff, 0, 1, 2)
		yoff += 1

	def __getIndependentAxes(self):
		axes = razer.getSupportedAxes(self.mouseWidget.mouse)
		axes = [axis for axis in axes if (axis[2] & Razer.RAZER_AXIS_INDEPENDENT_DPIMAPPING)]
		if not axes:
			axes = [ (0, "", 0) ]
		return axes

	def reload(self):
		# Refetch the data from the daemon
		self.mouseWidget.recurseProtect += 1

		# Frequency selection (if any)
		if self.freqSel:
			self.freqSel.updateContent()

		# Resolution selection
		for resSel in self.resSel:
			resSel.clear()
		supportedMappings = razer.getSupportedDpiMappings(self.mouseWidget.mouse)
		supportedMappings = [m for m in supportedMappings if (m.profileMask == 0) or\
						     (m.profileMask & (1 << self.profileId))]
		axisMappings = []
		for axis in self.__getIndependentAxes():
			axisMappings.append(razer.getDpiMapping(self.mouseWidget.mouse,
								self.profileId,
								axis[0]))
		for i, resSel in enumerate(self.resSel):
			resSel.addItem(self.tr("Unknown mapping"), 0xFFFFFFFF)
			for mapping in supportedMappings:
				r = [ r for r in mapping.res if r is not None ]
				r = [ ("%u" % x) if x else self.tr("Unknown") for x in r]
				rStr = "/".join(r)
				resSel.addItem(self.tr("Scan resolution %u   (%s DPI)" %\
						(mapping.id + 1, rStr)),
						mapping.id)
			index = resSel.findData(axisMappings[i])
			if index >= 0:
				resSel.setCurrentIndex(index)
		independent = bool([x for x in axisMappings if x != axisMappings[0]])
		if independent:
			self.resIndependent.setCheckState(Qt.Checked)
		else:
			self.resIndependent.setCheckState(Qt.Unchecked)
		self.resIndependentChanged(self.resIndependent.checkState())

		# Profile selection
		activeProf = razer.getActiveProfile(self.mouseWidget.mouse)
		self.profileActive.setChecked(activeProf == self.profileId)

		# Per-profile DPI mappings (if any)
		self.dpimappings.clear()
		self.dpimappings.updateContent(self.profileId)

		# Per-profile LEDs (if any)
		self.leds.clear()
		self.leds.updateContent(self.profileId)

		self.mouseWidget.recurseProtect -= 1

	def activeChanged(self, checked):
		if self.mouseWidget.recurseProtect:
			return
		if not checked:
			# Cannot disable
			self.mouseWidget.recurseProtect += 1
			self.profileActive.setChecked(1)
			self.mouseWidget.recurseProtect -= 1
			return
		razer.setActiveProfile(self.mouseWidget.mouse, self.profileId)
		self.mouseWidget.reloadProfiles()

	def resChanged(self, unused=None):
		if self.mouseWidget.recurseProtect:
			return
		if self.resIndependent.checkState() == Qt.Checked:
			axisId = 0
			for resSel in self.resSel:
				index = resSel.currentIndex()
				res = resSel.itemData(index)
				razer.setDpiMapping(self.mouseWidget.mouse, self.profileId,
						    res, axisId)
				axisId += 1
		else:
			index = self.resSel[0].currentIndex()
			res = self.resSel[0].itemData(index)
			razer.setDpiMapping(self.mouseWidget.mouse, self.profileId, res)
			for resSel in self.resSel[1:]:
				self.mouseWidget.recurseProtect += 1
				resSel.setCurrentIndex(index)
				self.mouseWidget.recurseProtect -= 1

	def resIndependentChanged(self, newState):
		if newState == Qt.Checked:
			for resSel in self.resSel[1:]:
				resSel.setEnabled(True)
		else:
			for resSel in self.resSel[1:]:
				resSel.setEnabled(False)
		self.resChanged()

	def showButtonConf(self, checked):
		bconf = ButtonConfDialog(self)
		bconf.exec_()

	def nameChange(self, unused):
		name = razer.getProfileName(self.mouseWidget.mouse, self.profileId)
		(newName, ok) = QInputDialog.getText(self, self.tr("New profile name"),
						     self.tr("New profile name:"),
						     QLineEdit.Normal,
						     name)
		if not ok:
			return
		razer.setProfileName(self.mouseWidget.mouse, self.profileId, newName)
		self.mouseWidget.reloadProfiles()

class OneDpiMapping(QWidget):
	def __init__(self, dpiMappingsWidget, dpimapping):
		QWidget.__init__(self, dpiMappingsWidget)
		self.setContentsMargins(QMargins())
		self.dpiMappingsWidget = dpiMappingsWidget
		self.dpimapping = dpimapping

		self.setLayout(QHBoxLayout(self))
		self.layout().setContentsMargins(QMargins())

		self.layout().addWidget(QLabel(self.tr("Scan resolution %u:" % (dpimapping.id + 1)),
					self))
		supportedRes = razer.getSupportedRes(self.dpiMappingsWidget.mouseWidget.mouse)
		haveMultipleDims = len([r for r in dpimapping.res if r is not None]) > 1
		dimNames = ( "X", "Y", "Z" )
		changeSlots = ( self.changedDim0, self.changedDim1, self.changedDim2 )
		for dimIdx, thisRes in enumerate([r for r in dpimapping.res if r is not None]):
			self.mappingSel = WrappedComboBox(self)
			name = self.tr("Unknown DPI")
			if haveMultipleDims:
				name = dimNames[dimIdx] + ": " + name
			self.mappingSel.addItem(name, 0)
			for res in supportedRes:
				name = self.tr("%u DPI" % res)
				if haveMultipleDims:
					name = dimNames[dimIdx] + ": " + name
				self.mappingSel.addItem(name, res)
			index = self.mappingSel.findData(thisRes)
			if index >= 0:
				self.mappingSel.setCurrentIndex(index)
			self.connect(self.mappingSel, SIGNAL("currentIndexChanged(int)"),
				     changeSlots[dimIdx])
			self.mappingSel.setEnabled(dpimapping.mutable)
			self.layout().addWidget(self.mappingSel)
		self.layout().addStretch()

	def changedDim0(self, index):
		self.changed(index, 0)

	def changedDim1(self, index):
		self.changed(index, 1)

	def changedDim2(self, index):
		self.changed(index, 2)

	def changed(self, index, dimIdx):
		if index <= 0:
			return
		resolution = self.mappingSel.itemData(index)
		razer.changeDpiMapping(self.dpiMappingsWidget.mouseWidget.mouse,
				       self.dpimapping.id,
				       dimIdx,
				       resolution)
		self.dpiMappingsWidget.mouseWidget.reloadProfiles()

class MouseDpiMappingsWidget(QGroupBox):
	def __init__(self, parent, mouseWidget):
		QGroupBox.__init__(self, parent.tr("Possible scan resolutions"), parent)
		self.mouseWidget = mouseWidget

		self.setLayout(QVBoxLayout(self))
		self.mappings = []
		self.clear()

	def clear(self):
		for mapping in self.mappings:
			mapping.deleteLater()
		self.mappings = []
		self.setEnabled(False)
		self.hide()

	def add(self, dpimapping):
		mapping = OneDpiMapping(self, dpimapping)
		self.mappings.append(mapping)
		self.layout().addWidget(mapping)
		if dpimapping.mutable:
			self.setEnabled(True)
			self.show()

	def updateContent(self, profileId=Razer.PROFILE_INVALID):
		for dpimapping in razer.getSupportedDpiMappings(self.mouseWidget.mouse):
			if (profileId == Razer.PROFILE_INVALID and dpimapping.profileMask == 0) or\
			   (profileId != Razer.PROFILE_INVALID and dpimapping.profileMask & (1 << profileId)):
				self.add(dpimapping)

class MouseScanFreqWidget(QWidget):
	def __init__(self, parent, mouseWidget, profileId=Razer.PROFILE_INVALID):
		QWidget.__init__(self, parent)
		self.setContentsMargins(QMargins())
		self.mouseWidget = mouseWidget
		self.profileId = profileId

		self.setLayout(QGridLayout(self))
		self.layout().setContentsMargins(QMargins())

		self.layout().addWidget(QLabel(self.tr("Scan frequency:"), self), 0, 0)
		self.freqSel = WrappedComboBox(self)
		self.layout().addWidget(self.freqSel, 0, 1)

		self.connect(self.freqSel, SIGNAL("currentIndexChanged(int)"), self.freqChanged)

	def freqChanged(self, index):
		if self.mouseWidget.recurseProtect or index <= 0:
			return
		freq = self.freqSel.itemData(index)
		razer.setFrequency(self.mouseWidget.mouse, self.profileId, freq)

	def updateContent(self):
		self.mouseWidget.recurseProtect += 1

		self.freqSel.clear()
		supportedFreqs = razer.getSupportedFreqs(self.mouseWidget.mouse)
		curFreq = razer.getCurrentFreq(self.mouseWidget.mouse, self.profileId)
		self.freqSel.addItem(self.tr("Unknown Hz"), 0)
		for freq in supportedFreqs:
			self.freqSel.addItem(self.tr("%u Hz" % freq), freq)
		index = self.freqSel.findData(curFreq)
		if index >= 0:
			self.freqSel.setCurrentIndex(index)

		self.mouseWidget.recurseProtect -= 1

class MouseWidget(QWidget):
	def __init__(self, parent=None):
		QWidget.__init__(self, parent)
		self.recurseProtect = 0

		self.mainwnd = parent

		self.setLayout(QVBoxLayout(self))

		self.mousesel = WrappedComboBox(self)
		self.connect(self.mousesel, SIGNAL("currentIndexChanged(int)"), self.mouseChanged)
		self.layout().addWidget(self.mousesel)
		self.layout().addSpacing(15)

		self.profiletab = QTabWidget(self)
		self.layout().addWidget(self.profiletab)
		self.profileWidgets = []

		self.freqSel = MouseScanFreqWidget(self, self)
		self.layout().addWidget(self.freqSel)

		self.dpimappings = MouseDpiMappingsWidget(self, self)
		self.layout().addWidget(self.dpimappings)

		self.leds = LedsWidget(self, self)
		self.layout().addWidget(self.leds)

		self.layout().addStretch()
		self.fwVer = QLabel(self)
		self.layout().addWidget(self.fwVer)

	def updateContent(self, mice):
		self.mice = mice
		self.mousesel.clear()
		for mouse in mice:
			id = RazerDevId(mouse)
			self.mousesel.addItem("%s   %s-%s %s" % \
				(id.getDevName(), id.getBusType(),
				 id.getBusPosition(), id.getDevId()))

	def mouseChanged(self, index):
		self.profiletab.clear()
		self.profileWidgets = []
		self.dpimappings.clear()
		self.leds.clear()
		self.profiletab.setEnabled(index > -1)
		if index == -1:
			self.fwVer.clear()
			return
		self.mouse = self.mice[index]

		minfo = razer.getMouseInfo(self.mouse)

		profileIds = razer.getProfiles(self.mouse)
		activeProfileId = razer.getActiveProfile(self.mouse)
		activeWidget = None
		for profileId in profileIds:
			widget = MouseProfileWidget(self, profileId)
			if profileId == activeProfileId:
				activeWidget = widget
			self.profiletab.addTab(widget, str(profileId + 1))
			self.profileWidgets.append(widget)
		self.reloadProfiles()
		if activeWidget:
			self.profiletab.setCurrentWidget(activeWidget)

		# Update global frequency selection (if any)
		if minfo & Razer.MOUSEINFOFLG_GLOBAL_FREQ:
			self.freqSel.updateContent()
			self.freqSel.show()
		else:
			self.freqSel.hide()

		# Update global DPI mappings (if any)
		self.dpimappings.updateContent()

		# Update global LEDs (if any)
		self.leds.updateContent()

		ver = razer.getFwVer(self.mouse)
		if (ver[0] == 0xFF and ver[1] == 0xFF) or\
		   (ver[0] == 0 and ver[1] == 0):
			self.fwVer.hide()
		else:
			extra = ""
			if minfo & Razer.MOUSEINFOFLG_SUGGESTFWUP:
				extra = self.tr("\nDue to known bugs in this firmware version, a "
					"firmware update is STRONGLY suggested!")
			self.fwVer.setText(self.tr("Firmware version: %u.%02u%s" % (ver[0], ver[1], extra)))
			self.fwVer.show()

	def reloadProfiles(self):
		for prof in self.profileWidgets:
			prof.reload()
		activeProf = razer.getActiveProfile(self.mouse)
		for i in range(0, self.profiletab.count()):
			profileId = self.profiletab.widget(i).profileId
			name = razer.getProfileName(self.mouse, profileId)
			if activeProf == profileId:
				name = ">" + name + "<"
			self.profiletab.setTabText(i, name)

class StatusBar(QStatusBar):
	def showMessage(self, msg):
		QStatusBar.showMessage(self, msg, 10000)

class MainWindow(QMainWindow):
	def __init__(self, parent = None, enableNotificationPolling = True):
		QMainWindow.__init__(self, parent)
		self.setWindowTitle(self.tr("Razer device configuration"))

		mb = QMenuBar(self)
		rzrmen = QMenu(self.tr("Razer"), mb)
		rzrmen.addAction(self.tr("Scan system for devices"), self.scan)
		rzrmen.addAction(self.tr("Re-apply all hardware settings"), self.reconfig)
		rzrmen.addSeparator()
		rzrmen.addAction(self.tr("Exit"), self.close)
		mb.addMenu(rzrmen)
		helpmen = QMenu(self.tr("Help"), mb)
		helpmen.addAction(self.tr("About"), self.about)
		mb.addMenu(helpmen)
		self.setMenuBar(mb)

		tab = QTabWidget(self)
		self.mousewidget = MouseWidget(self)
		tab.addTab(self.mousewidget, self.tr("Mice"))
		self.setCentralWidget(tab)

		self.setStatusBar(StatusBar())

		self.mice = []
		self.scan()
		if enableNotificationPolling:
			self.pokeNotificationTimer()

	def pokeNotificationTimer(self):
		QTimer.singleShot(300, self.pollNotifications)

	def pollNotifications(self):
		n = razer.pollNotifications()
		if n:
			self.scan()
		self.pokeNotificationTimer()

	# Rescan for new devices
	def scan(self):
		razer.rescanMice()
		mice = razer.getMice()
		if len(mice) != len(self.mice):
			if (len(mice) == 1):
				self.statusBar().showMessage(self.tr("Found one Razer mouse"))
			elif (len(mice) > 1):
				self.statusBar().showMessage(self.tr("Found %d Razer mice" % len(mice)))
		self.mice = mice
		self.mousewidget.updateContent(mice)

	def reconfig(self):
		razer.rescanDevices()
		razer.reconfigureDevices()

	def about(self):
		QMessageBox.information(self, self.tr("About"),
					self.tr("Razer device configuration tool\n"
						"Version %s\n"
						"Copyright (c) 2007-2014 Michael Buesch"
						% RAZER_VERSION))

class AppletMainWindow(MainWindow):
	def __init__(self, parent=None):
		super().__init__(parent, enableNotificationPolling = False)

	def updateContent(self):
		self.mousewidget.reloadProfiles()

	def closeEvent(self, event):
		event.ignore()
		self.hide()

class RazerApplet(QSystemTrayIcon):
	def __init__(self):
		QSystemTrayIcon.__init__(self)

		icon = QIcon()
		icon.addFile('razercfg.png', QSize(16,16))
		icon.addFile('razercfg.png', QSize(24,24))
		icon.addFile('razercfg.png', QSize(48,48))

		self.setIcon(icon)

		self.menu = QMenu()
		self.mainwnd = AppletMainWindow()

		self.mice = razer.getMice();
		self.buildMenu()

		self.setContextMenu(self.menu)

		# set timer for mice update
		self.poke()

	def poke(self):
		#TODO completely stop the update, if it is hidden. Re-enable update on show event.
		t = 2000 if self.mainwnd.isHidden() else 300
		QTimer.singleShot(t, self.updateContent)

	def updateContent(self):
		mice = razer.getMice()

		if razer.pollNotifications() and self.mice != mice:
			self.mice = mice
			self.mainwnd.scan()
			self.buildMenu()

		# continue timer
		self.poke()

	def buildMenu(self):
		# clear the menu
		self.menu.clear()

		# add mices and profiles to menu
		for mouse in self.mice:
			mouse_id = RazerDevId(mouse)
			mouse_menu = self.menu.addMenu("&" + mouse_id.getDevName() + " mouse")
			self.getMouseProfiles(mouse, mouse_menu)

		if not self.mice:
			act = self.menu.addAction("No Razer devices found")
			act.setEnabled(False)

		# add buttons
		self.menu.addSeparator()
		mainwnd = self.mainwnd
		self.menu.addAction("&Open main window...", mainwnd.show)
		self.menu.addAction("&Exit", sys.exit)

	def selectProfile(self, mouse, profileId):
		razer.setActiveProfile(mouse, profileId)
		self.mainwnd.updateContent()

	def getMouseProfiles(self, mouse, mouse_menu):
		mouse_menu.clear()
		profileIds = razer.getProfiles(mouse)
		activeProfileId = razer.getActiveProfile(mouse)
		group = QActionGroup(self)
		for profileId in profileIds:
			name = razer.getProfileName(mouse, profileId)
			action = QAction(name, mouse_menu)
			action.setCheckable(True)
			action.triggered.connect( partial(self.selectProfile, mouse, profileId))
			group.addAction(action)
			mouse_menu.addAction(action)
			if profileId == activeProfileId:
				action.setChecked(True)
