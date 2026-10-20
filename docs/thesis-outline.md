# Structura lucrării (schiță)

1. Introducere: motivație, obiective, structura lucrării
2. Fundamente teoretice
    - Reprezentări ale tablei (mailbox, 0x88, bitboards)
    - Căutare: minimax, alpha-beta, îmbunătățiri
    - Evaluare clasică vs. rețele neuronale (NNUE)
    - Protocolul UCI
3. KnightLift Chess Engine
   3.1 Reprezentarea tablei și generarea mutărilor   <- stage 1 (done)
   3.2 Protocolul UCI                                <- stage 2 (done)
   3.3 Căutarea
   3.4 Evaluarea
   3.5 Rețea neuronală NNUE (antrenare, inferență)
4. Infrastructura de testare (perft, SPRT, CI)
5. Aplicația web (arhitectură, backend, frontend)
6. Rezultate experimentale
7. Concluzii și direcții viitoare