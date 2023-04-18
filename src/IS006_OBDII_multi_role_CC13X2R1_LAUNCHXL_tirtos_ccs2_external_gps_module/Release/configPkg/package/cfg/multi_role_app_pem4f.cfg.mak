# invoke SourceDir generated makefile for multi_role_app.pem4f
multi_role_app.pem4f: .libraries,multi_role_app.pem4f
.libraries,multi_role_app.pem4f: package/cfg/multi_role_app_pem4f.xdl
	$(MAKE) -f C:\Users\Weli\workspace_v10\IS006_OBDII_multi_role_CC13X2R1_LAUNCHXL_tirtos_ccs2_external_gps_module/src/makefile.libs

clean::
	$(MAKE) -f C:\Users\Weli\workspace_v10\IS006_OBDII_multi_role_CC13X2R1_LAUNCHXL_tirtos_ccs2_external_gps_module/src/makefile.libs clean

