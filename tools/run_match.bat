@echo off
setlocal
if "%~2"=="" (
    echo usage: run_match NEW OLD [depth]
    exit /b 1
)
set "ENG=C:\Users\Vasile\Desktop\engines"
set "NEW=%~1"
set "OLD=%~2"
set "N=1"
set "PGN=%ENG%\%NEW%_vs_%OLD%.pgn"

:find
if exist "%PGN%" goto bump
goto run
:bump
set /a N+=1
set "PGN=%ENG%\%NEW%_vs_%OLD%_%N%.pgn"
goto find

:run
if "%~3"=="" (
    set "TC=tc=8+0.08"
    set "EXTRA=-rounds 5000 -sprt elo0=0 elo1=10 alpha=0.05 beta=0.05"
) else (
    set "TC=tc=inf depth=%~3"
    set "EXTRA=-rounds 200"
)

"C:\Program Files (x86)\Cute Chess\cutechess-cli.exe" ^
 -engine name=%NEW% cmd="%ENG%\knightlift_%NEW%.exe" ^
 -engine name=%OLD% cmd="%ENG%\knightlift_%OLD%.exe" ^
 -each proto=uci %TC% option.Hash=16 ^
 -openings file="%ENG%\8moves_v3.pgn" format=pgn order=random ^
 -games 2 -repeat -concurrency 6 %EXTRA% ^
 -pgnout "%PGN%"