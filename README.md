## NetLimiter for Garry's Mod servers

This module is an implementation of the net_chan_limit_msec convar that is available to CSGO and TF2 servers. <br>
This module allows you to limit the amount of processing time a player can use for networking effectively killing all flooding exploits. It works by detouring the ProcessMessages function that handles all networking.<br>
This module is also designed to be as simplified as possible to insure that it is optimized.

## Usage.

Place the DLL in lua/bin. The current pre-compiled version is for 64 bit only.<br>
Create an file in lua/autorun/server that runs ``require("netlimiter")`` and define ``net_chan_limit_msec 100`` in your server.cfg. You can change the limit to what suites your server best.

## Compilation
To Compile this project you will need [garrysmod_common][1]. I have supplied a 64 bit dll in the releases.  


## Credits other than myself
[Asriel][2]: Helped me get the detour working and with implementing the ratelimiting itself. Was good fun creating this module. <br>
[Daniel][3]: He is the creator of garrysmod_common which makes it easier for developers to create modules for Garry's Mod and also created a module called sourcenet(My favourite module) that gave a good insight into source engine networking.
  
[1]: https://github.com/danielga/garrysmod_common
[2]: https://github.com/A5R13L
[3]: https://github.com/danielga

