############################################################################
# This file is part of LImA, a Library for Image Acquisition
#
# Copyright (C) : 2009-2026
# European Synchrotron Radiation Facility
# CS40220 38043 Grenoble Cedex 9
# FRANCE
#
# Contact: lima@esrf.fr
#
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.
############################################################################
#=============================================================================
#
# file :        Basler.py
#
# description : Python source for the Basler and its commands.
#                The class is derived from Device. It represents the
#                CORBA servant object which will be accessed from the
#                network. All commands which can be executed on the
#                Pilatus are implemented in this file.
#
# project :     TANGO Device Server
#
# copyleft :    European Synchrotron Radiation Facility
#               BP 220, Grenoble 38043
#               FRANCE
#
#=============================================================================
#         (c) - Bliss - ESRF
#=============================================================================
#
import PyTango
from tango import AttrWriteType, DevState
from tango.server import Device, attribute, command, device_property

from lima import core
from lima import basler as BaslerAcq
from lima.server import AttrHelper


class Basler(Device):

    core.DEB_CLASS(core.DebModule.DebModApplication, 'LimaCCDs')

    # ------------------------------------------------------------------
    #    Static properties
    # ------------------------------------------------------------------
    _BaslerCam = None
    """Reference to Lima basler camera binding"""
    _BaslerInterface = None
    """Reference to Lima basler interface binding"""

    _TestImageSelector = {
        'TESTIMAGE_OFF': BaslerAcq.Camera.TestImageSelector.TestImage_Off,
        'TESTIMAGE_1': BaslerAcq.Camera.TestImageSelector.TestImage_1,
        'TESTIMAGE_2': BaslerAcq.Camera.TestImageSelector.TestImage_2,
        'TESTIMAGE_3': BaslerAcq.Camera.TestImageSelector.TestImage_3,
        'TESTIMAGE_4': BaslerAcq.Camera.TestImageSelector.TestImage_4,
        'TESTIMAGE_5': BaslerAcq.Camera.TestImageSelector.TestImage_5,
        'TESTIMAGE_6': BaslerAcq.Camera.TestImageSelector.TestImage_6,
        'TESTIMAGE_7': BaslerAcq.Camera.TestImageSelector.TestImage_7,
    }
    _Output1LineSource = {
        'LINESOURCE_OFF': BaslerAcq.Camera.LineSource.Off,
        'LINESOURCE_EXPOSURE_ACTIVE': BaslerAcq.Camera.LineSource.ExposureActive,
        'LINESOURCE_FRAME_TRIGGER_WAIT': BaslerAcq.Camera.LineSource.FrameTriggerWait,
        'LINESOURCE_LINE_TRIGGER_WAIT': BaslerAcq.Camera.LineSource.LineTriggerWait,
        'LINESOURCE_TIMER_ACTIVE': BaslerAcq.Camera.LineSource.TimerActive,
        'LINESOURCE_USER_OUTPUT': BaslerAcq.Camera.LineSource.UserOutput,
        'LINESOURCE_ACQUISITION_TRIGGER_WAIT': BaslerAcq.Camera.LineSource.AcquisitionTriggerWait,
    }

    # ------------------------------------------------------------------
    #    Device properties
    # ------------------------------------------------------------------
    # define one and only one of the following 4 properties:
    camera_id = device_property(dtype=str, default_value="")
    cam_ip_address = device_property(dtype=str, default_value="")
    serial_number = device_property(dtype=str, default_value="")
    user_name = device_property(dtype=str, default_value="")
    inter_packet_delay = device_property(dtype=int, default_value=0)
    frame_transmission_delay = device_property(dtype=int, default_value=0)
    packet_size = device_property(dtype=int, default_value=8000)
    force_video_mode = device_property(dtype=bool, default_value=False)
    blank_image_for_missed = device_property(dtype=bool, default_value=False)

    # ------------------------------------------------------------------
    #    Device initialization
    # ------------------------------------------------------------------
    @core.DEB_MEMBER_FUNCT
    def init_device(self):
        Device.init_device(self)

        # Leagcy code used by AttrHelper.get_attr_string_value_list
        self.__TestImageSelector = self._TestImageSelector
        self.__Output1LineSource = self._Output1LineSource

        self.set_state(DevState.ON)

    # ------------------------------------------------------------------
    #    Commands
    # ------------------------------------------------------------------
    @command(dtype_in=str, dtype_out=(str,))
    @core.DEB_MEMBER_FUNCT
    def getAttrStringValueList(self, attr_name):
        return AttrHelper.get_attr_string_value_list(self, attr_name)

    # ------------------------------------------------------------------
    #    Attributes fully dispatched by naming convention
    # ------------------------------------------------------------------
    _statistics_total_buffer_count_fget, _ = AttrHelper.make_fget_fset(
        "statistics_total_buffer_count", lambda self: self._BaslerCam
    )
    statistics_total_buffer_count = attribute(
        dtype=int,
        access=AttrWriteType.READ,
        unit="N/A",
        doc="total number of frame requested",
        fget=_statistics_total_buffer_count_fget,
    )

    _statistics_failed_buffer_count_fget, _ = AttrHelper.make_fget_fset(
        "statistics_failed_buffer_count", lambda self: self._BaslerCam
    )
    statistics_failed_buffer_count = attribute(
        dtype=int,
        access=AttrWriteType.READ,
        unit="N/A",
        doc="total number of failed frame",
        fget=_statistics_failed_buffer_count_fget,
    )

    _test_image_selector_fget, _test_image_selector_fset = AttrHelper.make_fget_fset(
        "test_image_selector", lambda self: self._BaslerCam, enum=_TestImageSelector
    )
    test_image_selector = attribute(
        dtype=str,
        access=AttrWriteType.READ_WRITE,
        unit="N/A",
        doc="select a test image image_off/image_1/.../image_7",
        fget=_test_image_selector_fget,
        fset=_test_image_selector_fset,
    )

    _output1_line_source_fget, _output1_line_source_fset = AttrHelper.make_fget_fset(
        "output1_line_source", lambda self: self._BaslerCam, enum=_Output1LineSource
    )
    output1_line_source = attribute(
        dtype=str,
        access=AttrWriteType.READ_WRITE,
        unit="N/A",
        doc="select a source for I/O output1 line",
        fget=_output1_line_source_fget,
        fset=_output1_line_source_fset,
    )

    _temperature_fget, _ = AttrHelper.make_fget_fset(
        "temperature", lambda self: self._BaslerCam
    )
    temperature = attribute(
        dtype=float,
        access=AttrWriteType.READ,
        unit="Celsuis",
        doc="camera core temperature",
        fget=_temperature_fget,
    )

    _frame_rate_fget, _ = AttrHelper.make_fget_fset(
        "frame_rate", lambda self: self._BaslerCam
    )
    frame_rate = attribute(
        dtype=float,
        access=AttrWriteType.READ,
        unit="fps",
        doc=(
            "Maximum frame acquisition rate with in frames per second, givent the "
            "current the Roi/Bin, exposure and bandwidth settings."
        ),
        fget=_frame_rate_fget,
    )


#----------------------------------------------------------------------------
# Plugins
#----------------------------------------------------------------------------

# packet_size = 8000 suppose the eth MTU is set at least to 8192 (Jumbo mode !)
# otherwise frame transfer can failed, the package size must but
# correspond to the MTU, see README file under Pylon-3.2.2 installation
# directory for for details about network optimization.

def get_control(frame_transmission_delay=0, inter_packet_delay=0,
                 packet_size=8000, force_video_mode='false', blank_image_for_missed='false',
                 _Basler=Basler, _Camera=BaslerAcq.Camera, _Interface=BaslerAcq.Interface,
                 **keys):
    camera = _Basler._BaslerCam
    interface = _Basler._BaslerInterface

    if 'camera_id' in keys:
        camera_id = keys['camera_id']
    elif 'serial_number' in keys:
        camera_id = 'sn://' + keys['serial_number']
    elif 'cam_ip_address' in keys:
        camera_id = 'ip://' + keys['cam_ip_address']
    elif 'user_name' in keys:
        camera_id = 'uname://' + keys['user_name']
    else:
        # if no property is present it uses the server personal name
        # as Basler user name to identify the camera
        util = PyTango.Util.instance()
        camera_id = 'uname://' + util.get_ds_inst_name()

    print ("basler camera_id:", camera_id)

    # all properties are passed as string from LimaCCDs device get_control helper
    # so need to be converted to correct type
    if force_video_mode == 'true':
        force = True
    elif force_video_mode == 'false':
        force = False
    else:
        raise ValueError("Invalid force_video_mode value, expecting 'true' or ' false'")

    if camera is None:
        camera = _Camera(camera_id, int(packet_size))
        camera.setInterPacketDelay(int(inter_packet_delay))
        camera.setFrameTransmissionDelay(int(frame_transmission_delay))
        interface = _Interface(camera, force)

    if blank_image_for_missed == 'true':
        interface.setBlankImageForMissed(True)

    _Basler._BaslerCam = camera
    _Basler._BaslerInterface = interface

    return core.CtControl(interface)

def get_tango_specific_class_n_device():
    return Basler.TangoClassClass, Basler
