SKIPUNZIP=0
api=$(getprop ro.build.version.sdk)
fp=$(getprop ro.system.build.fingerprint)
ui_print "- SDK $api, $fp"
if [ "$api" != "32" ]; then
  ui_print "! built for Android 12L (SDK 32); the library will fall back to the stock policy"
fi
set_perm_recursive $MODPATH/system 0 0 0755 0644 u:object_r:system_file:s0
set_perm $MODPATH/system/lib64/libaudiopolicymanagercustom.so 0 0 0644 u:object_r:system_lib_file:s0
