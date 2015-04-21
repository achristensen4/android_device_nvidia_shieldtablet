#!/sbin/sh
set -- `cat /proc/cmdline`
model="wx_na_wf"
for x in "$@"; do
	case "$x" in board_info=*)
		id=`echo "${x#board_info=}" | cut -c 8-13`
		echo "ID = $id"
		case $id in
			"0x00ea") model="wx_na_wf" ;;
			"0x04d2")
				if [ "$(cat /mnt/factory/wifi/country.txt)" = "US" ]; then
				  model="wx_na_do"
				else
				  model="wx_un_do"
				fi
				;;
			*) model="wx_un_mo" ;;
		esac
		;;
	esac
done
echo "Setting build properties for $model model"
setpropex ro.product.device shieldtablet
setpropex ro.product.model $model
