dnl #
dnl # LZFS_AC_META
dnl # Read metadata from the META file.
dnl #
dnl # AUTHOR:
dnl # Chris Dunlap <cdunlap@llnl.gov>
dnl # Brian Behlendorf <behlendorf1@llnl.gov>
dnl #
AC_DEFUN([LZFS_AC_META], [
	AC_MSG_CHECKING([metadata])

	META="$srcdir/META"
	_spl_ac_meta_got_file=no
	if test -f "$META"; then
		_spl_ac_meta_got_file=yes

		LZFS_META_NAME=_LZFS_AC_META_GETVAL([(?:NAME|PROJECT|PACKAGE)]);
		if test -n "$LZFS_META_NAME"; then
			AC_DEFINE_UNQUOTED([LZFS_META_NAME], ["$LZFS_META_NAME"],
				[Define the project name.]
			)
			AC_SUBST([LZFS_META_NAME])
		fi

		LZFS_META_VERSION=_LZFS_AC_META_GETVAL([VERSION]);
		if test -n "$LZFS_META_VERSION"; then
			AC_DEFINE_UNQUOTED([LZFS_META_VERSION], ["$LZFS_META_VERSION"],
				[Define the project version.]
			)
			AC_SUBST([LZFS_META_VERSION])
		fi

		LZFS_META_RELEASE=_LZFS_AC_META_GETVAL([RELEASE]);
		if test -n "$LZFS_META_RELEASE"; then
			AC_DEFINE_UNQUOTED([LZFS_META_RELEASE], ["$LZFS_META_RELEASE"],
				[Define the project release.]
			)
			AC_SUBST([LZFS_META_RELEASE])
		fi

		if test -n "$LZFS_META_NAME" -a -n "$LZFS_META_VERSION"; then
				LZFS_META_ALIAS="$LZFS_META_NAME-$SPL_META_VERSION"
				test -n "$LZFS_META_RELEASE" && 
				        LZFS_META_ALIAS="$LZFS_META_ALIAS-$SPL_META_RELEASE"
				AC_DEFINE_UNQUOTED([LZFS_META_ALIAS],
					["$LZFS_META_ALIAS"],
					[Define the project alias string.] 
				)
				AC_SUBST([LZFS_META_ALIAS])
		fi

		LZFS_META_DATA=_LZFS_AC_META_GETVAL([DATE]);
		if test -n "$LZFS_META_DATA"; then
			AC_DEFINE_UNQUOTED([LZFS_META_DATA], ["$LZFS_META_DATA"],
				[Define the project release date.] 
			)
			AC_SUBST([LZFS_META_DATA])
		fi

		LZFS_META_AUTHOR=_LZFS_AC_META_GETVAL([AUTHOR]);
		if test -n "$LZFS_META_AUTHOR"; then
			AC_DEFINE_UNQUOTED([LZFS_META_AUTHOR], ["$LZFS_META_AUTHOR"],
				[Define the project author.]
			)
			AC_SUBST([LZFS_META_AUTHOR])
		fi

		m4_pattern_allow([^LT_(CURRENT|REVISION|AGE)$])
		LZFS_META_LT_CURRENT=_LZFS_AC_META_GETVAL([LT_CURRENT]);
		LZFS_META_LT_REVISION=_LZFS_AC_META_GETVAL([LT_REVISION]);
		LZFS_META_LT_AGE=_LZFS_AC_META_GETVAL([LT_AGE]);
		if test -n "$LZFS_META_LT_CURRENT" \
				 -o -n "$LZFS_META_LT_REVISION" \
				 -o -n "$LZFS_META_LT_AGE"; then
			test -n "$LZFS_META_LT_CURRENT" || LZFS_META_LT_CURRENT="0"
			test -n "$LZFS_META_LT_REVISION" || LZFS_META_LT_REVISION="0"
			test -n "$LZFS_META_LT_AGE" || LZFS_META_LT_AGE="0"
			AC_DEFINE_UNQUOTED([LZFS_META_LT_CURRENT],
				["$LZFS_META_LT_CURRENT"],
				[Define the libtool library 'current'
				 version information.]
			)
			AC_DEFINE_UNQUOTED([LZFS_META_LT_REVISION],
				["$LZFS_META_LT_REVISION"],
				[Define the libtool library 'revision'
				 version information.]
			)
			AC_DEFINE_UNQUOTED([LZFS_META_LT_AGE], ["$LZFS_META_LT_AGE"],
				[Define the libtool library 'age' 
				 version information.]
			)
			AC_SUBST([LZFS_META_LT_CURRENT])
			AC_SUBST([LZFS_META_LT_REVISION])
			AC_SUBST([LZFS_META_LT_AGE])
		fi
	fi

	AC_MSG_RESULT([$_spl_ac_meta_got_file])
	]
)

AC_DEFUN([_LZFS_AC_META_GETVAL], 
	[`perl -n\
		-e "BEGIN { \\$key=shift @ARGV; }"\
		-e "next unless s/^\s*\\$key@<:@:=@:>@//i;"\
		-e "s/^((?:@<:@^'\"#@:>@*(?:(@<:@'\"@:>@)@<:@^\2@:>@*\2)*)*)#.*/\\@S|@1/;"\
		-e "s/^\s+//;"\
		-e "s/\s+$//;"\
		-e "s/^(@<:@'\"@:>@)(.*)\1/\\@S|@2/;"\
		-e "\\$val=\\$_;"\
		-e "END { print \\$val if defined \\$val; }"\
		'$1' $META`]dnl
)
