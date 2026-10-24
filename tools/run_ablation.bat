@echo off
setlocal
if "%~2"=="" (
    echo usage: run_ablation EXE OPTION
    exit /b 1
)
set "ENG=C:\Users\Vasile\Desktop\engines"
set "EXE=%~1"
set "OPT=%~2"
set "N=1"
set "PGN=%ENG%\%EXE%_%OPT%.pgn"

:find
if exist "%PGN%" goto bump
goto run
:bump
set /a N+=1
set "PGN=%ENG%\%EXE%_%OPT%_%N%.pgn"
goto find

:run
"C:\Program Files (x86)\Cute Chess\cutechess-cli.exe" ^
 -engine name=%OPT%_on cmd="%ENG%\knightlift_%EXE%.exe" option.%OPT%=true ^
 -engine name=%OPT%_off cmd="%ENG%\knightlift_%EXE%.exe" option.%OPT%=false ^
 -each proto=uci tc=8+0.08 option.Hash=16 ^
 -openings file="%ENG%\8moves_v3.pgn" format=pgn order=random ^
 -games 2 -rounds 5000 -repeat -concurrency 6 ^
 -sprt elo0=0 elo1=10 alpha=0.05 beta=0.05 ^
 -pgnout "%PGN%"