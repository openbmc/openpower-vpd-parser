#VPD KEYWORD EDITOR
VPD Keyword editor is an implementation of the 
xyz.openbmc_project.Inventory.VPD.VPDKeywordEditor DBus interface.
It uses a combination of build-time YAML files, run-time calls to the WriteKeyword
method of the VPDKeywordEditor interface.

###build
**To build the service execute the following commands**
-Launch The SDK
-meson <directory>, direcory to contain the exe.This command will generate config file having BUSNAME,OBJPATH,INTERFACE value.
-ninja -C <directory_path>. This command performs the compilation and linking and place .exe in the directory_path.

eg:
-**meson builddir**
 - *This will now have config.h*

-**ninja -C builddir**
 - *If it succeeds builddir will now have the exe.*

#execution in simics
- Launch simics.
- Run the simulator.
- Copy the exe to a path in the simulator system.
- Launch the exe. This will start the service.
- From client launch the following command to execute the method over dbus
  - *This is a test command with dummy data*
  - busctl call xyz.openbmc_project.Inventory.VPD.VPDKeywordEditor /xyz/openbmc_project/Inventory/VPD xyz.openbmc_project.Inventory.VPD.VPDKeywordEditor WriteKeyword sssay "test" "test" "test" 3 "1" "2" "3"
  - On server side you can see the output of the method on a successfull call


