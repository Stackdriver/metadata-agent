dnl Process this file with autoconf to produce a configure script.
AC_PREREQ([2.60])
AC_INIT([metadata-agent],[m4_esyscmd(./version-gen.sh)])
AC_CONFIG_SRCDIR(src/target_set.c)
AC_CONFIG_HEADERS(src/config.h)
# AC_CONFIG_AUX_DIR([libltdl/config])
# AC_CONFIG_SUBDIRS([src/libmongoc])

dnl older automake's default of ARFLAGS=cru is noisy on newer binutils;
dnl we don't really need the 'u' even in older toolchains.  Then there is
dnl older libtool, which spelled it AR_FLAGS
m4_divert_text([DEFAULTS], [: "${ARFLAGS=cr} ${AR_FLAGS=cr}"])

m4_ifdef([LT_PACKAGE_VERSION],
	# libtool >= 2.2
	[
	 LT_CONFIG_LTDL_DIR([libltdl])
	 LT_INIT([dlopen])
	 LTDL_INIT([convenience])
	 AC_DEFINE(LIBTOOL_VERSION, 2, [Define to used libtool version.])
	]
,
	# libtool <= 1.5
	[
	 AC_LIBLTDL_CONVENIENCE
	 AC_SUBST(LTDLINCL)
	 AC_SUBST(LIBLTDL)
	 AC_LIBTOOL_DLOPEN
	 AC_CONFIG_SUBDIRS(libltdl)
	 AC_DEFINE(LIBTOOL_VERSION, 1, [Define to used libtool version.])
	]
)

AM_CONDITIONAL([BUILD_INCLUDED_LTDL], [test "x$LTDLDEPS" != "x"])

AM_INIT_AUTOMAKE([subdir-objects tar-pax dist-bzip2 no-dist-gzip foreign])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_LANG(C)

AC_PREFIX_DEFAULT("/opt/stackdriver/metadata")

AC_SYS_LARGEFILE

#
# Checks for programs.
#
AC_PROG_CC_C99([],
  [AC_MSG_ERROR([No compiler found that supports C99])]
)
AC_PROG_CXX
AC_PROG_CPP
AC_PROG_EGREP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PROG_CC_C_O
AM_CONDITIONAL(COMPILER_IS_GCC, test "x$GCC" = "xyes")

AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_PROG_LEX
AC_PROG_YACC

AC_PATH_PROG([VALGRIND], [valgrind])

# Warn when pkg.m4 is missing
m4_pattern_forbid([^_?PKG_[A-Z_]+$],[*** pkg.m4 missing, please install pkg-config])

PKG_PROG_PKG_CONFIG

AC_ARG_VAR([PROTOC], [path to the protoc binary])
AC_PATH_PROG([PROTOC], [protoc])
have_protoc3="no"
if test "x$PROTOC" != "x"; then
	AC_MSG_CHECKING([for protoc 3.0.0+])
	if $PROTOC --version | $EGREP libprotoc.3 >/dev/null; then
		protoc3="yes (`$PROTOC --version`)"
		have_protoc3="yes"
	else
		protoc3="no (`$PROTOC --version`)"
	fi
	AC_MSG_RESULT([$protoc3])
fi
AM_CONDITIONAL(HAVE_PROTOC3, test "x$have_protoc3" = "xyes")

AC_ARG_VAR([GRPC_CPP_PLUGIN], [path to the grpc_cpp_plugin binary])
AC_PATH_PROG([GRPC_CPP_PLUGIN], [grpc_cpp_plugin])
AM_CONDITIONAL(HAVE_GRPC_CPP, test "x$GRPC_CPP_PLUGIN" != "x")

AC_ARG_VAR([PROTOC_C], [path to the protoc-c binary])
AC_PATH_PROG([PROTOC_C], [protoc-c])
if test "x$PROTOC_C" = "x"
then
  have_protoc_c="no (protoc-c compiler not found)"
else
  have_protoc_c="yes"
fi

AC_MSG_CHECKING([for kernel type ($host_os)])
case $host_os in
	*linux*)
	AC_DEFINE([KERNEL_LINUX], 1, [True if program is to be compiled for a Linux kernel])
	ac_system="Linux"
	;;
	*solaris*)
	AC_DEFINE([KERNEL_SOLARIS], 1, [True if program is to be compiled for a Solaris kernel])
	ac_system="Solaris"
	;;
	*darwin*)
	AC_DEFINE([KERNEL_DARWIN], 1, [True if program is to be compiled for a Darwin kernel])
	ac_system="Darwin"
	;;
	*openbsd*)
	AC_DEFINE([KERNEL_OPENBSD], 1, [True if program is to be compiled for an OpenBSD kernel])
	ac_system="OpenBSD"
	;;
	*netbsd*)
	AC_DEFINE([KERNEL_NETBSD], 1, [True if program is to be compiled for a NetBSD kernel])
	ac_system="NetBSD"
	;;
	*aix*)
	AC_DEFINE([KERNEL_AIX], 1, [True if program is to be compiled for a AIX kernel])
	ac_system="AIX"
	;;
	*freebsd*)
	AC_DEFINE([KERNEL_FREEBSD], 1, [True if program is to be compiled for a FreeBSD kernel])
	ac_system="FreeBSD"
	;;
	*)
	ac_system="unknown"
esac
AC_MSG_RESULT([$ac_system])

AM_CONDITIONAL([BUILD_AIX], [test "x$ac_system" = "xAIX"])
AM_CONDITIONAL([BUILD_DARWIN], [test "x$ac_system" = "xDarwin"])
AM_CONDITIONAL([BUILD_FREEBSD], [test "x$ac_system" = "xFreeBSD"])
AM_CONDITIONAL([BUILD_LINUX], [test "x$ac_system" = "xLinux"])
AM_CONDITIONAL([BUILD_OPENBSD], [test "x$ac_system" = "xOpenBSD"])
AM_CONDITIONAL([BUILD_SOLARIS], [test "x$ac_system" = "xSolaris"])

if test "x$ac_system" = "xLinux"
then
	AC_ARG_VAR([KERNEL_DIR], [path to Linux kernel sources])
	if test -z "$KERNEL_DIR"
	then
		KERNEL_DIR="/lib/modules/`uname -r`/source"
	fi

	KERNEL_CFLAGS="-I$KERNEL_DIR/include"
	AC_SUBST(KERNEL_CFLAGS)
fi

if test "x$ac_system" = "xSolaris"
then
	AC_DEFINE(_POSIX_PTHREAD_SEMANTICS, 1, [Define to enforce POSIX thread semantics under Solaris.])
	AC_DEFINE(_REENTRANT,               1, [Define to enable reentrancy interfaces.])

	AC_MSG_CHECKING([whether compiler builds 64bit binaries])
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
			   #ifndef _LP64
			   # error "Compiler not in 64bit mode."
			   #endif
			   ])],
			   [AC_MSG_RESULT([yes])],
			   [
			    AC_MSG_RESULT([no])
			    AC_MSG_NOTICE([Solaris detected. Please consider building a 64-bit binary.])
			   ])
fi

if test "x$ac_system" = "xAIX"
then
	AC_DEFINE(_THREAD_SAFE_ERRNO, 1, [Define to use the thread-safe version of errno under AIX.])
fi

# Where to install .pc files.
pkgconfigdir="${libdir}/pkgconfig"
AC_SUBST(pkgconfigdir)

# Check for standards compliance mode
AC_ARG_ENABLE(standards,
	      AS_HELP_STRING([--enable-standards], [Enable standards compliance mode]),
	      [enable_standards="$enableval"],
	      [enable_standards="no"])
if test "x$enable_standards" = "xyes"
then
	AC_DEFINE(_ISOC99_SOURCE,        1, [Define to enforce ISO C99 compliance.])
	AC_DEFINE(_POSIX_C_SOURCE, 200809L, [Define to enforce POSIX.1-2008 compliance.])
	AC_DEFINE(_XOPEN_SOURCE,       700, [Define to enforce X/Open 7 (XSI) compliance.])
	AC_DEFINE(_REENTRANT,            1, [Define to enable reentrancy interfaces.])
	if test "x$GCC" = "xyes"
	then
		CFLAGS="$CFLAGS -std=c99"
	fi
fi
AM_CONDITIONAL(BUILD_FEATURE_STANDARDS, test "x$enable_standards" = "xyes")

#
# Checks for header files.
#
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_HEADER_DIRENT
AC_HEADER_STDBOOL

AC_CHECK_HEADERS(stdio.h errno.h math.h stdarg.h syslog.h fcntl.h signal.h assert.h sys/types.h sys/socket.h sys/select.h poll.h netdb.h arpa/inet.h sys/resource.h sys/param.h kstat.h regex.h sys/ioctl.h endian.h sys/isa_defs.h fnmatch.h libgen.h)

# For entropy plugin on newer NetBSD
AC_CHECK_HEADERS(sys/rndio.h, [], [],
[#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
])

# For ping library
AC_CHECK_HEADERS(netinet/in_systm.h, [], [],
[#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
])
AC_CHECK_HEADERS(netinet/in.h, [], [],
[#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_NETINET_IN_SYSTM_H
# include <netinet/in_systm.h>
#endif
])
AC_CHECK_HEADERS(netinet/ip.h, [], [],
[#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_NETINET_IN_SYSTM_H
# include <netinet/in_systm.h>
#endif
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
])
AC_CHECK_HEADERS(netinet/ip_icmp.h, [], [],
[#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_NETINET_IN_SYSTM_H
# include <netinet/in_systm.h>
#endif
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#if HAVE_NETINET_IP_H
# include <netinet/ip.h>
#endif
])
AC_CHECK_HEADERS(netinet/ip_var.h, [], [],
[#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_NETINET_IN_SYSTM_H
# include <netinet/in_systm.h>
#endif
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#if HAVE_NETINET_IP_H
# include <netinet/ip.h>
#endif
])
AC_CHECK_HEADERS(netinet/ip6.h, [], [],
[#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_NETINET_IN_SYSTM_H
# include <netinet/in_systm.h>
#endif
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
])
AC_CHECK_HEADERS(netinet/icmp6.h, [], [],
[#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_NETINET_IN_SYSTM_H
# include <netinet/in_systm.h>
#endif
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#if HAVE_NETINET_IP6_H
# include <netinet/ip6.h>
#endif
])
AC_CHECK_HEADERS(netinet/tcp.h, [], [],
[#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_NETINET_IN_SYSTM_H
# include <netinet/in_systm.h>
#endif
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#if HAVE_NETINET_IP_H
# include <netinet/ip.h>
#endif
])
AC_CHECK_HEADERS(netinet/udp.h, [], [],
[#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_NETINET_IN_SYSTM_H
# include <netinet/in_systm.h>
#endif
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#if HAVE_NETINET_IP_H
# include <netinet/ip.h>
#endif
])

have_ip6_ext="no"
AC_CHECK_TYPES([struct ip6_ext], [have_ip6_ext="yes"], [have_ip6_ext="no"],
[#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_NETINET_IN_SYSTM_H
# include <netinet/in_systm.h>
#endif
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#if HAVE_NETINET_IP6_H
# include <netinet/ip6.h>
#endif
])

if test "x$have_ip6_ext" = "xno"; then
	SAVE_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS -DSOLARIS2=8"

	AC_CHECK_TYPES([struct ip6_ext],
		       [have_ip6_ext="yes, with -DSOLARIS2=8"],
		       [have_ip6_ext="no"],
[#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_NETINET_IN_SYSTM_H
# include <netinet/in_systm.h>
#endif
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#if HAVE_NETINET_IP6_H
# include <netinet/ip6.h>
#endif
])

	if test "x$have_ip6_ext" = "xno"; then
		CFLAGS="$SAVE_CFLAGS"
	fi
fi

# For cpu modules
AC_CHECK_HEADERS(sys/dkstat.h)
if test "x$ac_system" = "xDarwin"
then
	AC_CHECK_HEADERS(mach/mach_init.h mach/host_priv.h mach/mach_error.h mach/mach_host.h mach/mach_port.h mach/mach_types.h mach/message.h mach/processor_set.h mach/processor.h mach/processor_info.h mach/task.h mach/thread_act.h mach/vm_region.h mach/vm_map.h mach/vm_prot.h mach/vm_statistics.h mach/kern_return.h)
	AC_CHECK_HEADERS(CoreFoundation/CoreFoundation.h IOKit/IOKitLib.h IOKit/IOTypes.h IOKit/ps/IOPSKeys.h IOKit/IOBSD.h IOKit/storage/IOBlockStorageDriver.h)
	# For the battery plugin
	AC_CHECK_HEADERS(IOKit/ps/IOPowerSources.h, [], [],
[
#if HAVE_IOKIT_IOKITLIB_H
#  include <IOKit/IOKitLib.h>
#endif
#if HAVE_IOKIT_IOTYPES_H
#  include <IOKit/IOTypes.h>
#endif
])

fi

AC_CHECK_HEADERS(sys/sysctl.h, [], [],
[
#if HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
])

AC_MSG_CHECKING([for sysctl kern.cp_times])
if test -x /sbin/sysctl
then
	/sbin/sysctl kern.cp_times >/dev/null 2>&1
	if test $? -eq 0
	then
		AC_MSG_RESULT([yes])
		AC_DEFINE(HAVE_SYSCTL_KERN_CP_TIMES, 1,
		[Define if sysctl supports kern.cp_times])
	else
		AC_MSG_RESULT([no])
	fi
else
	AC_MSG_RESULT([no])
fi

AC_MSG_CHECKING([for sysctl kern.cp_time])
if test -x /sbin/sysctl
then
	/sbin/sysctl kern.cp_time >/dev/null 2>&1
	if test $? -eq 0
	then
		AC_MSG_RESULT([yes])
		AC_DEFINE(HAVE_SYSCTL_KERN_CP_TIME, 1,
			[Define if sysctl supports kern.cp_time])
	else
		AC_MSG_RESULT([no])
	fi
else
	AC_MSG_RESULT([no])
fi

# For hddtemp module
AC_CHECK_HEADERS(linux/major.h)

# For md module (Linux only)
if test "x$ac_system" = "xLinux"
then
	AC_CHECK_HEADERS(linux/raid/md_u.h,
			 [have_linux_raid_md_u_h="yes"],
			 [have_linux_raid_md_u_h="no"],
[
#include <sys/ioctl.h>
#include <linux/major.h>
#include <linux/types.h>
])
	AC_CHECK_HEADERS([sys/sysmacros.h])
else
	have_linux_raid_md_u_h="no"
fi

# For the wireless module
have_linux_wireless_h="no"
if test "x$ac_system" = "xLinux"
then
  AC_CHECK_HEADERS(linux/wireless.h,
		   [have_linux_wireless_h="yes"],
		   [have_linux_wireless_h="no"],
[
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
])
fi

# For the swap module
have_sys_swap_h="yes"
AC_CHECK_HEADERS(sys/swap.h vm/anon.h, [], [have_sys_swap_h="no"],
[
#undef _FILE_OFFSET_BITS
#undef _LARGEFILE64_SOURCE
#if HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
])

# For load module
# For the processes plugin
# For users module
AC_CHECK_HEADERS(sys/loadavg.h linux/config.h utmp.h utmpx.h)

# For interface plugin
AC_CHECK_HEADERS(ifaddrs.h)
AC_CHECK_HEADERS(net/if.h, [], [],
[
#if HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#if HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif
])
AC_CHECK_HEADERS(linux/if.h, [], [],
[
#if HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#if HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif
])
AC_CHECK_HEADERS(linux/inet_diag.h, [], [],
[
#if HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#if HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif
#if HAVE_LINUX_INET_DIAG_H
# include <linux/inet_diag.h>
#endif
])
AC_CHECK_HEADERS(linux/netdevice.h, [], [],
[
#if HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#if HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif
#if HAVE_LINUX_IF_H
# include <linux/if.h>
#endif
])

# For ethstat module
AC_CHECK_HEADERS(linux/sockios.h,
    [have_linux_sockios_h="yes"],
    [have_linux_sockios_h="no"],
    [
#if HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#if HAVE_NET_IF_H
# include <net/if.h>
#endif
    ])
AC_CHECK_HEADERS(linux/ethtool.h,
    [have_linux_ethtool_h="yes"],
    [have_linux_ethtool_h="no"],
    [
#if HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#if HAVE_NET_IF_H
# include <net/if.h>
#endif
#if HAVE_LINUX_SOCKIOS_H
# include <linux/sockios.h>
#endif
    ])

# For ipvs module
have_linux_ip_vs_h="no"
have_net_ip_vs_h="no"
have_ip_vs_h="no"
ip_vs_h_needs_kernel_cflags="no"
if test "x$ac_system" = "xLinux"
then
	AC_CHECK_HEADERS(linux/ip_vs.h, [have_linux_ip_vs_h="yes"])
	AC_CHECK_HEADERS(net/ip_vs.h, [have_net_ip_vs_h="yes"])
	AC_CHECK_HEADERS(ip_vs.h, [have_ip_vs_h="yes"])

	if test "x$have_linux_ip_vs_h$have_net_ip_vs_h$have_ip_vs_h" = "xnonono" && test -d "$KERNEL_DIR"
	then
		SAVE_CFLAGS="$CFLAGS"
		CFLAGS="$CFLAGS $KERNEL_CFLAGS"

		AC_MSG_NOTICE([Did not find ip_vs.h. Trying again using headers from $KERNEL_DIR.])

		AC_CHECK_HEADERS(linux/ip_vs.h, [have_linux_ip_vs_h="yes"])
		AC_CHECK_HEADERS(net/ip_vs.h, [have_net_ip_vs_h="yes"])
		AC_CHECK_HEADERS(ip_vs.h, [have_ip_vs_h="yes"])

		if test "x$have_linux_ip_vs_h" = "xyes" || test "x$have_net_ip_vs_h" = "xyes" || test "x$have_ip_vs_h" = "xyes"
		then
			ip_vs_h_needs_kernel_cflags="yes"
		fi

		CFLAGS="$SAVE_CFLAGS"
	fi
fi
AM_CONDITIONAL(IP_VS_H_NEEDS_KERNEL_CFLAGS, test "x$ip_vs_h_needs_kernel_cflags" = "xyes")

# For quota module
AC_CHECK_HEADERS(sys/ucred.h, [], [],
[
#if HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
])

# For mount interface
AC_CHECK_HEADERS(sys/mount.h, [], [],
[
#if HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
])

# For the email plugin
AC_CHECK_HEADERS(linux/un.h, [], [],
[
#if HAVE_SYS_SOCKET_H
#	include <sys/socket.h>
#endif
])

AC_CHECK_HEADERS([ \
  ctype.h \
  fs_info.h \
  fshelp.h \
  grp.h \
  kvm.h \
  limits.h \
  locale.h \
  mntent.h \
  mnttab.h \
  paths.h \
  pwd.h \
  sys/fs_types.h \
  sys/fstyp.h \
  sys/mntent.h \
  sys/mnttab.h \
  sys/statfs.h \
  sys/statvfs.h \
  sys/un.h \
  sys/vfs.h \
  sys/vfstab.h \
  sys/vmmeter.h \
  wordexp.h \
])

# For the dns plugin
AC_CHECK_HEADERS(arpa/nameser.h)
AC_CHECK_HEADERS(arpa/nameser_compat.h, [], [],
[
#if HAVE_ARPA_NAMESER_H
# include <arpa/nameser.h>
#endif
])

AC_CHECK_HEADERS(net/if_arp.h, [], [],
[#if HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
])
AC_CHECK_HEADERS(net/ppp_defs.h)
AC_CHECK_HEADERS(net/if_ppp.h, [], [],
[#if HAVE_NET_PPP_DEFS_H
# include <net/ppp_defs.h>
#endif
])
AC_CHECK_HEADERS(netinet/if_ether.h, [], [],
[#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#if HAVE_NET_IF_H
# include <net/if.h>
#endif
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
])

have_net_pfvar_h="no"
AC_CHECK_HEADERS(net/pfvar.h,
               [have_net_pfvar_h="yes"],
               [have_net_pfvar_h="no"],
[
#if HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#if HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#if HAVE_NET_IF_H
# include <net/if.h>
#endif
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
])

# For cpusleep plugin
AC_CACHE_CHECK([whether clock_boottime and clock_monotonic are supported],
		       [c_cv_have_clock_boottime_monotonic],
		       AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[[
#include <time.h>
]],
[[
 struct timespec b, m;
 clock_gettime(CLOCK_BOOTTIME, &b );
 clock_gettime(CLOCK_MONOTONIC, &m );
]]
		       )],
		       [c_cv_have_clock_boottime_monotonic="yes"],
		       [c_cv_have_clock_boottime_monotonic="no"]))


# For the turbostat plugin
have_asm_msrindex_h="no"
AC_CHECK_HEADERS(asm/msr-index.h, [have_asm_msrindex_h="yes"])

if test "x$have_asm_msrindex_h" = "xyes"
then
  AC_CACHE_CHECK([whether asm/msr-index.h has MSR_PKG_C10_RESIDENCY],
                 [c_cv_have_usable_asm_msrindex_h],
                 AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[[[
#include<asm/msr-index.h>
]]],
[[[
int y = MSR_PKG_C10_RESIDENCY;
return(y);
]]]
  )],
                 [c_cv_have_usable_asm_msrindex_h="yes"],
                 [c_cv_have_usable_asm_msrindex_h="no"],
                                  )
                 )
fi

have_cpuid_h="no"
AC_CHECK_HEADERS(cpuid.h, [have_cpuid_h="yes"])

AC_CHECK_HEADERS(sys/capability.h)
#
# Checks for typedefs, structures, and compiler characteristics.
#
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UID_T
AC_HEADER_TIME

#
# Checks for library functions.
#
AC_CHECK_FUNCS(gettimeofday select strdup strtol getaddrinfo getnameinfo strchr memcpy strstr strcmp strncmp strncpy strlen strncasecmp strcasecmp openlog closelog sysconf setenv if_indextoname setlocale asprintf)

AC_FUNC_STRERROR_R

test_cxx_flags() {
	AC_LANG_PUSH([C++])
	AC_LANG_CONFTEST([
		AC_LANG_SOURCE([[int main(void){}]])
	])
	$CXX -c conftest.cpp $CXXFLAGS $@ > /dev/null 2> /dev/null
	ret=$?
	rm -f conftest.o
	AC_LANG_POP([C++])
	return $ret
}

SAVE_CFLAGS="$CFLAGS"
# Emulate behavior of src/Makefile.am
if test "x$GCC" = "xyes"
then
	CFLAGS="$CFLAGS -Wall -Werror"
fi

AC_CACHE_CHECK([for strtok_r],
  [c_cv_have_strtok_r_default],
  AC_LINK_IFELSE(
    [AC_LANG_PROGRAM(
[[[
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
]]],
[[[
      char buffer[] = "foo,bar,baz";
      char *token;
      char *dummy;
      char *saveptr;

      dummy = buffer;
      saveptr = NULL;
      while ((token = strtok_r (dummy, ",", &saveptr)) != NULL)
      {
        dummy = NULL;
        printf ("token = %s;\n", token);
      }
]]]
    )],
    [c_cv_have_strtok_r_default="yes"],
    [c_cv_have_strtok_r_default="no"]
  )
)

if test "x$c_cv_have_strtok_r_default" = "xno"
then
  CFLAGS="$CFLAGS -D_REENTRANT=1"

  AC_CACHE_CHECK([if strtok_r needs _REENTRANT],
    [c_cv_have_strtok_r_reentrant],
    AC_LINK_IFELSE(
      [AC_LANG_PROGRAM(
[[[
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
]]],
[[[
        char buffer[] = "foo,bar,baz";
        char *token;
        char *dummy;
        char *saveptr;

        dummy = buffer;
        saveptr = NULL;
        while ((token = strtok_r (dummy, ",", &saveptr)) != NULL)
        {
          dummy = NULL;
          printf ("token = %s;\n", token);
        }
]]]
      )],
      [c_cv_have_strtok_r_reentrant="yes"],
      [AC_MSG_FAILURE([strtok_r isn't available. Please file a bugreport!])]
    )
  )
fi

CFLAGS="$SAVE_CFLAGS"
if test "x$c_cv_have_strtok_r_reentrant" = "xyes"
then
	CFLAGS="$CFLAGS -D_REENTRANT=1"
fi

AC_CHECK_FUNCS(getpwnam_r getgrnam_r setgroups regcomp regerror regexec regfree)

socket_needs_socket="no"
AC_CHECK_FUNCS(socket, [], AC_CHECK_LIB(socket, socket, [socket_needs_socket="yes"], AC_MSG_ERROR(cannot find socket)))
AM_CONDITIONAL(BUILD_WITH_LIBSOCKET, test "x$socket_needs_socket" = "xyes")

clock_gettime_needs_rt="no"
clock_gettime_needs_posix4="no"
have_clock_gettime="no"
AC_CHECK_FUNCS(clock_gettime, [have_clock_gettime="yes"])
if test "x$have_clock_gettime" = "xno"
then
	AC_CHECK_LIB(rt, clock_gettime, [clock_gettime_needs_rt="yes"
					 have_clock_gettime="yes"])
fi
if test "x$have_clock_gettime" = "xno"
then
	AC_CHECK_LIB(posix4, clock_gettime, [clock_gettime_needs_posix4="yes"
					     have_clock_gettime="yes"])
fi
if test "x$have_clock_gettime" = "xyes"
then
	AC_DEFINE(HAVE_CLOCK_GETTIME, 1, [Define if the clock_gettime(2) function is available.])
fi

nanosleep_needs_rt="no"
nanosleep_needs_posix4="no"
AC_CHECK_FUNCS(nanosleep,
    [],
    AC_CHECK_LIB(rt, nanosleep,
        [nanosleep_needs_rt="yes"],
        AC_CHECK_LIB(posix4, nanosleep,
            [nanosleep_needs_posix4="yes"],
            AC_MSG_ERROR(cannot find nanosleep))))

AM_CONDITIONAL(BUILD_WITH_LIBRT, test "x$clock_gettime_needs_rt" = "xyes" || test "x$nanosleep_needs_rt" = "xyes")
AM_CONDITIONAL(BUILD_WITH_LIBPOSIX4, test "x$clock_gettime_needs_posix4" = "xyes" || test "x$nanosleep_needs_posix4" = "xyes")

AC_CHECK_FUNCS(sysctl, [have_sysctl="yes"], [have_sysctl="no"])
AC_CHECK_FUNCS(sysctlbyname, [have_sysctlbyname="yes"], [have_sysctlbyname="no"])
AC_CHECK_FUNCS(host_statistics, [have_host_statistics="yes"], [have_host_statistics="no"])
AC_CHECK_FUNCS(processor_info, [have_processor_info="yes"], [have_processor_info="no"])
AC_CHECK_FUNCS(thread_info, [have_thread_info="yes"], [have_thread_info="no"])
AC_CHECK_FUNCS(statfs, [have_statfs="yes"], [have_statfs="no"])
AC_CHECK_FUNCS(statvfs, [have_statvfs="yes"], [have_statvfs="no"])
AC_CHECK_FUNCS(getifaddrs, [have_getifaddrs="yes"], [have_getifaddrs="no"])
AC_CHECK_FUNCS(getloadavg, [have_getloadavg="yes"], [have_getloadavg="no"])
AC_CHECK_FUNCS(syslog, [have_syslog="yes"], [have_syslog="no"])
AC_CHECK_FUNCS(getutent, [have_getutent="yes"], [have_getutent="no"])
AC_CHECK_FUNCS(getutxent, [have_getutxent="yes"], [have_getutxent="no"])

# Check for strptime {{{
if test "x$GCC" = "xyes"
then
	SAVE_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS -Wall -Wextra -Werror"
fi

AC_CHECK_FUNCS(strptime, [have_strptime="yes"], [have_strptime="no"])
if test "x$have_strptime" = "xyes"
then
	AC_CACHE_CHECK([whether strptime is exported by default],
		       [c_cv_have_strptime_default],
		       AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[[[
#include <time.h>
]]],
[[[
 struct tm stm;
 (void) strptime ("2010-12-30%13:42:42", "%Y-%m-%dT%T", &stm);
]]]
		       )],
		       [c_cv_have_strptime_default="yes"],
		       [c_cv_have_strptime_default="no"]))
fi
if test "x$have_strptime" = "xyes" && test "x$c_cv_have_strptime_default" = "xno"
then
	AC_CACHE_CHECK([whether strptime needs standards mode],
		       [c_cv_have_strptime_standards],
		       AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[[[
#ifndef _ISOC99_SOURCE
# define _ISOC99_SOURCE 1
#endif
#ifndef _POSIX_C_SOURCE
# define _POSIX_C_SOURCE 200112L
#endif
#ifndef _XOPEN_SOURCE
# define _XOPEN_SOURCE 500
#endif
#include <time.h>
]]],
[[[
 struct tm stm;
 (void) strptime ("2010-12-30%13:42:42", "%Y-%m-%dT%T", &stm);
]]]
		       )],
		       [c_cv_have_strptime_standards="yes"],
		       [c_cv_have_strptime_standards="no"]))

	if test "x$c_cv_have_strptime_standards" = "xyes"
	then
		AC_DEFINE([STRPTIME_NEEDS_STANDARDS], 1, [Set to true if strptime is only exported in X/Open mode (GNU libc).])
	else
		have_strptime="no"
	fi
fi

if test "x$GCC" = "xyes"
then
	CFLAGS="$SAVE_CFLAGS"
fi
# }}} Check for strptime

AC_CHECK_FUNCS(swapctl, [have_swapctl="yes"], [have_swapctl="no"])
if test "x$have_swapctl" = "xyes"; then
        AC_CACHE_CHECK([whether swapctl takes two arguments],
                [c_cv_have_swapctl_two_args],
                AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[[[
#if HAVE_SYS_SWAP_H && !defined(_LP64) && _FILE_OFFSET_BITS == 64
#  undef _FILE_OFFSET_BITS
#  undef _LARGEFILE64_SOURCE
#endif
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/swap.h>
#include <unistd.h>
]]],
[[[
int num = swapctl(0, NULL);
]]]
                        )],
                        [c_cv_have_swapctl_two_args="yes"],
                        [c_cv_have_swapctl_two_args="no"]
                )
        )
        AC_CACHE_CHECK([whether swapctl takes three arguments],
                [c_cv_have_swapctl_three_args],
                AC_COMPILE_IFELSE(
                        [AC_LANG_PROGRAM(
[[[
#if HAVE_SYS_SWAP_H && !defined(_LP64) && _FILE_OFFSET_BITS == 64
#  undef _FILE_OFFSET_BITS
#  undef _LARGEFILE64_SOURCE
#endif
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/swap.h>
#include <unistd.h>
]]],
[[[
int num = swapctl(0, NULL, 0);
]]]
                        )],
                        [c_cv_have_swapctl_three_args="yes"],
                        [c_cv_have_swapctl_three_args="no"]
                )
        )
fi
# Check for different versions of `swapctl' here..
if test "x$have_swapctl" = "xyes"; then
        if test "x$c_cv_have_swapctl_two_args" = "xyes"; then
                AC_DEFINE(HAVE_SWAPCTL_TWO_ARGS, 1,
                          [Define if the function swapctl exists and takes two arguments.])
        fi
        if test "x$c_cv_have_swapctl_three_args" = "xyes"; then
                AC_DEFINE(HAVE_SWAPCTL_THREE_ARGS, 1,
                          [Define if the function swapctl exists and takes three arguments.])
        fi
fi

# Check for NAN
AC_ARG_WITH(nan-emulation, [AS_HELP_STRING([--with-nan-emulation], [use emulated NAN. For crosscompiling only.])],
[
 if test "x$withval" = "xno"; then
	 nan_type="none"
 else if test "x$withval" = "xyes"; then
	 nan_type="zero"
 else
	 nan_type="$withval"
 fi; fi
],
[nan_type="none"])
if test "x$nan_type" = "xnone"; then
  AC_CACHE_CHECK([whether NAN is defined by default],
    [c_cv_have_nan_default],
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[[[
#include <stdlib.h>
#include <math.h>
static double foo = NAN;
]]],
[[[
       if (isnan (foo))
        return 0;
       else
	return 1;
]]]
      )],
      [c_cv_have_nan_default="yes"],
      [c_cv_have_nan_default="no"]
    )
  )
  if test "x$c_cv_have_nan_default" = "xyes"
  then
    nan_type="default"
  fi
fi
if test "x$nan_type" = "xnone"; then
  AC_CACHE_CHECK([whether NAN is defined by __USE_ISOC99],
    [c_cv_have_nan_isoc],
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[[[
#include <stdlib.h>
#define __USE_ISOC99 1
#include <math.h>
static double foo = NAN;
]]],
[[[
       if (isnan (foo))
        return 0;
       else
	return 1;
]]]
      )],
      [c_cv_have_nan_isoc="yes"],
      [c_cv_have_nan_isoc="no"]
    )
  )
  if test "x$c_cv_have_nan_isoc" = "xyes"
  then
    nan_type="isoc99"
  fi
fi
if test "x$nan_type" = "xnone"; then
  SAVE_LDFLAGS=$LDFLAGS
  LDFLAGS="$LDFLAGS -lm"
  AC_CACHE_CHECK([whether NAN can be defined by 0/0],
    [c_cv_have_nan_zero],
    AC_RUN_IFELSE([AC_LANG_PROGRAM(
[[[
#include <stdlib.h>
#include <math.h>
#ifdef NAN
# undef NAN
#endif
#define NAN (0.0 / 0.0)
#ifndef isnan
# define isnan(f) ((f) != (f))
#endif
static double foo = NAN;
]]],
[[[
       if (isnan (foo))
        return 0;
       else
	return 1;
]]]
      )],
      [c_cv_have_nan_zero="yes"],
      [c_cv_have_nan_zero="no"]
    )
  )
  LDFLAGS=$SAVE_LDFLAGS
  if test "x$c_cv_have_nan_zero" = "xyes"
  then
    nan_type="zero"
  fi
fi

if test "x$nan_type" = "xdefault"; then
  AC_DEFINE(NAN_STATIC_DEFAULT, 1,
    [Define if NAN is defined by default and can initialize static variables.])
else if test "x$nan_type" = "xisoc99"; then
  AC_DEFINE(NAN_STATIC_ISOC, 1,
    [Define if NAN is defined by __USE_ISOC99 and can initialize static variables.])
else if test "x$nan_type" = "xzero"; then
  AC_DEFINE(NAN_ZERO_ZERO, 1,
    [Define if NAN can be defined as (0.0 / 0.0)])
else
  AC_MSG_ERROR([Didn't find out how to statically initialize variables to NAN. Sorry.])
fi; fi; fi

AC_ARG_WITH(fp-layout, [AS_HELP_STRING([--with-fp-layout], [set the memory layout of doubles. For crosscompiling only.])],
[
 if test "x$withval" = "xnothing"; then
 	fp_layout_type="nothing"
 else if test "x$withval" = "xendianflip"; then
 	fp_layout_type="endianflip"
 else if test "x$withval" = "xintswap"; then
 	fp_layout_type="intswap"
 else
 	AC_MSG_ERROR([Invalid argument for --with-fp-layout. Valid arguments are: nothing, endianflip, intswap]);
fi; fi; fi
],
[fp_layout_type="unknown"])

if test "x$fp_layout_type" = "xunknown"; then
  AC_CACHE_CHECK([if doubles are stored in x86 representation],
    [c_cv_fp_layout_need_nothing],
    AC_RUN_IFELSE([AC_LANG_PROGRAM(
[[[
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#if HAVE_STDBOOL_H
# include <stdbool.h>
#endif
]]],
[[[
	uint64_t i0;
	uint64_t i1;
	uint8_t c[8];
	double d;

	d = 8.642135e130;
	memcpy ((void *) &i0, (void *) &d, 8);

	i1 = i0;
	memcpy ((void *) c, (void *) &i1, 8);

	if ((c[0] == 0x2f) && (c[1] == 0x25)
			&& (c[2] == 0xc0) && (c[3] == 0xc7)
			&& (c[4] == 0x43) && (c[5] == 0x2b)
			&& (c[6] == 0x1f) && (c[7] == 0x5b))
		return (0);
	else
		return (1);
]]]
      )],
      [c_cv_fp_layout_need_nothing="yes"],
      [c_cv_fp_layout_need_nothing="no"]
    )
  )
  if test "x$c_cv_fp_layout_need_nothing" = "xyes"; then
    fp_layout_type="nothing"
  fi
fi
if test "x$fp_layout_type" = "xunknown"; then
  AC_CACHE_CHECK([if endianflip converts to x86 representation],
    [c_cv_fp_layout_need_endianflip],
    AC_RUN_IFELSE([AC_LANG_PROGRAM(
[[[
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#if HAVE_STDBOOL_H
# include <stdbool.h>
#endif
#define endianflip(A) ((((uint64_t)(A) & 0xff00000000000000LL) >> 56) | \
                       (((uint64_t)(A) & 0x00ff000000000000LL) >> 40) | \
                       (((uint64_t)(A) & 0x0000ff0000000000LL) >> 24) | \
                       (((uint64_t)(A) & 0x000000ff00000000LL) >> 8)  | \
                       (((uint64_t)(A) & 0x00000000ff000000LL) << 8)  | \
                       (((uint64_t)(A) & 0x0000000000ff0000LL) << 24) | \
                       (((uint64_t)(A) & 0x000000000000ff00LL) << 40) | \
                       (((uint64_t)(A) & 0x00000000000000ffLL) << 56))
]]],
[[[
	uint64_t i0;
	uint64_t i1;
	uint8_t c[8];
	double d;

	d = 8.642135e130;
	memcpy ((void *) &i0, (void *) &d, 8);

	i1 = endianflip (i0);
	memcpy ((void *) c, (void *) &i1, 8);

	if ((c[0] == 0x2f) && (c[1] == 0x25)
			&& (c[2] == 0xc0) && (c[3] == 0xc7)
			&& (c[4] == 0x43) && (c[5] == 0x2b)
			&& (c[6] == 0x1f) && (c[7] == 0x5b))
		return (0);
	else
		return (1);
]]]
      )],
      [c_cv_fp_layout_need_endianflip="yes"],
      [c_cv_fp_layout_need_endianflip="no"]
    )
  )
  if test "x$c_cv_fp_layout_need_endianflip" = "xyes"; then
    fp_layout_type="endianflip"
  fi
fi
if test "x$fp_layout_type" = "xunknown"; then
  AC_CACHE_CHECK([if intswap converts to x86 representation],
    [c_cv_fp_layout_need_intswap],
    AC_RUN_IFELSE([AC_LANG_PROGRAM(
[[[
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#if HAVE_STDBOOL_H
# include <stdbool.h>
#endif
#define intswap(A)    ((((uint64_t)(A) & 0xffffffff00000000LL) >> 32) | \
                       (((uint64_t)(A) & 0x00000000ffffffffLL) << 32))
]]],
[[[
	uint64_t i0;
	uint64_t i1;
	uint8_t c[8];
	double d;

	d = 8.642135e130;
	memcpy ((void *) &i0, (void *) &d, 8);

	i1 = intswap (i0);
	memcpy ((void *) c, (void *) &i1, 8);

	if ((c[0] == 0x2f) && (c[1] == 0x25)
			&& (c[2] == 0xc0) && (c[3] == 0xc7)
			&& (c[4] == 0x43) && (c[5] == 0x2b)
			&& (c[6] == 0x1f) && (c[7] == 0x5b))
		return (0);
	else
		return (1);
]]]
      )],
      [c_cv_fp_layout_need_intswap="yes"],
      [c_cv_fp_layout_need_intswap="no"]
    )
  )
  if test "x$c_cv_fp_layout_need_intswap" = "xyes"; then
    fp_layout_type="intswap"
  fi
fi

if test "x$fp_layout_type" = "xnothing"; then
  AC_DEFINE(FP_LAYOUT_NEED_NOTHING, 1,
  [Define if doubles are stored in x86 representation.])
else if test "x$fp_layout_type" = "xendianflip"; then
  AC_DEFINE(FP_LAYOUT_NEED_ENDIANFLIP, 1,
  [Define if endianflip is needed to convert to x86 representation.])
else if test "x$fp_layout_type" = "xintswap"; then
  AC_DEFINE(FP_LAYOUT_NEED_INTSWAP, 1,
  [Define if intswap is needed to convert to x86 representation.])
else
  AC_MSG_ERROR([Didn't find out how doubles are stored in memory. Sorry.])
fi; fi; fi

# --with-useragent {{{
AC_ARG_WITH(useragent, [AS_HELP_STRING([--with-useragent@<:@=AGENT@:>@], [User agent to use on http requests])],
[
    if test "x$withval" != "xno" && test "x$withval" != "xyes"
    then
        AC_DEFINE_UNQUOTED(METADATAD_USERAGENT, ["$withval"], [User agent for http requests])
    fi
])

# }}}

# --with-data-max-name-len {{{
AC_ARG_WITH(data-max-name-len, [AS_HELP_STRING([--with-data-max-name-len@<:@=VALUE@:>@], [Maximum length of data buffers])],
[
    if test "x$withval" != "x" && test $withval -gt 0
    then
        AC_DEFINE_UNQUOTED(DATA_MAX_NAME_LEN, [$withval], [Maximum length of data buffers])
    else
        AC_MSG_ERROR([DATA_MAX_NAME_LEN must be a positive integer -- $withval given])
    fi
],
[   AC_DEFINE(DATA_MAX_NAME_LEN, 128, [Maximum length of data buffers])]
)
# }}}

have_getfsstat="no"
AC_CHECK_FUNCS(getfsstat, [have_getfsstat="yes"])
have_getvfsstat="no"
AC_CHECK_FUNCS(getvfsstat, [have_getvfsstat="yes"])
have_listmntent="no"
AC_CHECK_FUNCS(listmntent, [have_listmntent="yes"])
have_getmntent_r="no"
AC_CHECK_FUNCS(getmntent_r, [have_getmntent_r="yes"])

have_getmntent="no"
AC_CHECK_FUNCS(getmntent, [have_getmntent="c"])
if test "x$have_getmntent" = "xno"; then
	AC_CHECK_LIB(sun, getmntent, [have_getmntent="sun"])
fi
if test "x$have_getmntent" = "xno"; then
	AC_CHECK_LIB(seq, getmntent, [have_getmntent="seq"])
fi
if test "x$have_getmntent" = "xno"; then
	AC_CHECK_LIB(gen, getmntent, [have_getmntent="gen"])
fi

if test "x$have_getmntent" = "xc"; then
	AC_CACHE_CHECK([whether getmntent takes one argument],
		[c_cv_have_one_getmntent],
		AC_COMPILE_IFELSE(
			[AC_LANG_PROGRAM(
[[[
#include "$srcdir/src/utils_mount.h"
]]],
[[[
FILE *fh;
struct mntent *me;
fh = setmntent ("/etc/mtab", "r");
me = getmntent (fh);
return(me->mnt_passno);
]]]
			)],
			[c_cv_have_one_getmntent="yes"],
			[c_cv_have_one_getmntent="no"]
		)
	)
	AC_CACHE_CHECK([whether getmntent takes two arguments],
		[c_cv_have_two_getmntent],
		AC_COMPILE_IFELSE(
			[AC_LANG_PROGRAM(
[[[
#include "$srcdir/src/utils_mount.h"
]]],
[[[
				 FILE *fh;
				 struct mnttab mt;
				 int status;
				 fh = fopen ("/etc/mnttab", "r");
				 status = getmntent (fh, &mt);
				 return(status);
]]]
			)],
			[c_cv_have_two_getmntent="yes"],
			[c_cv_have_two_getmntent="no"]
		)
	)
fi

# Check for different versions of `getmntent' here..

if test "x$have_getmntent" = "xc"; then
	if test "x$c_cv_have_one_getmntent" = "xyes"; then
		AC_DEFINE(HAVE_ONE_GETMNTENT, 1,
			  [Define if the function getmntent exists and takes one argument.])
	fi
	if test "x$c_cv_have_two_getmntent" = "xyes"; then
		AC_DEFINE(HAVE_TWO_GETMNTENT, 1,
			  [Define if the function getmntent exists and takes two arguments.])
	fi
fi
if test "x$have_getmntent" = "xsun"; then
	AC_DEFINE(HAVE_SUN_GETMNTENT, 1,
		  [Define if the function getmntent exists. It is the version from libsun.])
fi
if test "x$have_getmntent" = "xseq"; then
	AC_DEFINE(HAVE_SEQ_GETMNTENT, 1,
		  [Define if the function getmntent exists. It is the version from libseq.])
fi
if test "x$have_getmntent" = "xgen"; then
	AC_DEFINE(HAVE_GEN_GETMNTENT, 1,
		  [Define if the function getmntent exists. It is the version from libgen.])
fi

# Check for htonll
AC_CACHE_CHECK([if have htonll defined],
                  [c_cv_have_htonll],
                  AC_LINK_IFELSE([AC_LANG_PROGRAM(
[[[
#include <sys/types.h>
#include <netinet/in.h>
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif
]]],
[[[
          return htonll(0);
]]]
    )],
    [c_cv_have_htonll="yes"],
    [c_cv_have_htonll="no"]
  )
)
if test "x$c_cv_have_htonll" = "xyes"
then
    AC_DEFINE(HAVE_HTONLL, 1, [Define if the function htonll exists.])
fi

# Check for structures
AC_CHECK_MEMBERS([struct if_data.ifi_ibytes, struct if_data.ifi_opackets, struct if_data.ifi_ierrors],
	[AC_DEFINE(HAVE_STRUCT_IF_DATA, 1, [Define if struct if_data exists and is usable.])],
	[],
	[
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <net/if.h>
	])
AC_CHECK_MEMBERS([struct net_device_stats.rx_bytes, struct net_device_stats.tx_packets, struct net_device_stats.rx_errors],
	[AC_DEFINE(HAVE_STRUCT_NET_DEVICE_STATS, 1, [Define if struct net_device_stats exists and is usable.])],
	[],
	[
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <linux/if.h>
	#include <linux/netdevice.h>
	])
AC_CHECK_MEMBERS([struct inet_diag_req.id, struct inet_diag_req.idiag_states],
	[AC_DEFINE(HAVE_STRUCT_LINUX_INET_DIAG_REQ, 1, [Define if struct inet_diag_req exists and is usable.])],
	[],
	[
	#include <linux/inet_diag.h>
	])


AC_CHECK_MEMBERS([struct ip_mreqn.imr_ifindex], [],
	[],
	[
	#include <netinet/in.h>
	#include <net/if.h>
	])

AC_CHECK_MEMBERS([struct kinfo_proc.ki_pid, struct kinfo_proc.ki_rssize, struct kinfo_proc.ki_rusage],
	[
		AC_DEFINE(HAVE_STRUCT_KINFO_PROC_FREEBSD, 1,
			[Define if struct kinfo_proc exists in the FreeBSD variant.])
		have_struct_kinfo_proc_freebsd="yes"
	],
	[
		have_struct_kinfo_proc_freebsd="no"
	],
	[
#include <kvm.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>
	])

AC_CHECK_MEMBERS([struct kinfo_proc.p_pid, struct kinfo_proc.p_vm_rssize],
	[
		AC_DEFINE(HAVE_STRUCT_KINFO_PROC_OPENBSD, 1,
			[Define if struct kinfo_proc exists in the OpenBSD variant.])
		have_struct_kinfo_proc_openbsd="yes"
	],
	[
		have_struct_kinfo_proc_openbsd="no"
	],
	[
#include <sys/param.h>
#include <sys/sysctl.h>
#include <kvm.h>
	])


AC_CHECK_MEMBERS([struct kinfo_proc2.p_pid, struct kinfo_proc2.p_uru_maxrss],
	[
		AC_DEFINE(HAVE_STRUCT_KINFO_PROC2_NETBSD, 1,
			[Define if struct kinfo_proc2 exists in the NetBSD variant.])
		have_struct_kinfo_proc2_netbsd="yes"
	],
	[
		have_struct_kinfo_proc2_netbsd="no"
	],
	[
#include <sys/param.h>
#include <sys/sysctl.h>
#include <kvm.h>
	])



AC_CHECK_MEMBERS([struct udphdr.uh_dport, struct udphdr.uh_sport], [], [],
[#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_NETINET_IN_SYSTM_H
# include <netinet/in_systm.h>
#endif
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#if HAVE_NETINET_IP_H
# include <netinet/ip.h>
#endif
#if HAVE_NETINET_UDP_H
# include <netinet/udp.h>
#endif
])
AC_CHECK_MEMBERS([struct udphdr.dest, struct udphdr.source], [], [],
[#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_NETINET_IN_SYSTM_H
# include <netinet/in_systm.h>
#endif
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#if HAVE_NETINET_IP_H
# include <netinet/ip.h>
#endif
#if HAVE_NETINET_UDP_H
# include <netinet/udp.h>
#endif
])

AC_CHECK_MEMBERS([kstat_io_t.nwritten, kstat_io_t.writes, kstat_io_t.nwrites, kstat_io_t.wtime],
	[],
	[],
	[
#if HAVE_KSTAT_H
# include <kstat.h>
#endif
	])

#
# Checks for libraries begin here
#

with_libresolv="yes"
AC_CHECK_LIB(resolv, res_search,
[
	AC_DEFINE(HAVE_LIBRESOLV, 1, [Define to 1 if you have the 'resolv' library (-lresolv).])
],
[with_libresolv="no"])
AM_CONDITIONAL(BUILD_WITH_LIBRESOLV, test "x$with_libresolv" = "xyes")

SAVE_LIBS="$LIBS"
AC_CHECK_LIB([pthread],
  [pthread_create],
  [],
  [AC_MSG_ERROR([Symbol 'pthread_create' not found in libpthread"])],
  []
)
PTHREAD_LIBS="$LIBS"
LIBS="$SAVE_LIBS"

AC_CHECK_HEADERS([pthread.h],
  [],
  [AC_MSG_ERROR([pthread.h not found])]
)
AC_SUBST([PTHREAD_LIBS])

m4_divert_once([HELP_WITH], [
metadatad additional packages:])

# --with-libcurl {{{
with_curl_config="curl-config"
with_curl_cflags=""
with_curl_libs=""
AC_ARG_WITH(libcurl, [AS_HELP_STRING([--with-libcurl@<:@=PREFIX@:>@], [Path to libcurl.])],
[
	if test "x$withval" = "xno"
	then
		with_libcurl="no"
	else if test "x$withval" = "xyes"
	then
		with_libcurl="yes"
	else
		if test -f "$withval" && test -x "$withval"
		then
			with_curl_config="$withval"
			with_libcurl="yes"
		else if test -x "$withval/bin/curl-config"
		then
			with_curl_config="$withval/bin/curl-config"
			with_libcurl="yes"
		fi; fi
		with_libcurl="yes"
	fi; fi
],
[
	with_libcurl="yes"
])
if test "x$with_libcurl" = "xyes"
then
	with_curl_cflags=`$with_curl_config --cflags 2>/dev/null`
	curl_config_status=$?

	if test $curl_config_status -ne 0
	then
		with_libcurl="no ($with_curl_config failed)"
	else
		SAVE_CPPFLAGS="$CPPFLAGS"
		CPPFLAGS="$CPPFLAGS $with_curl_cflags"

		AC_CHECK_HEADERS(curl/curl.h, [], [with_libcurl="no (curl/curl.h not found)"], [])

		CPPFLAGS="$SAVE_CPPFLAGS"
	fi
fi
if test "x$with_libcurl" = "xyes"
then
	with_curl_libs=`$with_curl_config --libs 2>/dev/null`
	curl_config_status=$?

	if test $curl_config_status -ne 0
	then
		with_libcurl="no ($with_curl_config failed)"
	else
		AC_CHECK_LIB(curl, curl_easy_init,
		 [with_libcurl="yes"],
		 [with_libcurl="no (symbol 'curl_easy_init' not found)"],
		 [$with_curl_libs])
		AC_CHECK_DECL(CURLOPT_USERNAME,
		 [have_curlopt_username="yes"],
		 [have_curlopt_username="no"],
		 [[#include <curl/curl.h>]])
		AC_CHECK_DECL(CURLOPT_TIMEOUT_MS,
		 [have_curlopt_timeout="yes"],
		 [have_curlopt_timeout="no"],
		 [[#include <curl/curl.h>]])
	fi
fi
if test "x$with_libcurl" = "xyes"
then
	SAVE_CPPFLAGS="$CPPFLAGS"
	SAVE_LDFLAGS="$LDFLAGS"
	CPPFLAGS="$CPPFLAGS $with_curl_cflags"
	LDFLAGS="$LDFLAGS $with_curl_libs"
	AC_CACHE_CHECK([for CURLINFO_APPCONNECT_TIME],
		[c_cv_have_curlinfo_appconnect_time],
		AC_LINK_IFELSE([AC_LANG_PROGRAM(
[[
#include <curl/curl.h>
]],
[[
int val = CURLINFO_APPCONNECT_TIME;
return val;
]]
			)],
			[c_cv_have_curlinfo_appconnect_time="yes"],
			[c_cv_have_curlinfo_appconnect_time="no"]
		)
	)
	CPPFLAGS="$SAVE_CPPFLAGS"
	LDFLAGS="$SAVE_LDFLAGS"
fi
AM_CONDITIONAL(BUILD_WITH_LIBCURL, test "x$with_libcurl" = "xyes")
if test "x$c_cv_have_curlinfo_appconnect_time" = "xyes"
then
	AC_DEFINE(HAVE_CURLINFO_APPCONNECT_TIME, 1, [Define if curl.h defines CURLINFO_APPCONNECT_TIME.])
fi

if test "x$with_libcurl" = "xyes"
then
	BUILD_WITH_LIBCURL_CFLAGS="$with_curl_cflags"
	BUILD_WITH_LIBCURL_LIBS="$with_curl_libs"
	AC_SUBST(BUILD_WITH_LIBCURL_CFLAGS)
	AC_SUBST(BUILD_WITH_LIBCURL_LIBS)

	if test "x$have_curlopt_username" = "xyes"
	then
		AC_DEFINE(HAVE_CURLOPT_USERNAME, 1, [Define if libcurl supports CURLOPT_USERNAME option.])
	fi

	if test "x$have_curlopt_timeout" = "xyes"
	then
		AC_DEFINE(HAVE_CURLOPT_TIMEOUT_MS, 1, [Define if libcurl supports CURLOPT_TIMEOUT_MS option.])
	fi
fi
# }}}

# --with-libgcrypt {{{
GCRYPT_CPPFLAGS="$GCRYPT_CPPFLAGS"
GCRYPT_LDFLAGS="$GCRYPT_LDFLAGS"
GCRYPT_LIBS="$GCRYPT_LIBS"
AC_ARG_WITH(libgcrypt, [AS_HELP_STRING([--with-libgcrypt@<:@=PREFIX@:>@], [Path to libgcrypt.])],
[
 if test -f "$withval" && test -x "$withval"
 then
	 with_libgcrypt_config="$withval"
	 with_libgcrypt="yes"
 else if test -f "$withval/bin/gcrypt-config" && test -x "$withval/bin/gcrypt-config"
 then
	 with_libgcrypt_config="$withval/bin/gcrypt-config"
	 with_libgcrypt="yes"
 else if test -d "$withval"
 then
	 GCRYPT_CPPFLAGS="$GCRYPT_CPPFLAGS -I$withval/include"
	 GCRYPT_LDFLAGS="$GCRYPT_LDFLAGS -L$withval/lib"
	 with_libgcrypt="yes"
 else
	 with_libgcrypt_config="gcrypt-config"
	 with_libgcrypt="$withval"
 fi; fi; fi
],
[
 with_libgcrypt_config="libgcrypt-config"
 with_libgcrypt="yes"
])

if test "x$with_libgcrypt" = "xyes" && test "x$with_libgcrypt_config" != "x"
then
	if test "x$GCRYPT_CPPFLAGS" = "x"
	then
		GCRYPT_CPPFLAGS=`"$with_libgcrypt_config" --cflags 2>/dev/null`
	fi

	if test "x$GCRYPT_LIBS" = "x"
	then
		GCRYPT_LIBS=`"$with_libgcrypt_config" --libs 2>/dev/null`
	fi
fi

SAVE_CPPFLAGS="$CPPFLAGS"
SAVE_LDFLAGS="$LDFLAGS"
SAVE_LIBS="$LIBS"
CPPFLAGS="$CPPFLAGS $GCRYPT_CPPFLAGS"
LDFLAGS="$LDFLAGS $GCRYPT_LDFLAGS"
LIBS="$LIBS $GCRYPT_LIBS"

if test "x$with_libgcrypt" = "xyes"
then
	if test "x$GCRYPT_CPPFLAGS" != "x"
	then
		AC_MSG_NOTICE([gcrypt CPPFLAGS: $GCRYPT_CPPFLAGS])
	fi
	AC_CHECK_HEADERS(gcrypt.h,
		[with_libgcrypt="yes"],
		[with_libgcrypt="no (gcrypt.h not found)"])
fi

if test "x$with_libgcrypt" = "xyes"
then
	AC_CHECK_LIB(gcrypt, gcry_md_hash_buffer,
		[with_libgcrypt="yes"],
		[with_libgcrypt="no (symbol gcry_md_hash_buffer not found)"])
fi

CPPFLAGS="$SAVE_CPPFLAGS"
LDFLAGS="$SAVE_LDFLAGS"
LIBS="$SAVE_LIBS"

if test "x$with_libgcrypt" = "xyes"
then
	AC_DEFINE(HAVE_LIBGCRYPT, 1, [Define to 1 if you have the gcrypt library (-lgcrypt).])
fi

AC_SUBST(GCRYPT_CPPFLAGS)
AC_SUBST(GCRYPT_LDFLAGS)
AC_SUBST(GCRYPT_LIBS)
AM_CONDITIONAL(BUILD_WITH_LIBGCRYPT, test "x$with_libgcrypt" = "xyes")
# }}}

# --with-libgrpc++ {{{
with_libgrpcpp_cppflags=""
with_libgrpcpp_ldflags=""
AC_ARG_WITH([libgrpc++], [AS_HELP_STRING([--with-libgrpc++@<:@=PREFIX@:>@], [Path to libgrpc++.])],
  [
    with_grpcpp="$withval"
    if test "x$withval" != "xno" && test "x$withval" != "xyes"
    then
      with_libgrpcpp_cppflags="-I$withval/include"
      with_libgrpcpp_ldflags="-L$withval/lib"
      with_libgrpcpp="yes"
    fi
    if test "x$withval" = "xno"
    then
      with_libgrpcpp="no (disabled on command line)"
    fi
  ],
  [withval="yes"]
)
if test "x$withval" = "xyes"
then
PKG_CHECK_MODULES([GRPCPP], [grpc++],
  [with_libgrpcpp="yes"],
  [with_libgrpcpp="no (pkg-config could not find libgrpc++)"]
)
fi

if test "x$withval" != "xno"
then
  AC_MSG_CHECKING([whether $CXX accepts -std=c++11])
  if test_cxx_flags -std=c++11; then
    AC_MSG_RESULT([yes])
  else
    AC_MSG_RESULT([no])
    with_libgrpcpp="no (requires C++11 support)"
  fi
fi

if test "x$with_libgrpcpp" = "xyes"
then
  AC_LANG_PUSH(C++)
  SAVE_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="-std=c++11 $with_libgrpcpp_cppflags $GRPCPP_CFLAGS $CPPFLAGS"
  AC_CHECK_HEADERS([grpc++/grpc++.h], [],
    [with_libgrpcpp="no (<grpc++/grpc++.h> not found)"]
  )
  CPPFLAGS="$SAVE_CPPFLAGS"
  AC_LANG_POP(C++)
fi
if test "x$with_libgrpcpp" = "xyes"
then
  AC_LANG_PUSH(C++)
  SAVE_CPPFLAGS="$CPPFLAGS"
  SAVE_LDFLAGS="$LDFLAGS"
  SAVE_LIBS="$LIBS"
  CPPFLAGS="-std=c++11 $with_libgrpcpp_cppflags $GRPCPP_CFLAGS $CPPFLAGS"
  LDFLAGS="$with_libgrpcpp_ldflags"
  if test "x$GRPCPP_LIBS" = "x"
  then
    LIBS="-lgrpc++"
  else
    LIBS="$GRPCPP_LIBS"
  fi
  AC_LINK_IFELSE(
    [AC_LANG_PROGRAM(
      [[#include <grpc++/grpc++.h>]],
      [[grpc::ServerBuilder sb;]]
    )],
    [
      with_libgrpcpp="yes"
      if test "x$GRPCPP_LIBS" = "x"
      then
        GRPCPP_LIBS="-lgrpc++"
      fi
    ],
    [with_libgrpcpp="no (libgrpc++ not found)"]
  )
  CPPFLAGS="$SAVE_CPPFLAGS"
  LDFLAGS="$SAVE_LDFLAGS"
  LIBS="$SAVE_LIBS"
  AC_LANG_POP(C++)
fi
BUILD_WITH_LIBGRPCPP_CPPFLAGS="-std=c++11 $with_libgrpcpp_cppflags $GRPCPP_CFLAGS"
BUILD_WITH_LIBGRPCPP_LDFLAGS="$with_libgrpcpp_ldflags"
BUILD_WITH_LIBGRPCPP_LIBS="$GRPCPP_LIBS"
AC_SUBST([BUILD_WITH_LIBGRPCPP_CPPFLAGS])
AC_SUBST([BUILD_WITH_LIBGRPCPP_LDFLAGS])
AC_SUBST([BUILD_WITH_LIBGRPCPP_LIBS])
# }}}

# --with-java {{{
with_java_home="$JAVA_HOME"
if test "x$with_java_home" = "x"
then
	with_java_home="/usr/lib/jvm"
fi
JAVAC="$JAVAC"
JAR="$JAR"
AC_ARG_WITH(java, [AS_HELP_STRING([--with-java@<:@=PREFIX@:>@], [Path to Java home.])],
[
	if test "x$withval" = "xno"
	then
		with_java="no"
	else if test "x$withval" = "xyes"
	then
		with_java="yes"
	else
		with_java_home="$withval"
		with_java="yes"
	fi; fi
],
[with_java="yes"])
if test "x$with_java" = "xyes"
then
	if test -d "$with_java_home"
	then
		AC_MSG_CHECKING([for jni.h])
		TMPVAR=`find -L "$with_java_home" -name jni.h -type f -exec 'dirname' '{}' ';' 2>/dev/null | head -n 1`
		if test "x$TMPVAR" != "x"
		then
			AC_MSG_RESULT([found in $TMPVAR])
			JAVA_CPPFLAGS="$JAVA_CPPFLAGS -I$TMPVAR"
		else
			AC_MSG_RESULT([not found])
		fi

		AC_MSG_CHECKING([for jni_md.h])
		TMPVAR=`find -L "$with_java_home" -name jni_md.h -type f -exec 'dirname' '{}' ';' 2>/dev/null | head -n 1`
		if test "x$TMPVAR" != "x"
		then
			AC_MSG_RESULT([found in $TMPVAR])
			JAVA_CPPFLAGS="$JAVA_CPPFLAGS -I$TMPVAR"
		else
			AC_MSG_RESULT([not found])
		fi

		AC_MSG_CHECKING([for libjvm.so])
		TMPVAR=`find -L "$with_java_home" -type f \( -name libjvm.so -o -name libjvm.dylib \) -exec 'dirname' '{}' ';' 2>/dev/null | head -n 1`
		if test "x$TMPVAR" != "x"
		then
			AC_MSG_RESULT([found in $TMPVAR])
			JAVA_LDFLAGS="$JAVA_LDFLAGS -L$TMPVAR -Wl,-rpath -Wl,$TMPVAR"
		else
			AC_MSG_RESULT([not found])
		fi

		if test "x$JAVAC" = "x"
		then
			AC_MSG_CHECKING([for javac])
			TMPVAR=`find -L "$with_java_home" -name javac -type f 2>/dev/null | head -n 1`
			if test "x$TMPVAR" != "x"
			then
				JAVAC="$TMPVAR"
				AC_MSG_RESULT([$JAVAC])
			else
				AC_MSG_RESULT([not found])
			fi
		fi
		if test "x$JAR" = "x"
		then
			AC_MSG_CHECKING([for jar])
			TMPVAR=`find -L "$with_java_home" -name jar -type f 2>/dev/null | head -n 1`
			if test "x$TMPVAR" != "x"
			then
				JAR="$TMPVAR"
				AC_MSG_RESULT([$JAR])
			else
				AC_MSG_RESULT([not found])
			fi
		fi
	else if test "x$with_java_home" != "x"
	then
		AC_MSG_WARN([JAVA_HOME: No such directory: $with_java_home])
	fi; fi
fi

if test "x$JAVA_CPPFLAGS" != "x"
then
	AC_MSG_NOTICE([Building with JAVA_CPPFLAGS set to: $JAVA_CPPFLAGS])
fi
if test "x$JAVA_CFLAGS" != "x"
then
	AC_MSG_NOTICE([Building with JAVA_CFLAGS set to: $JAVA_CFLAGS])
fi
if test "x$JAVA_LDFLAGS" != "x"
then
	AC_MSG_NOTICE([Building with JAVA_LDFLAGS set to: $JAVA_LDFLAGS])
fi
if test "x$JAVA_LIBS" != "x"
then
	AC_MSG_NOTICE([Building with JAVA_LIBS set to: $JAVA_LIBS])
fi
if test "x$JAVAC" = "x"
then
	with_javac_path="$PATH"
	if test "x$with_java_home" != "x"
	then
		with_javac_path="$with_java_home:with_javac_path"
		if test -d "$with_java_home/bin"
		then
			with_javac_path="$with_java_home/bin:with_javac_path"
		fi
	fi

	AC_PATH_PROG(JAVAC, javac, [], "$with_javac_path")
fi
if test "x$JAVAC" = "x"
then
	with_java="no (javac not found)"
fi
if test "x$JAR" = "x"
then
	with_jar_path="$PATH"
	if test "x$with_java_home" != "x"
	then
		with_jar_path="$with_java_home:$with_jar_path"
		if test -d "$with_java_home/bin"
		then
			with_jar_path="$with_java_home/bin:$with_jar_path"
		fi
	fi

	AC_PATH_PROG(JAR, jar, [], "$with_jar_path")
fi
if test "x$JAR" = "x"
then
	with_java="no (jar not found)"
fi

SAVE_CPPFLAGS="$CPPFLAGS"
SAVE_CFLAGS="$CFLAGS"
SAVE_LDFLAGS="$LDFLAGS"
SAVE_LIBS="$LIBS"
CPPFLAGS="$CPPFLAGS $JAVA_CPPFLAGS"
CFLAGS="$CFLAGS $JAVA_CFLAGS"
LDFLAGS="$LDFLAGS $JAVA_LDFLAGS"
LIBS="$LIBS $JAVA_LIBS"

if test "x$with_java" = "xyes"
then
	AC_CHECK_HEADERS(jni.h, [], [with_java="no (jni.h not found)"])
fi
if test "x$with_java" = "xyes"
then
	AC_CHECK_LIB(jvm, JNI_CreateJavaVM,
	[with_java="yes"],
	[with_java="no (Symbol 'JNI_CreateJavaVM' not found)"],
	[$JAVA_LIBS $PTHREAD_LIBS])
fi
if test "x$with_java" = "xyes"
then
	JAVA_LIBS="$JAVA_LIBS -ljvm"
	AC_MSG_NOTICE([Building with JAVA_LIBS set to: $JAVA_LIBS])
fi

CPPFLAGS="$SAVE_CPPFLAGS"
CFLAGS="$SAVE_CFLAGS"
LDFLAGS="$SAVE_LDFLAGS"
LIBS="$SAVE_LIBS"

AC_SUBST(JAVA_CPPFLAGS)
AC_SUBST(JAVA_CFLAGS)
AC_SUBST(JAVA_LDFLAGS)
AC_SUBST(JAVA_LIBS)
AM_CONDITIONAL(BUILD_WITH_JAVA, test "x$with_java" = "xyes")
# }}}

# --with-liblua {{{
AC_ARG_VAR([LIBLUA_PKG_CONFIG_NAME], [Name of liblua used by pkg-config])
if test "x$LIBLUA_PKG_CONFIG_NAME" != "x"
then
  PKG_CHECK_MODULES([LUA], [$LIBLUA_PKG_CONFIG_NAME],
    [with_liblua="yes"],
    [with_liblua="no"]
  )
else
  PKG_CHECK_MODULES([LUA], [lua],
    [with_liblua="yes"],
    [
      PKG_CHECK_MODULES([LUA], [lua-5.3],
        [with_liblua="yes"],
        [
	  PKG_CHECK_MODULES([LUA], [lua5.3],
            [with_liblua="yes"],
            [
              PKG_CHECK_MODULES([LUA], [lua-5.2],
                [with_liblua="yes"],
                [
                  PKG_CHECK_MODULES([LUA], [lua5.2],
                    [with_liblua="yes"],
                    [
                      PKG_CHECK_MODULES([LUA], [lua-5.1],
                        [with_liblua="yes"],
                        [
                          PKG_CHECK_MODULES([LUA], [lua5.1],
                            [with_liblua="yes"],
                            [with_liblua="no (pkg-config cannot find liblua)"]
                          )
                        ]
                      )
                    ]
                  )
                ]
              )
            ]
          )
        ]
      )
    ]
  )
fi

if test "x$with_liblua" = "xyes"
then
  SAVE_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS $LUA_CFLAGS"

  AC_CHECK_HEADERS([lua.h lauxlib.h lualib.h],
    [with_liblua="yes"],
    [with_liblua="no (header not found)"]
  )

  CFLAGS="$SAVE_CFLAGS"
fi

if test "x$with_liblua" = "xyes"
then
  SAVE_LIBS="$LIBS"
  LIBS="$LIBS $LUA_LIBS"

  AC_CHECK_FUNC([lua_settop],
    [with_liblua="yes"],
    [with_liblua="no (symbol 'lua_settop' not found)"]
  )

  LIBS="$SAVE_LIBS"
fi

if test "x$with_liblua" = "xyes"
then
    BUILD_WITH_LIBLUA_CFLAGS="$LUA_CFLAGS"
    BUILD_WITH_LIBLUA_LIBS="$LUA_LIBS"
fi
AC_SUBST(BUILD_WITH_LIBLUA_CFLAGS)
AC_SUBST(BUILD_WITH_LIBLUA_LIBS)
# }}}

# --with-liboconfig {{{
with_own_liboconfig="no"
liboconfig_LDFLAGS="$LDFLAGS"
liboconfig_CPPFLAGS="$CPPFLAGS"
AC_ARG_WITH(liboconfig, [AS_HELP_STRING([--with-liboconfig@<:@=PREFIX@:>@], [Path to liboconfig.])],
[
	if test "x$withval" != "xno" && test "x$withval" != "xyes"
	then
		if test -d "$withval/lib"
		then
			liboconfig_LDFLAGS="$LDFLAGS -L$withval/lib"
		fi
		if test -d "$withval/include"
		then
			liboconfig_CPPFLAGS="$CPPFLAGS -I$withval/include"
		fi
	fi
	if test "x$withval" = "xno"
	then
		AC_MSG_ERROR("liboconfig is required")
	fi
],
[
	with_liboconfig="yes"
])

save_LDFLAGS="$LDFLAGS"
save_CPPFLAGS="$CPPFLAGS"
LDFLAGS="$liboconfig_LDFLAGS"
CPPFLAGS="$liboconfig_CPPFLAGS"
AC_CHECK_LIB(oconfig, oconfig_parse_file,
[
	with_liboconfig="yes"
	with_own_liboconfig="no"
],
[
	with_liboconfig="yes"
	with_own_liboconfig="yes"
	LDFLAGS="$save_LDFLAGS"
	CPPFLAGS="$save_CPPFLAGS"
])

AM_CONDITIONAL(BUILD_WITH_OWN_LIBOCONFIG, test "x$with_own_liboconfig" = "xyes")
if test "x$with_own_liboconfig" = "xyes"
then
	with_liboconfig="yes (shipped version)"
fi
# }}}

# --with-libperl {{{
perl_interpreter="perl"
AC_ARG_WITH(libperl, [AS_HELP_STRING([--with-libperl@<:@=PREFIX@:>@], [Path to libperl.])],
[
	if test -f "$withval" && test -x "$withval"
	then
		perl_interpreter="$withval"
		with_libperl="yes"
	else if test "x$withval" != "xno" && test "x$withval" != "xyes"
	then
		LDFLAGS="$LDFLAGS -L$withval/lib"
		CPPFLAGS="$CPPFLAGS -I$withval/include"
		perl_interpreter="$withval/bin/perl"
		with_libperl="yes"
	else
		with_libperl="$withval"
	fi; fi
],
[
	with_libperl="yes"
])

AC_MSG_CHECKING([for perl])
perl_interpreter=`which "$perl_interpreter" 2> /dev/null`
if test -x "$perl_interpreter"
then
	AC_MSG_RESULT([yes ($perl_interpreter)])
else
	perl_interpreter=""
	AC_MSG_RESULT([no])
fi

AC_SUBST(PERL, "$perl_interpreter")

if test "x$with_libperl" = "xyes" \
	&& test -n "$perl_interpreter"
then
  SAVE_CFLAGS="$CFLAGS"
  SAVE_LIBS="$LIBS"
dnl ARCHFLAGS="" -> disable multi -arch on OSX (see Config_heavy.pl:fetch_string)
  PERL_CFLAGS=`ARCHFLAGS="" $perl_interpreter -MExtUtils::Embed -e perl_inc`
  PERL_LIBS=`ARCHFLAGS="" $perl_interpreter -MExtUtils::Embed -e ldopts`
  CFLAGS="$CFLAGS $PERL_CFLAGS"
  LIBS="$LIBS $PERL_LIBS"

  AC_CACHE_CHECK([for libperl],
    [c_cv_have_libperl],
    AC_LINK_IFELSE([AC_LANG_PROGRAM(
[[[
#define PERL_NO_GET_CONTEXT
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>
]]],
[[[
       dTHX;
       load_module (PERL_LOADMOD_NOIMPORT,
			 newSVpv ("Collectd::Plugin::FooBar", 24),
			 Nullsv);
]]]
      )],
      [c_cv_have_libperl="yes"],
      [c_cv_have_libperl="no"]
    )
  )

  if test "x$c_cv_have_libperl" = "xyes"
  then
	  AC_DEFINE(HAVE_LIBPERL, 1, [Define if libperl is present and usable.])
	  AC_SUBST(PERL_CFLAGS)
	  AC_SUBST(PERL_LIBS)
  else
	  with_libperl="no"
  fi

  CFLAGS="$SAVE_CFLAGS"
  LIBS="$SAVE_LIBS"
else if test -z "$perl_interpreter"; then
  with_libperl="no (no perl interpreter found)"
  c_cv_have_libperl="no"
fi; fi
AM_CONDITIONAL(BUILD_WITH_LIBPERL, test "x$with_libperl" = "xyes")

if test "x$with_libperl" = "xyes"
then
	SAVE_CFLAGS="$CFLAGS"
	SAVE_LIBS="$LIBS"
	CFLAGS="$CFLAGS $PERL_CFLAGS"
	LIBS="$LIBS $PERL_LIBS"

	AC_CACHE_CHECK([if perl supports ithreads],
		[c_cv_have_perl_ithreads],
		AC_LINK_IFELSE([AC_LANG_PROGRAM(
[[[
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#if !defined(USE_ITHREADS)
# error "Perl does not support ithreads!"
#endif /* !defined(USE_ITHREADS) */
]]],
[[[ ]]]
			)],
			[c_cv_have_perl_ithreads="yes"],
			[c_cv_have_perl_ithreads="no"]
		)
	)

	if test "x$c_cv_have_perl_ithreads" = "xyes"
	then
		AC_DEFINE(HAVE_PERL_ITHREADS, 1, [Define if Perl supports ithreads.])
	fi

	CFLAGS="$SAVE_CFLAGS"
	LIBS="$SAVE_LIBS"
fi

if test "x$with_libperl" = "xyes"
then
	SAVE_CFLAGS="$CFLAGS"
	SAVE_LIBS="$LIBS"
	# trigger an error if Perl_load_module*() uses __attribute__nonnull__(3)
	# (see issues #41 and #42)
	CFLAGS="$CFLAGS $PERL_CFLAGS -Wall -Werror"
	LIBS="$LIBS $PERL_LIBS"

	AC_CACHE_CHECK([for broken Perl_load_module()],
		[c_cv_have_broken_perl_load_module],
		AC_LINK_IFELSE([AC_LANG_PROGRAM(
[[[
#define PERL_NO_GET_CONTEXT
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>
]]],
[[[
			 dTHX;
			 load_module (PERL_LOADMOD_NOIMPORT,
			     newSVpv ("Collectd::Plugin::FooBar", 24),
			     Nullsv);
]]]
			)],
			[c_cv_have_broken_perl_load_module="no"],
			[c_cv_have_broken_perl_load_module="yes"]
		)
	)

	CFLAGS="$SAVE_CFLAGS"
	LIBS="$SAVE_LIBS"
fi
AM_CONDITIONAL(HAVE_BROKEN_PERL_LOAD_MODULE,
		test "x$c_cv_have_broken_perl_load_module" = "xyes")

if test "x$with_libperl" = "xyes"
then
	SAVE_CFLAGS="$CFLAGS"
	SAVE_LIBS="$LIBS"
	CFLAGS="$CFLAGS $PERL_CFLAGS"
	LIBS="$LIBS $PERL_LIBS"

	AC_CHECK_MEMBER(
		[struct mgvtbl.svt_local],
		[have_struct_mgvtbl_svt_local="yes"],
		[have_struct_mgvtbl_svt_local="no"],
		[
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>
		])

	if test "x$have_struct_mgvtbl_svt_local" = "xyes"
	then
		AC_DEFINE(HAVE_PERL_STRUCT_MGVTBL_SVT_LOCAL, 1,
				  [Define if Perl's struct mgvtbl has member svt_local.])
	fi

	CFLAGS="$SAVE_CFLAGS"
	LIBS="$SAVE_LIBS"
fi
# }}}

# --with-libpq {{{
with_pg_config="pg_config"
with_libpq_includedir=""
with_libpq_libdir=""
with_libpq_cppflags=""
with_libpq_ldflags=""
AC_ARG_WITH(libpq, [AS_HELP_STRING([--with-libpq@<:@=PREFIX@:>@],
	[Path to libpq.])],
[
	if test "x$withval" = "xno"
	then
		with_libpq="no"
	else if test "x$withval" = "xyes"
	then
		with_libpq="yes"
	else
		if test -f "$withval" && test -x "$withval";
		then
			with_pg_config="$withval"
		else if test -x "$withval/bin/pg_config"
		then
			with_pg_config="$withval/bin/pg_config"
		fi; fi
		with_libpq="yes"
	fi; fi
],
[
	with_libpq="yes"
])
if test "x$with_libpq" = "xyes"
then
	with_libpq_includedir=`$with_pg_config --includedir 2> /dev/null`
	pg_config_status=$?

	if test $pg_config_status -eq 0
	then
		if test -n "$with_libpq_includedir"; then
			for dir in $with_libpq_includedir; do
				with_libpq_cppflags="$with_libpq_cppflags -I$dir"
			done
		fi
	else
		AC_MSG_WARN([$with_pg_config returned with status $pg_config_status])
	fi

	SAVE_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $with_libpq_cppflags"

	AC_CHECK_HEADERS(libpq-fe.h, [],
		[with_libpq="no (libpq-fe.h not found)"], [])

	CPPFLAGS="$SAVE_CPPFLAGS"
fi
if test "x$with_libpq" = "xyes"
then
	with_libpq_libdir=`$with_pg_config --libdir 2> /dev/null`
	pg_config_status=$?

	if test $pg_config_status -eq 0
	then
		if test -n "$with_libpq_libdir"; then
			for dir in $with_libpq_libdir; do
				with_libpq_ldflags="$with_libpq_ldflags -L$dir"
			done
		fi
	else
		AC_MSG_WARN([$with_pg_config returned with status $pg_config_status])
	fi

	SAVE_LDFLAGS="$LDFLAGS"
	LDFLAGS="$LDFLAGS $with_libpq_ldflags"

	AC_CHECK_LIB(pq, PQconnectdb,
		[with_libpq="yes"],
		[with_libpq="no (symbol 'PQconnectdb' not found)"])

	AC_CHECK_LIB(pq, PQserverVersion,
		[with_libpq="yes"],
		[with_libpq="no (symbol 'PQserverVersion' not found)"])

	LDFLAGS="$SAVE_LDFLAGS"
fi
if test "x$with_libpq" = "xyes"
then
	BUILD_WITH_LIBPQ_CPPFLAGS="$with_libpq_cppflags"
	BUILD_WITH_LIBPQ_LDFLAGS="$with_libpq_ldflags"
	AC_SUBST(BUILD_WITH_LIBPQ_CPPFLAGS)
	AC_SUBST(BUILD_WITH_LIBPQ_LDFLAGS)
fi
AM_CONDITIONAL(BUILD_WITH_LIBPQ, test "x$with_libpq" = "xyes")
# }}}

# --with-libprotobuf {{{
with_libprotobuf_cppflags=""
with_libprotobuf_ldflags=""
AC_ARG_WITH([libprotobuf], [AS_HELP_STRING([--with-libprotobuf@<:@=PREFIX@:>@], [Path to libprotobuf.])],
  [
    if test "x$withval" != "xno" && test "x$withval" != "xyes"
    then
      with_libprotobuf_cppflags="-I$withval/include"
      with_libprotobuf_ldflags="-L$withval/lib"
      with_libprotobuf="yes"
    fi
    if test "x$withval" = "xno"
    then
      with_libprotobuf="no (disabled on command line)"
    fi
  ],
  [withval="yes"]
)
if test "x$withval" = "xyes"
then
PKG_CHECK_MODULES([PROTOBUF], [protobuf],
  [with_libprotobuf="yes"],
  [with_libprotobuf="no (pkg-config could not find libprotobuf)"]
)
fi

if test "x$withval" != "xno"
then
  SAVE_LDFLAGS="$LDFLAGS"
  SAVE_LIBS="$LIBS"
  LDFLAGS="$with_libprotobuf_ldflags"
  LIBS="$PROTOBUF_LIBS $LIBS"
  AC_LANG_PUSH([C++])
  AC_CHECK_LIB([protobuf], [main],
    [
      SAVE_CPPFLAGS="$CPPFLAGS"
      CPPFLAGS="$with_libprotobuf_cppflags $PROTOBUF_CFLAGS"
      if test "x$PROTOBUF_LIBS" = "x"
      then
        PROTOBUF_LIBS="-lprotobuf"
      fi
      AC_CHECK_HEADERS([google/protobuf/util/time_util.h],
        [with_libprotobuf="yes"],
        [with_libprotobuf="no (<google/protobuf/util/time_util.h> not found)"]
      )
      CPPFLAGS="$SAVE_CPPFLAGS"
    ],
    [with_libprotobuf="no (libprotobuf not found)"]
  )
  AC_LANG_POP([C++])
  LDFLAGS="$SAVE_LDFLAGS"
  LIBS="$SAVE_LIBS"
fi
BUILD_WITH_LIBPROTOBUF_CPPFLAGS="$with_libprotobuf_cppflags $PROTOBUF_CFLAGS"
BUILD_WITH_LIBPROTOBUF_LDFLAGS="$with_libprotobuf_ldflags"
BUILD_WITH_LIBPROTOBUF_LIBS="$PROTOBUF_LIBS"
AC_SUBST([BUILD_WITH_LIBPROTOBUF_CPPFLAGS])
AC_SUBST([BUILD_WITH_LIBPROTOBUF_LDFLAGS])
AC_SUBST([BUILD_WITH_LIBPROTOBUF_LIBS])
# }}}

# --with-libprotobuf-c {{{
with_libprotobuf_c_cppflags=""
with_libprotobuf_c_ldflags=""
AC_ARG_WITH([libprotobuf-c], [AS_HELP_STRING([--with-libprotobuf-c@<:@=PREFIX@:>@], [Path to libprotobuf-c.])],
  [
    if test "x$withval" != "xno" && test "x$withval" != "xyes"
    then
      with_libprotobuf_c_cppflags="-I$withval/include"
      with_libprotobuf_c_ldflags="-L$withval/lib"
      with_libprotobuf_c="yes"
    fi
    if test "x$withval" = "xno"
    then
      with_libprotobuf_c="no (disabled on command line)"
    fi
  ],
  [withval="yes"]
)
if test "x$withval" = "xyes"
then
PKG_CHECK_MODULES([PROTOBUF_C], [libprotobuf-c],
  [with_libprotobuf_c="yes"],
  [with_libprotobuf_c="no (pkg-config could not find libprotobuf-c)"]
)
fi

if test "x$withval" != "xno"
then
  SAVE_LDFLAGS="$LDFLAGS"
  SAVE_LIBS="$LIBS"
  LDFLAGS="$with_libprotobuf_c_ldflags"
  LIBS="$PROTOBUF_C_LIBS $LIBS"
  AC_CHECK_LIB([protobuf-c], [protobuf_c_message_pack],
    [
      SAVE_CPPFLAGS="$CPPFLAGS"
      CPPFLAGS="$with_libprotobuf_c_cppflags $PROTOBUF_C_CFLAGS"
      if test "x$PROTOBUF_C_LIBS" = "x"
      then
        PROTOBUF_C_LIBS="-lprotobuf-c"
      fi
      AC_CHECK_HEADERS([protobuf-c/protobuf-c.h google/protobuf-c/protobuf-c.h],
        [
          with_libprotobuf_c="yes"
          break
        ],
        [with_libprotobuf_c="no (<protobuf-c.h> not found)"]
      )
      CPPFLAGS="$SAVE_CPPFLAGS"
    ],
    [with_libprotobuf_c="no (libprotobuf-c not found)"]
  )
  LDFLAGS="$SAVE_LDFLAGS"
  LIBS="$SAVE_LIBS"
fi
BUILD_WITH_LIBPROTOBUF_C_CPPFLAGS="$with_libprotobuf_c_cppflags $PROTOBUF_C_CFLAGS"
BUILD_WITH_LIBPROTOBUF_C_LDFLAGS="$with_libprotobuf_c_ldflags"
BUILD_WITH_LIBPROTOBUF_C_LIBS="$PROTOBUF_C_LIBS"
AC_SUBST([BUILD_WITH_LIBPROTOBUF_C_CPPFLAGS])
AC_SUBST([BUILD_WITH_LIBPROTOBUF_C_LDFLAGS])
AC_SUBST([BUILD_WITH_LIBPROTOBUF_C_LIBS])
# }}}

# --with-libpython {{{
AC_ARG_VAR([LIBPYTHON_CPPFLAGS], [Preprocessor flags for libpython])
AC_ARG_VAR([LIBPYTHON_LDFLAGS], [Linker flags for libpython])
AC_ARG_VAR([LIBPYTHON_LIBS], [Libraries for libpython])

AC_ARG_WITH([libpython],
  [AS_HELP_STRING([--with-libpython],
    [if we should build with libpython @<:@default=yes@:>@])
  ],
  [with_libpython="$withval"],
  [with_libpython="check"]
)
if test "$with_libpython" != "no"; then
  if test "$LIBPYTHON_CPPFLAGS" = "" && test "$LIBPYTHON_LDFLAGS" = ""; then
    AC_ARG_VAR([PYTHON_CONFIG], [path to python-config])
    AC_PATH_PROGS([PYTHON_CONFIG],
      [python3-config python2-config python-config]
    )
    if test "$PYTHON_CONFIG" = ""; then
      if test "$with_libpython" = "yes"; then
        AC_MSG_ERROR([Unable to find python-config])
      fi
      with_libpython="no"
    fi
  fi
fi

if test "$PYTHON_CONFIG" != ""; then
  LIBPYTHON_CPPFLAGS="`${PYTHON_CONFIG} --includes`"
  if test $? -ne 0; then
    with_libpython="no"
  fi
  LIBPYTHON_LDFLAGS="`${PYTHON_CONFIG} --ldflags`"
  if test $? -ne 0; then
    with_libpython="no"
  fi
  LIBPYTHON_LIBS="`${PYTHON_CONFIG} --libs`"
  if test $? -ne 0; then
    with_libpython="no"
  fi
fi

if test "$with_libpython" != "xno"; then
  SAVE_CPPFLAGS="$CPPFLAGS"
  SAVE_LDFLAGS="$LDFLAGS"
  SAVE_LIBS="$LIBS"
  CPPFLAGS="$LIBPYTHON_CPPFLAGS $CPPFLAGS"
  LDFLAGS="$LIBPYTHON_LDFLAGS $LDFLAGS"
  LIBS="$LIBPYTHON_LIBS $LIBS"
  AC_CHECK_HEADERS([Python.h],
    [
      AC_MSG_CHECKING([for libpython])
      AC_LINK_IFELSE([AC_LANG_PROGRAM(
          [[#include <Python.h>]],
          [[Py_Initialize();]])
        ],
        [with_libpython="yes"],
        [with_libpython="no"]
      )
      AC_MSG_RESULT([$with_libpython])
    ],
    [with_libpython="no"]
  )
  CPPFLAGS="$SAVE_CPPFLAGS"
  LDFLAGS="$SAVE_LDFLAGS"
  LIBS="$SAVE_LIBS"
fi
# }}} --with-libpython

# --with-libssl {{{
with_libssl_cflags=""
with_libssl_ldflags=""
AC_ARG_WITH(libssl, [AS_HELP_STRING([--with-libssl@<:@=PREFIX@:>@], [Path to libssl.])],
[
	if test "x$withval" != "xno" && test "x$withval" != "xyes"
	then
		with_libssl_cppflags="-I$withval/include"
		with_libssl_ldflags="-L$withval/lib"
		with_libssl="yes"
	else
		with_libssl="$withval"
	fi
],
[
	with_libssl="yes"
])
if test "x$with_libssl" = "xyes"
then
	SAVE_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $with_libssl_cppflags"

  AC_CHECK_HEADERS(openssl/sha.h openssl/blowfish.h openssl/rand.h,
    [with_libssl="yes"],
    [with_libssl="no (ssl header not found)"])

	CPPFLAGS="$SAVE_CPPFLAGS"
fi
if test "x$with_libssl" = "xyes"
then
	SAVE_CPPFLAGS="$CPPFLAGS"
	SAVE_LDFLAGS="$LDFLAGS"
	CPPFLAGS="$CPPFLAGS $with_libssl_cppflags"
	LDFLAGS="$LDFLAGS $with_libssl_ldflags"

	AC_CHECK_LIB(ssl, SSL_library_init, [with_libssl="yes"], [with_libssl="no (Symbol 'SSL_library_init' not found)"])

	CPPFLAGS="$SAVE_CPPFLAGS"
	LDFLAGS="$SAVE_LDFLAGS"
fi
if test "x$with_libssl" = "xyes"
then
	BUILD_WITH_LIBSSL_CFLAGS="$with_libssl_cflags"
	BUILD_WITH_LIBSSL_LDFLAGS="$with_libssl_ldflags"
	BUILD_WITH_LIBSSL_LIBS="-lssl -lcrypto"
	AC_SUBST(BUILD_WITH_LIBSSL_CFLAGS)
	AC_SUBST(BUILD_WITH_LIBSSL_LDFLAGS)
	AC_SUBST(BUILD_WITH_LIBSSL_LIBS)
	AC_DEFINE(HAVE_LIBSSL, 1, [Define if libssl is present and usable.])
fi
AM_CONDITIONAL(BUILD_WITH_LIBSSL, test "x$with_libssl" = "xyes")
# }}}

# --with-libyajl {{{
with_libyajl_cppflags=""
with_libyajl_ldflags=""
AC_ARG_WITH(libyajl, [AS_HELP_STRING([--with-libyajl@<:@=PREFIX@:>@], [Path to libyajl.])],
[
	if test "x$withval" != "xno" && test "x$withval" != "xyes"
	then
		with_libyajl_cppflags="-I$withval/include"
		with_libyajl_ldflags="-L$withval/lib"
		with_libyajl="yes"
	else
		with_libyajl="$withval"
	fi
],
[
	with_libyajl="yes"
])
if test "x$with_libyajl" = "xyes"
then
	SAVE_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $with_libyajl_cppflags"

	AC_CHECK_HEADERS(yajl/yajl_parse.h, [with_libyajl="yes"], [with_libyajl="no (yajl/yajl_parse.h not found)"])
	AC_CHECK_HEADERS(yajl/yajl_version.h)

	CPPFLAGS="$SAVE_CPPFLAGS"
fi
if test "x$with_libyajl" = "xyes"
then
	SAVE_CPPFLAGS="$CPPFLAGS"
	SAVE_LDFLAGS="$LDFLAGS"
	CPPFLAGS="$CPPFLAGS $with_libyajl_cppflags"
	LDFLAGS="$LDFLAGS $with_libyajl_ldflags"

	AC_CHECK_LIB(yajl, yajl_alloc, [with_libyajl="yes"], [with_libyajl="no (Symbol 'yajl_alloc' not found)"])

	CPPFLAGS="$SAVE_CPPFLAGS"
	LDFLAGS="$SAVE_LDFLAGS"
fi
if test "x$with_libyajl" = "xyes"
then
	BUILD_WITH_LIBYAJL_CPPFLAGS="$with_libyajl_cppflags"
	BUILD_WITH_LIBYAJL_LDFLAGS="$with_libyajl_ldflags"
	BUILD_WITH_LIBYAJL_LIBS="-lyajl"
	AC_SUBST(BUILD_WITH_LIBYAJL_CPPFLAGS)
	AC_SUBST(BUILD_WITH_LIBYAJL_LDFLAGS)
	AC_SUBST(BUILD_WITH_LIBYAJL_LIBS)
	AC_DEFINE(HAVE_LIBYAJL, 1, [Define if libyajl is present and usable.])
fi
AM_CONDITIONAL(BUILD_WITH_LIBYAJL, test "x$with_libyajl" = "xyes")
# }}}

# pkg-config --exists 'libxml-2.0'; pkg-config --exists libvirt {{{
with_libxml2="no (pkg-config isn't available)"
with_libxml2_cflags=""
with_libxml2_ldflags=""
with_libvirt="no (pkg-config isn't available)"
with_libvirt_cflags=""
with_libvirt_ldflags=""
$PKG_CONFIG --exists 'libxml-2.0' 2>/dev/null
if test "$?" = "0"
then
	with_libxml2="yes"
else
	with_libxml2="no (pkg-config doesn't know libxml-2.0)"
fi

$PKG_CONFIG --exists libvirt 2>/dev/null
if test "$?" = "0"
then
	with_libvirt="yes"
else
	with_libvirt="no (pkg-config doesn't know libvirt)"
fi
if test "x$with_libxml2" = "xyes"
then
	with_libxml2_cflags="`$PKG_CONFIG --cflags libxml-2.0`"
	if test $? -ne 0
	then
		with_libxml2="no"
	fi
	with_libxml2_ldflags="`$PKG_CONFIG --libs libxml-2.0`"
	if test $? -ne 0
	then
		with_libxml2="no"
	fi
fi
if test "x$with_libxml2" = "xyes"
then
	SAVE_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $with_libxml2_cflags"

	AC_CHECK_HEADERS(libxml/parser.h, [],
		      [with_libxml2="no (libxml/parser.h not found)"])

	CPPFLAGS="$SAVE_CPPFLAGS"
fi
if test "x$with_libxml2" = "xyes"
then
	SAVE_CFLAGS="$CFLAGS"
	SAVE_LDFLAGS="$LDFLAGS"

	CFLAGS="$CFLAGS $with_libxml2_cflags"
	LDFLAGS="$LDFLAGS $with_libxml2_ldflags"

	AC_CHECK_LIB(xml2, xmlXPathEval,
		     [with_libxml2="yes"],
		     [with_libxml2="no (symbol xmlXPathEval not found)"])

	CFLAGS="$SAVE_CFLAGS"
	LDFLAGS="$SAVE_LDFLAGS"
fi
dnl Add the right compiler flags and libraries.
if test "x$with_libxml2" = "xyes"; then
	BUILD_WITH_LIBXML2_CFLAGS="$with_libxml2_cflags"
	BUILD_WITH_LIBXML2_LIBS="$with_libxml2_ldflags"
	AC_SUBST(BUILD_WITH_LIBXML2_CFLAGS)
	AC_SUBST(BUILD_WITH_LIBXML2_LIBS)
fi
if test "x$with_libvirt" = "xyes"
then
	with_libvirt_cflags="`$PKG_CONFIG --cflags libvirt`"
	if test $? -ne 0
	then
		with_libvirt="no"
	fi
	with_libvirt_ldflags="`$PKG_CONFIG --libs libvirt`"
	if test $? -ne 0
	then
		with_libvirt="no"
	fi
fi
if test "x$with_libvirt" = "xyes"
then
	SAVE_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $with_libvirt_cflags"

	AC_CHECK_HEADERS(libvirt/libvirt.h, [],
		      [with_libvirt="no (libvirt/libvirt.h not found)"])

	CPPFLAGS="$SAVE_CPPFLAGS"
fi
if test "x$with_libvirt" = "xyes"
then
	SAVE_CFLAGS="$CFLAGS"
	SAVE_LDFLAGS="$LDFLAGS"

	CFLAGS="$CFLAGS $with_libvirt_cflags"
	LDFLAGS="$LDFLAGS $with_libvirt_ldflags"

	AC_CHECK_LIB(virt, virDomainBlockStats,
		     [with_libvirt="yes"],
		     [with_libvirt="no (symbol virDomainBlockStats not found)"])

	CFLAGS="$SAVE_CFLAGS"
	LDFLAGS="$SAVE_LDFLAGS"
fi
dnl Add the right compiler flags and libraries.
if test "x$with_libvirt" = "xyes"; then
	BUILD_WITH_LIBVIRT_CFLAGS="$with_libvirt_cflags"
	BUILD_WITH_LIBVIRT_LIBS="$with_libvirt_ldflags"
	AC_SUBST(BUILD_WITH_LIBVIRT_CFLAGS)
	AC_SUBST(BUILD_WITH_LIBVIRT_LIBS)
fi
# }}}

# $PKG_CONFIG --exists OpenIPMIpthread {{{
with_libopenipmipthread="yes"
with_libopenipmipthread_cflags=""
with_libopenipmipthread_libs=""

if test "x$with_libopenipmipthread" = "xyes"
then
	AC_MSG_CHECKING([for libOpenIPMIpthread])
	$PKG_CONFIG --exists OpenIPMIpthread 2>/dev/null
	if test "$?" != "0"
	then
		with_libopenipmipthread="no (pkg-config doesn't know OpenIPMIpthread)"
	fi
	AC_MSG_RESULT([$with_libopenipmipthread])
fi

if test "x$with_libopenipmipthread" = "xyes"
then
	AC_MSG_CHECKING([for libOpenIPMIpthread CFLAGS])
	temp_result="`$PKG_CONFIG --cflags OpenIPMIpthread`"
	if test "$?" = "0"
	then
		with_libopenipmipthread_cflags="$temp_result"
	else
		with_libopenipmipthread="no ($PKG_CONFIG --cflags OpenIPMIpthread failed)"
		temp_result="$PKG_CONFIG --cflags OpenIPMIpthread failed"
	fi
	AC_MSG_RESULT([$temp_result])
fi

if test "x$with_libopenipmipthread" = "xyes"
then
	AC_MSG_CHECKING([for libOpenIPMIpthread LDFLAGS])
	temp_result="`$PKG_CONFIG --libs OpenIPMIpthread`"
	if test "$?" = "0"
	then
		with_libopenipmipthread_ldflags="$temp_result"
	else
		with_libopenipmipthread="no ($PKG_CONFIG --libs OpenIPMIpthread failed)"
		temp_result="$PKG_CONFIG --libs OpenIPMIpthread failed"
	fi
	AC_MSG_RESULT([$temp_result])
fi

if test "x$with_libopenipmipthread" = "xyes"
then
	SAVE_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $with_libopenipmipthread_cflags"

	AC_CHECK_HEADERS(OpenIPMI/ipmi_smi.h,
			 [with_libopenipmipthread="yes"],
			 [with_libopenipmipthread="no (OpenIPMI/ipmi_smi.h not found)"],
[#include <OpenIPMI/ipmiif.h>
#include <OpenIPMI/ipmi_err.h>
#include <OpenIPMI/ipmi_posix.h>
#include <OpenIPMI/ipmi_conn.h>
])

	CPPFLAGS="$SAVE_CPPFLAGS"
fi

if test "x$with_libopenipmipthread" = "xyes"
then
	BUILD_WITH_OPENIPMI_CFLAGS="$with_libopenipmipthread_cflags"
	BUILD_WITH_OPENIPMI_LIBS="$with_libopenipmipthread_ldflags"
	AC_SUBST(BUILD_WITH_OPENIPMI_CFLAGS)
	AC_SUBST(BUILD_WITH_OPENIPMI_LIBS)
fi
# }}}

PKG_CHECK_MODULES([LIBNOTIFY], [libnotify],
		[with_libnotify="yes"],
		[with_libnotify="no (pkg-config doesn't know libnotify)"]
)

dnl ip_vs.h
if test "x$ac_system" = "xLinux" \
	&& test "x$have_linux_ip_vs_h$have_net_ip_vs_h$have_ip_vs_h" = "xnonono"
then
	enable_ipvs="$enable_ipvs (ip_vs.h not found)"
fi

if test "x$ip_vs_h_needs_kernel_cflags" = "xyes"
then
	enable_ipvs="$enable_ipvs (needs $KERNEL_CFLAGS)"
fi

dnl Perl bindings
PERL_BINDINGS_OPTIONS="PREFIX=${prefix}"
AC_ARG_WITH(perl-bindings, [AS_HELP_STRING([--with-perl-bindings@<:@=OPTIONS@:>@], [Options passed to "perl Makefile.PL".])],
[
	if test "x$withval" != "xno" && test "x$withval" != "xyes"
	then
		PERL_BINDINGS_OPTIONS="$withval"
		with_perl_bindings="yes"
	else
		with_perl_bindings="$withval"
	fi
],
[
	if test -n "$perl_interpreter"
	then
		with_perl_bindings="yes"
	else
		with_perl_bindings="no (no perl interpreter found)"
	fi
])

if test "x$with_perl_bindings" = "xyes"
then
	AC_MSG_CHECKING([for the ExtUtils::MakeMaker module])
	if $PERL -MExtUtils::MakeMaker -e '' 2>/dev/null; then
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
		with_perl_bindings="no (ExtUtils::MakeMaker not found)"
	fi
fi

if test "x$with_perl_bindings" = "xyes"
then
	PERL_BINDINGS="perl"
else
	PERL_BINDINGS=""
fi
AC_SUBST(PERL_BINDINGS)
AC_SUBST(PERL_BINDINGS_OPTIONS)

dnl libcollectdclient
LCC_VERSION_MAJOR=`echo $PACKAGE_VERSION | cut -d'.' -f1`
LCC_VERSION_MINOR=`echo $PACKAGE_VERSION | cut -d'.' -f2`
LCC_VERSION_PATCH=`echo $PACKAGE_VERSION | cut -d'.' -f3`

LCC_VERSION_EXTRA=`echo $PACKAGE_VERSION | cut -d'.' -f4-`

LCC_VERSION_STRING="$LCC_VERSION_MAJOR.$LCC_VERSION_MINOR.$LCC_VERSION_PATCH"

AC_SUBST(LCC_VERSION_MAJOR)
AC_SUBST(LCC_VERSION_MINOR)
AC_SUBST(LCC_VERSION_PATCH)
AC_SUBST(LCC_VERSION_EXTRA)
AC_SUBST(LCC_VERSION_STRING)

AC_CONFIG_FILES(src/libcollectdclient/collectd/lcc_features.h)

AM_CFLAGS="-Wall"
AM_CXXFLAGS="-Wall"
if test "x$enable_werror" != "xno"
then
        AM_CFLAGS="$AM_CFLAGS -Werror"
        AM_CXXFLAGS="$AM_CXXFLAGS -Werror"
fi
AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_CXXFLAGS])

AC_CONFIG_FILES([Makefile proto/Makefile src/Makefile src/daemon/Makefile src/collectd.conf src/libcollectdclient/Makefile src/libcollectdclient/libcollectdclient.pc src/liboconfig/Makefile bindings/Makefile bindings/java/Makefile])
AC_OUTPUT

if test "x$with_librrd" = "xyes" \
	&& test "x$librrd_threadsafe" != "xyes"
then
	with_librrd="yes (warning: librrd is not thread-safe)"
fi

if test "x$with_libperl" = "xyes"
then
	with_libperl="yes (version `$perl_interpreter -MConfig -e 'print $Config{version};'`)"
else
	enable_perl="no (needs libperl)"
fi

if test "x$enable_perl" = "xno" && test "x$c_cv_have_perl_ithreads" = "xno"
then
	enable_perl="no (libperl doesn't support ithreads)"
fi

if test "x$with_perl_bindings" = "xyes" \
	&& test "x$PERL_BINDINGS_OPTIONS" != "x"
then
	with_perl_bindings="yes ($PERL_BINDINGS_OPTIONS)"
fi

AC_MSG_RESULT()
AC_MSG_RESULT([Configuration:])
AC_MSG_RESULT([  Build:])
AC_MSG_RESULT([    Platform  . . . . . . $ac_system])
AC_MSG_RESULT([    CC  . . . . . . . . . $CC])
AC_MSG_RESULT([    CFLAGS  . . . . . . . $AM_CFLAGS $CFLAGS])
AC_MSG_RESULT([    CXXFLAGS  . . . . . . $AM_CXXFLAGS $CXXFLAGS])
AC_MSG_RESULT([    CPP . . . . . . . . . $CPP])
AC_MSG_RESULT([    CPPFLAGS  . . . . . . $CPPFLAGS])
AC_MSG_RESULT([    GRPC_CPP_PLUGIN . . . $GRPC_CPP_PLUGIN])
AC_MSG_RESULT([    LD  . . . . . . . . . $LD])
AC_MSG_RESULT([    LDFLAGS . . . . . . . $LDFLAGS])
AC_MSG_RESULT([    PROTOC  . . . . . . . $PROTOC])
AC_MSG_RESULT([    YACC  . . . . . . . . $YACC])
AC_MSG_RESULT([    YFLAGS  . . . . . . . $YFLAGS])
AC_MSG_RESULT()
AC_MSG_RESULT([  Libraries:])
AC_MSG_RESULT([    libcurl . . . . . . . $with_libcurl])
AC_MSG_RESULT([    libgcrypt . . . . . . $with_libgcrypt])
AC_MSG_RESULT([    libgrpc++ . . . . . . $with_libgrpcpp])
AC_MSG_RESULT([    libjvm  . . . . . . . $with_java])
AC_MSG_RESULT([    liblua  . . . . . . . $with_liblua])
AC_MSG_RESULT([    libnotify . . . . . . $with_libnotify])
AC_MSG_RESULT([    liboconfig  . . . . . $with_liboconfig])
AC_MSG_RESULT([    libopenipmi . . . . . $with_libopenipmipthread])
AC_MSG_RESULT([    libperl . . . . . . . $with_libperl])
AC_MSG_RESULT([    libpq . . . . . . . . $with_libpq])
AC_MSG_RESULT([    libprotobuf . . . . . $with_libprotobuf])
AC_MSG_RESULT([    libprotobuf-c . . . . $with_libprotobuf_c])
AC_MSG_RESULT([    libpthread  . . . . . $with_libpthread])
AC_MSG_RESULT([    libpython . . . . . . $with_libpython])
AC_MSG_RESULT([    librrd  . . . . . . . $with_librrd])
AC_MSG_RESULT([    libssl  . . . . .   . $with_libssl])
AC_MSG_RESULT([    libvirt . . . . . . . $with_libvirt])
AC_MSG_RESULT([    libxml2 . . . . . . . $with_libxml2])
AC_MSG_RESULT([    libyajl . . . . . . . $with_libyajl])
AC_MSG_RESULT([    protobuf-c  . . . . . $have_protoc_c])
AC_MSG_RESULT([    protoc 3  . . . . . . $have_protoc3])
AC_MSG_RESULT()
AC_MSG_RESULT([  Features:])
AC_MSG_RESULT([    daemon mode . . . . . $enable_daemon])
AC_MSG_RESULT([    debug . . . . . . . . $enable_debug])
AC_MSG_RESULT()
AC_MSG_RESULT([  Bindings:])
AC_MSG_RESULT([    perl  . . . . . . . . $with_perl_bindings])
AC_MSG_RESULT()

if test "x$dependency_error" = "xyes"; then
	AC_MSG_ERROR("Some plugins are missing dependencies - see the summary above for details")
fi

if test "x$dependency_warning" = "xyes"; then
	AC_MSG_WARN("Some plugins seem to have missing dependencies but have been enabled forcibly - see the summary above for details")
fi

# vim: set fdm=marker :
