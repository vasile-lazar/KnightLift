@echo off
setlocal
if "%~4"=="" (
    echo usage: run_param EXE OPTION NEWVALUE OLDVALUE
    exit /b 1
)
set "ENG=C:\Users\Vasile\Desktop\engines"
set "EXE=%~1"
set "OPT=%~2"
set "NEWV=%~3"
set "OLDV=%~4"
set "N=1"
set "PGN=%ENG%\%EXE%_%OPT%_%NEWV%_vs_%OLDV%.pgn"

:find
if exist "%PGN%" goto bump
goto run
:bump
set /a N+=1
set "PGN=%ENG%\%EXE%_%OPT%_%NEWV%_vs_%OLDV%_%N%.pgn"
goto find

:run
"C:\Program Files (x86)\Cute Chess\cutechess-cli.exe" ^
 -engine name=%OPT%_%NEWV% cmd="%ENG%\knightlift_%EXE%.exe" option.%OPT%=%NEWV% ^
 -engine name=%OPT%_%OLDV% cmd="%ENG%\knightlift_%EXE%.exe" option.%OPT%=%OLDV% ^
 -each proto=uci tc=8+0.08 option.Hash=16 ^
 -openings file="%ENG%\8moves_v3.pgn" format=pgn order=random ^
 -games 2 -rounds 5000 -repeat -concurrency 6 ^
 -sprt elo0=0 elo1=10 alpha=0.05 beta=0.05 ^
 -pgnout "%PGN%"