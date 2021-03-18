$!
$! BUILD.COM
$!
$ WRITE SYS$OUTPUT "Compiling..."
$ CC tpcfix.c
$!          
$ WRITE SYS$OUTPUT "Linking..."
$ LINK tpcfix.obj, SYS$INPUT:/OPTIONS       
  SYS$SHARE:VAXCRTL.EXE/SHAREABLE 
$!
