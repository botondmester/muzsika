# Muzsika 
This discord bot is written in c++ using a library called [D++](https://github.com/brainboxdotcc/DPP) for interacting with the discord API.
It was written in about 3 hours or so, so don't expect much, it just works.

The bot plays mp3 files and supports a number of commands:
- /ping: replies with pong
- /join: joins the user's voice channel
- /mp3: accepts an pm3 file as an attachment and adds it to the queue
- /queue: prints out the current song and the ones in the queue
- /pause: pauses the playback
- /resume: resumes playback
- /skip: skips the current song (if looping is enabled, it leaves the song in the queue)
- /loop True/False: loops the whole queue if enabled
Currently the bot won't leave the channel by itself

If you have any ideas or suggestions, let me know

## Template 

This [D++](https://github.com/brainboxdotcc/DPP) Discord Bot was created using a template for Visual Studio 2022 (x64 and x86, release and debug). The template is the result of [this tutorial](https://dpp.dev/build-a-discord-bot-windows-visual-studio.html) with additional enhancements for automatic selection of the correct architecture, and copying of the dll files into the correct folders on successful build.

For support and assistance about the original template please join [the official support discord](https://discord.gg/dpp).
