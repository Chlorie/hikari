# Hikari

A funny way to notate music excerpts. Take a look at the documentations [here](https://hikari-music.readthedocs.io/).

Some day in the past I saw a video on Bilibili about a QQ chat bot that supports 8-bit music playback with a simple grammar for music notation. Thus I thought to myself, how about making a similar feature but supporting more complex music? And the piano playback feature (`/p`) was then implemented in one of my QQ chat bots (HikariBot, source code not yet made public). The original HikariBot `/p` format supported basic polyphony, arbitrary tuplets, transpositions, changing of tempi, and really primitive error checking.

But due to the hackiness of the original code, the conversion from notation to synthesized music was handmade by myself and may contain many bugs. This project aims to implement a better format based on the existing one, maybe more features will also be added. I decided to rewrite it in C++ just because I want to.

## Plans

- [x] Basic grammar
- [x] Macros
- [x] Better diagnostics
- [ ] Repeats
- [ ] Conversion to Lilypond
