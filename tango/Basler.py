############################################################################
# This file is part of LImA, a Library for Image Acquisition
#
# Copyright (C) : 2009-2022
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
from Lima import Core
from Lima import Basler as BaslerAcq
from Lima.Server import AttrHelper


class Basler(PyTango.LatestDeviceImpl):

    Core.DEB_CLASS(Core.DebModApplication, 'LimaCCDs')


#------------------------------------------------------------------
#    Device constructor
#------------------------------------------------------------------
    def __init__(self,*args) :
        PyTango.LatestDeviceImpl.__init__(self,*args)

        self.__TestImageSelector = {'TESTIMAGE_OFF': BaslerAcq.Camera.TestImage_Off,
                                    'TESTIMAGE_1': BaslerAcq.Camera.TestImage_1,
                                    'TESTIMAGE_2': BaslerAcq.Camera.TestImage_2,
                                    'TESTIMAGE_3': BaslerAcq.Camera.TestImage_3,
                                    'TESTIMAGE_4': BaslerAcq.Camera.TestImage_4,
                                    'TESTIMAGE_5': BaslerAcq.Camera.TestImage_5,
                                    'TESTIMAGE_6': BaslerAcq.Camera.TestImage_6,
                                    'TESTIMAGE_7': BaslerAcq.Camera.TestImage_7,
        }
        self.__Output1LineSource = {
            'LINESOURCE_OFF': BaslerAcq.Camera.Off,
            'LINESOURCE_EXPOSURE_ACTIVE': BaslerAcq.Camera.ExposureActive,
            'LINESOURCE_FRAME_TRIGGER_WAIT': BaslerAcq.Camera.FrameTriggerWait,
            'LINESOURCE_LINE_TRIGGER_WAIT': BaslerAcq.Camera.LineTriggerWait,
            'LINESOURCE_TIMER_ACTIVE': BaslerAcq.Camera.TimerActive,
            'LINESOURCE_USER_OUTPUT': BaslerAcq.Camera.UserOutput,
            'LINESOURCE_ACQUISITION_TRIGGER_WAIT': BaslerAcq.Camera.AcquisitionTriggerWait,
        }
        self.__Attribute2FunctionBase = {
        }
        
        self.init_device()

#------------------------------------------------------------------
#    Device destructor
#------------------------------------------------------------------
    def delete_device(self):
        pass

#------------------------------------------------------------------
#    Device initialization
#------------------------------------------------------------------
    @Core.DEB_MEMBER_FUNCT
    def init_device(self):
        self.set_state(PyTango.DevState.ON)
        self.get_device_properties(self.get_device_class())

#------------------------------------------------------------------
#    getAttrStringValueList command:
#
#    Description: return a list of authorized values if any
#    argout: DevVarStringArray
#------------------------------------------------------------------
    @Core.DEB_MEMBER_FUNCT
    def getAttrStringValueList(self, attr_name):
        #use AttrHelper
        return AttrHelper.get_attr_string_value_list(self, attr_name)
#==================================================================
#
#    Basler read/write attribute methods
#
#==================================================================
    def __getattr__(self,name) :
        #use AttrHelper
        return AttrHelper.get_attr_4u(self,name,_BaslerCam)


#==================================================================
#
#    BaslerClass class definition
#
#==================================================================
class BaslerClass(PyTango.DeviceClass):

    class_property_list = {}

    device_property_list = {
        # define one and only one of the following 4 properties:
        'camera_id':
        [PyTango.DevString,
         "Camera ID", None],
        'cam_ip_address':
        [PyTango.DevString,
         "Camera ip address",[]],
        'serial_number':
        [PyTango.DevString,
         "Camera serial number", None],
        'user_name':
        [PyTango.DevString,
         "Camera user name", None],
        'inter_packet_delay':
        [PyTango.DevLong,
         "Inter Packet Delay",0],
        'frame_transmission_delay':
        [PyTango.DevLong,
         "Frame Transmission Delay",0],
        'packet_size':
        [PyTango.DevLong,
         "Network packet size (MTU)",8000],
        'force_video_mode':
        [PyTango.DevBoolean,
         "For B/W camera force to color-video mode",False],
        'blank_image_for_missed':
        [PyTango.DevBoolean,
         "blank image when frame missed",False],
        }

    cmd_list = {
        'getAttrStringValueList':
        [[PyTango.DevString, "Attribute name"],
         [PyTango.DevVarStringArray, "Authorized String value list"]],
        }

    attr_list = {
        'statistics_total_buffer_count':
        [[PyTango.DevLong,
          PyTango.SCALAR,
          PyTango.READ],
         {
             'unit': 'N/A',
             'format': '',
             'description': 'total number of frame requested',
         }],        
        'statistics_failed_buffer_count':
        [[PyTango.DevLong,
          PyTango.SCALAR,
          PyTango.READ],
         {
             'unit': 'N/A',
             'format': '',
             'description': 'total number of failed frame',
         }],        
        'test_image_selector':
        [[PyTango.DevString,
          PyTango.SCALAR,
          PyTango.READ_WRITE],
         {
             'unit': 'N/A',
             'format': '',
             'description': 'select a test image image_off/image_1/.../image_7',
         }],
        'output1_line_source':
        [[PyTango.DevString,
          PyTango.SCALAR,
          PyTango.READ_WRITE],
         {
             'unit': 'N/A',
             'format': '',
             'description': 'select a source for I/O output1 line',
         }],
        'temperature':
        [[PyTango.DevFloat,
          PyTango.SCALAR,
          PyTango.READ],
         {
             'unit': 'Celsuis',
             'format': '',
             'description': 'camera core temperature',
         }],        
    }

    def __init__(self,name) :
        PyTango.DeviceClass.__init__(self,name)
        self.set_type(name)

#----------------------------------------------------------------------------
# Plugins
#----------------------------------------------------------------------------
_BaslerCam = None
_BaslerInterface = None

# packet_size = 8000 suppose the eth MTU is set at least to 8192 (Jumbo mode !)
# otherwise frame transfer can failed, the package size must but
# correspond to the MTU, see README file under Pylon-3.2.2 installation
# directory for for details about network optimization.

def get_control(frame_transmission_delay = 0, inter_packet_delay = 0,
                packet_size = 8000,force_video_mode= 'false',blank_image_for_missed = 'false',
                **keys) :
    global _BaslerCam
    global _BaslerInterface

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

    if _BaslerCam is None:
        _BaslerCam = BaslerAcq.Camera(camera_id, int(packet_size))
        _BaslerCam.setInterPacketDelay(int(inter_packet_delay))
        _BaslerCam.setFrameTransmissionDelay(int(frame_transmission_delay))
        _BaslerInterface = BaslerAcq.Interface(_BaslerCam, force)

    if blank_image_for_missed == 'true':
        _BaslerInterface.setBlankImageForMissed(True)
        
    return Core.CtControl(_BaslerInterface)

def get_tango_specific_class_n_device():
    return BaslerClass,Basler
