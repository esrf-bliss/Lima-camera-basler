Basler
-------

.. image:: basler.png

Intoduction
```````````
Basler's area scan cameras are designed for industrial users who demand superior image quality and an excellent price/performance ratio. You can choose from an area scan portfolio that includes monochrome or color models with various resolutions, frame rates, and sensor technologies.

The Lima module as been tested only with this GigE cameras models:
  - Scout
  - Pilot
  - Ace

And for now It's only work with monochrome cameras

Module configuration
````````````````````
Previously to this you have to install the Basler SDK *Pylon* to the default path (/opt/pylon)

Basler python module need at least the lima core module.

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


See :ref:`Compilation`

Installation
`````````````

- After installing basler modules :ref:`installation`

- And probably Tango server :ref:`tango_installation`


Configuration
``````````````

- First you have to setup ip addresse of the Basler Camera by using *IpConfigurator* (/opt/pylon/bin/IpConfigurator)

- Then in the Basler Tango device set the property *cam_ip_address* to the address previously set.

- If you are running the server with linux kernel >= 2.6.13, you should add this line into *etc/security/limits.conf*. With this line, the acquisition thread will be in real time mode.

.. code-block:: sh

  USER_RUNNING_DEVICE_SERVER	-	rtprio	99
