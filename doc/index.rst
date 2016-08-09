.. _camera-basler:

Basler camera
--------------

.. image:: basler.png

Intoduction
```````````
Basler's area scan cameras are designed for industrial users who demand superior image quality and an excellent price/performance ratio. You can choose from an area scan portfolio that includes monochrome or color models with various resolutions, frame rates, and sensor technologies.

The Lima module as been tested only with this GigE cameras models:
  - Scout
  - Pilot
  - Ace

Monochrome and color cameras are supported, using the **SDK 3.2.2**.

Installation & Module configuration
````````````````````````````````````
Previously to this you have to install the Basler SDK *Pylon* to the default path (/opt/pylon). 

-  follow the steps for the linux installation :ref:`linux_installation`
-  follow the steps for the windows installation :ref:`windows_installation`

The minimum configuration file is *config.inc* :

.. code-block:: sh

  COMPILE_CORE=1
  COMPILE_SIMULATOR=0
  COMPILE_SPS_IMAGE=1
  COMPILE_ESPIA=0
  COMPILE_FRELON=0
  COMPILE_MAXIPIX=0
  COMPILE_PILATUS=0
  COMPILE_BASLER=1
  COMPILE_CBF_SAVING=0
  export COMPILE_CORE COMPILE_SPS_IMAGE COMPILE_SIMULATOR \
         COMPILE_ESPIA COMPILE_FRELON COMPILE_MAXIPIX COMPILE_PILATUS \
         COMPILE_BASLER COMPILE_CBF_SAVING


-  start the compilation :ref:`linux_compilation`

-  finally for the Tango server installation :ref:`tango_installation`

Initialisation and Capabilities
````````````````````````````````

Camera initialisation
......................

The camera will be initialized   by creating Basler ::Camera object.  The Camera contructor
sets the camera with default parameters, only the ip address or hostname of the camera is mandatory.

Std capabilites
................

This plugin has been implement in respect of the mandatory capabilites but with some limitations which
are due to the camera and SDK features. Only restriction on capabilites are documented here.

* HwDetInfo
  
  getCurrImageType/getDefImageType(): it can change if the video mode change (see HwVideo capability).

  setCurrImageType(): It only supports Bpp8 and Bpp16.

* HwSync

  get/setTrigMode(): the supported mode are IntTrig, IntTrigMult, ExtTrigMult and ExtGate.
  
Optional capabilites
........................
In addition to the standard capabilities, we make the choice to implement some optional capabilities which
are supported by the SDK. **Video**,  Roi and Binning are available.

* HwVideo

  The prosilica cameras are pure video device, so video format for image are supported:
   For color cameras, 

   - BAYER_RG8
   - BAYER_BG8
   - BAYER_RG16
   - BAYER_BG16
   - RGB24
   - BGR24
   - RGB32
   - BGR32
   - YUV411
   - YUV422
   - YUV444
   
   For color and monochrome cameras,

    - Y8   
    - Y16   

  Use get/setVideoMode() on video object for video format.

* HwBin 

  There is no restriction for the binning up to the maximum size.

* HwRoi 

  There is no restriction for the Roi up to the maximum size.


Configuration
``````````````

- First you have to setup ip addresse of the Basler Camera by using *IpConfigurator* (/opt/pylon/bin/IpConfigurator) or by matching the MAC address with a choosen IP into the DHCP.

- Then in the Basler Tango device set the property *cam_ip_address* to the address previously set.

- If you are running the server with linux kernel >= 2.6.13, you should add this line into */etc/security/limits.conf*. With this line, the acquisition thread will be in real time mode.

.. code-block:: sh

  USER_RUNNING_DEVICE_SERVER	-	rtprio	99


How to use
````````````
This is a python code example for a simple test:

.. code-block:: python

  from Lima import Basler
  from lima import Core

  #----------------------------------------+
  #                        packet-size     |
  #                                        |
  #-------------------------------------+  |
  #              inter-packet delay     |  |
  #                                     |  |
  #----------------------------------+  |  |
  #      frame-transmission delay    |  |  |
  #                                  |  |  |
  #--------------------+             |  |  |
  # cam ip or hostname |             |  |  |
  #                    v             v  v  v 
  cam = Basler.Camera('192.168.1.1', 0, 0, 8000)


  hwint = Basler.Interface(cam)
  ct = Core.CtControl(hwint)

  acq = ct.acquisition()


  # set and test video
  #

  video=ct.video()
  video.setMode(Core.RGB24)
  video.startLive()
  video.stopLive()
  video_img = video.getLastImage()

  # set and test an acquisition
  #

  # setting new file parameters and autosaving mode
  saving=ct.saving()

  pars=saving.getParameters()
  pars.directory='/buffer/lcb18012/opisg/test_lima'
  pars.prefix='test1_'
  pars.suffix='.edf'
  pars.fileFormat=Core.CtSaving.TIFF
  pars.savingMode=Core.CtSaving.AutoFrame
  saving.setParameters(pars)

  # now ask for 2 sec. exposure and 10 frames
  acq.setAcqExpoTime(2)
  acq.setNbImages(10) 
  
  ct.prepareAcq()
  ct.startAcq()

  # wait for last image (#9) ready
  lastimg = ct.getStatus().ImageCounters.LastImageReady
  while lastimg !=9:
    time.sleep(1)
    lastimg = ct.getStatus().ImageCounters.LastImageReady
 
  # read the first image
  im0 = ct.ReadImage(0)


