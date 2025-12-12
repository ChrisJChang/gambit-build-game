See CLAUDE.md for instructions on how to build the game executable.

To run with a gambit build, put your gambit copy folder in the same place as this gambit-build-game, and run:
```bash
bash BuildAndPlay.sh
```

where compile_gambit.sh can be edited to match your favourite build.

I put a timeout on the build of 100s for now because it does not need to actually get all the way.

Current Problem: Getting gambit to stop building after the game is stopped prematurely does not happen.
