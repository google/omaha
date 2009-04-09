@echo off

rem normally, only 127 patches can be applied to an msi

set /A ii=0

rem msiexec /i GoogleUpdateHelper.msi /qn
rem echo original product installed

:repeat

msiexec /update GoogleUpdateHelperPatch.msp REINSTALL=ALL /qn /L*v patchapply%ii%.log
echo patch %ii% applied
if %ii% GEQ 127	pause
msiexec /uninstall {E0D0D2C9-5836-4023-AB1D-54EC3B90AD03} /package {A92DAB39-4E2C-4304-9AB6-BC44E68B55E2} /qn /L*v patchremove%ii%.log
echo patch %ii% removed

set /A ii=%ii%+1

if %ii% NEQ 300	goto repeat
rem if %ii% NEQ 127	goto repeat

set ii=
