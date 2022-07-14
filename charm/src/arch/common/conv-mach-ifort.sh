# for Intel Fortran compiler 8.0 and higher which is renamed to ifort from ifc
# does not work for ifc 7.0
CMK_CF77="ifort -auto -fpic "
CMK_CF90="ifort -auto -fpic "
CMK_CF90_FIXED="$CMK_CF90 -132 -FI "
F90DIR=`command -v ifort 2> /dev/null`
if test -x "$F90DIR" 
then
  MYDIR="$PWD"
  cd `dirname "$F90DIR"`
  if test -L 'ifort'
  then
    F90DIR=`readlink ifort`
    cd `dirname "$F90DIR"`
  fi
  F90DIR=`pwd -P`
  cd "$MYDIR"

  Minor=`basename $F90DIR`
  F90LIBDIR="$F90DIR/../lib/$Minor"
  if ! test -x "$F90LIBDIR"
  then
    F90LIBDIR="$F90DIR/../lib"
    if ! test -x "$F90LIBDIR"
    then
      F90LIBDIR="$F90DIR/../../compiler/lib/$Minor"
    fi
    if ! test -x "$F90LIBDIR"
    then
      F90LIBDIR="$F90DIR/../../lib/$Minor"
    fi
    if ! test -x "$F90LIBDIR"
    then
      F90LIBDIR="$F90DIR/../../compiler/lib/${Minor}_lin"
    fi
    if ! test -x "$F90LIBDIR"
    then
      F90LIBDIR="$F90DIR/../../lib/${Minor}_lin"
    fi
  fi
  F90MAIN="$F90LIBDIR/for_main.o"
fi
# for_main.o is important for main() in f90 code
if test -z "$ICC_STATIC"
then
CMK_F90MAINLIBS="$F90MAIN "
CMK_F90LIBS="-L$F90LIBDIR -lifcore -lifport -lifcore "
else
CMK_F90LIBS="$F90LIBDIR/libifcore.a $F90LIBDIR/libifport.a $F90LIBDIR/libifcore.a "
fi
CMK_F77LIBS="$CMK_F90LIBS"

CMK_F90_USE_MODDIR=""
