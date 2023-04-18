## THIS IS A GENERATED FILE -- DO NOT EDIT
.configuro: .libraries,em4f linker.cmd package/cfg/multi_role_app_pem4f.oem4f

# To simplify configuro usage in makefiles:
#     o create a generic linker command file name 
#     o set modification times of compiler.opt* files to be greater than
#       or equal to the generated config header
#
linker.cmd: package/cfg/multi_role_app_pem4f.xdl
	$(SED) 's"^\"\(package/cfg/multi_role_app_pem4fcfg.cmd\)\"$""\"C:/Users/Weli/workspace_v10/IS006_OBDII_multi_role_CC13X2R1_LAUNCHXL_tirtos_ccs2_external_gps_module/Release/configPkg/\1\""' package/cfg/multi_role_app_pem4f.xdl > $@
	-$(SETDATE) -r:max package/cfg/multi_role_app_pem4f.h compiler.opt compiler.opt.defs
