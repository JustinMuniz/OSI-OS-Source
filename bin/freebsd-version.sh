#!/bin/sh
set -e
USERLAND_VERSION="@@REVISION@@-@@BRANCH@@"
: ${ROOT:=}
: ${LOADER_DIR:=$ROOT/boot}
: ${LOADER_CONF_FILES:=$LOADER_DIR/defaults/loader.conf $LOADER_DIR/loader.conf $LOADER_DIR/loader.conf.local}
LOADER_RE1='^\([A-Z_a-z][0-9A-Z_a-z]*=[-./0-9A-Z_a-z]\{1,\}\).*$'
LOADER_RE2='^\([A-Z_a-z][0-9A-Z_a-z]*="[-./0-9A-Z_a-z]\{1,\}"\).*$'
KERNEL_RE='^@(#)@@TYPE@@ \([-.0-9A-Za-z]\{1,\}\) .*$'
progname=$(basename $0)
error() {
	echo "$progname: $*" >&2
	exit 1
}
kernel_file() {
	eval $(sed -n "s/$LOADER_RE1/\\1;/p; s/$LOADER_RE2/\\1;/p" \
	    $LOADER_CONF_FILES 2>/dev/null)
	echo "$LOADER_DIR/${kernel:-kernel}/${bootfile:-kernel}"
}
kernel_version() {
	kernfile=$(kernel_file)
	if [ ! -f "$kernfile" -o ! -r "$kernfile" ] ; then
		error "unable to locate kernel"
	fi
	strings "$kernfile" | sed -n "s/$KERNEL_RE/\\1/p"
}
userland_version() {
	echo $USERLAND_VERSION
}
usage() {
	echo "usage: $progname [-ku]" >&2
	exit 1
}
main() {
	while getopts "ku" option ; do
		case $option in
		k)
			opt_k=1
			;;
		u)
			opt_u=1
			;;
		*)
			usage
			;;
		esac
	done
	if [ $OPTIND -le $# ] ; then
		usage
	fi
	if [ $((opt_k + opt_u)) -eq 0 ] ; then
		opt_u=1
	fi
	if [ $opt_k ] ; then
		kernel_version
	fi
	if [ $opt_u ] ; then
		userland_version
	fi
}
main "$@"
