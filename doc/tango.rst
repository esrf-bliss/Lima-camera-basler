.. _lima-tango-basler:

Basler Tango device
=====================

This is the reference documentation of the Basler Tango device.

you can also find some useful information about the camera models/prerequisite/installation/configuration/compilation in the :ref:`Basler camera plugin <camera-basler>` section.

Properties
----------

======================== =============== ================================= =====================================
Property name	         Mandatory	 Default value	                   Description
======================== =============== ================================= =====================================
camera_id                No              uname://*<server instance name>*  The camera ID (see details below)
packet_size              No              8000                              the packet size
inter_packet_delay       No              0                                 The inter packet delay
frame_transmission_delay No              0                                 The frame transmission delay
======================== =============== ================================= =====================================

*camera_id* property identifies the camera in the network. Several types of ID might be given:

* IP/hostname (examples: `ip://192.168.5.2`, `ip://white_beam_viewer1.esrf.fr`)
* Basler serial number (example: `sn://12345678`)
* Basler user name (example: `uname://white_beam_viewer1`)

If no *camera_id* is given, it uses the server instance name as the camera user name (example, if your server is 
called `LimaCCDs/white_beam_viewer1`, the default value for *camera_id* will be `uname://white_beam_viewer1`).

To maintain backward compatibility, the old *cam_ip_address* is still supported but is considered deprecated
and might disappear in the future.

Both inter_packet_delay and frame_tranmission_delay properties can be used to tune the GiGE performance, for
more information on how to configure a GiGE Basler camera please refer to the Basler documentation.

Attributes
----------
============================== ======= ======================= ============================================================
Attribute name		       RW      Type                    Description
============================== ======= ======================= ============================================================
statistics_total_buffer_count  rw      DevLong                 Total number of requested frames
statistics_failed_buffer_count rw      DevLong                 Total number of failed frames
test_image_selector            rw      DevString               Select a test image: image_off/image_1/.../image_7 **(\*)**
output1_line_source            rw      DevString               Select a source for I/O output1 line **(\*)**
user_output_lin1               rw      DevBoolean              Switch on/off UserOuput on output1 line **(\*)**
============================== ======= ======================= ============================================================

**(\*)** Use the command getAttrStringValueList to get the list of the supported value for these attributes. 


Commands
--------

=======================	=============== =======================	===========================================
Command name		Arg. in		Arg. out		Description
=======================	=============== =======================	===========================================
Init			DevVoid 	DevVoid			Do not use
State			DevVoid		DevLong			Return the device state
Status			DevVoid		DevString		Return the device state as a string
getAttrStringValueList	DevString:	DevVarStringArray:	Return the authorized string value list for
			Attribute name	String value list	a given attribute name
=======================	=============== =======================	===========================================


