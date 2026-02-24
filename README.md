## MQRemote

MQRemote is a modernized remote communication plugin for MacroQuest, inspired by EQBC and MQ2DanNet. It provides reliable, multi-channel text communication between clients with a cleaner internal design, and tighter integration with contemporary MacroQuest workflows using its built-in actors system.

## Getting Started
Load the plugin like any other MacroQuest plugin:

```txt
/plugin MQRemote
```

The plugin initializes automatically on load. Default channels (Globla, Server, Group, Raid, Zone) are created dynamically as they become available in-game. No additional setup is required for basic functionality.

### Commands
MQRemote supports multiple logical communication channels. All commands use the /rc prefix.

#### Built-in Channels
All channels follow the pattern of `/rc [+self] <channel> <message>` where `+self` is optional.
or `/rc <channel> <character> <message>` to send a tell to just that character in a channel (most used will probably be server channel).
##### Global Channel
The global channel is always available
```
/rc global <message>        - Send a command to the global channel excluding self
/rc +self global <message>  - Send a command to the global channel message including self
/rc global name <message>   - Send a command to the global channel to the specific named character
```

##### Server Channel
The server channel is available from character select screen and onwards. Use this to limit recievers to be only the other characters logged into the same server.
```
/rc server <message>        - Send a command to the server channel excluding self
/rc +self server <message>  - Send a command to the server channel message including self
/rc server name <message>   - Send a command to the server channel to the specific named character

```

##### Zone Channel
The zone channel is available once the character is registered as being ingame. Use this to limit recievers to be only the other characters who are in the same zone.
```
/rc zone <message>        - Send a command to the zone channel excluding self
/rc +self zone <message>  - Send a command to the zone channel message including self
/rc zone name <message>   - Send a command to the zone channel to the specific named character
```

##### Group Channel
The group channel is available whenever the character is in a group. Sends a command only to the characters in the same group.
```
/rc group <message>        - Send a command to the group channel excluding self
/rc +self group <message>  - Send a command to the group channel message including self
/rc group name <message>   - Send a command to the group channel to the specific named character
```

##### Raid Channel
The group channel is available whenever the character is in a raid. Sends a command only to the characters in the same raid.
```
/rc raid <message>        - Send a command to the raid channel excluding self
/rc +self raid <message>  - Send a command to the raid channel message including self
/rc raid name <message>   - Send a command to the raid channel to the specific named character
```

##### Class Channel
The class channel is available once the character is registered as being ingame. Use this to limit recievers to be only the other characters who are of the same class.
```
/rc class <message>        - Send a command to the class channel excluding self
/rc +self class <message>  - Send a command to the class channel message including self
/rc class name <message>   - Send a command to the class channel to the specific named character
```

#### Custom Channels
You can also create and use custom channels dynamically. Channels may be marked as auto (default) or noauto to persist in settings if the channel should be automatically joined by the character.
```
# create
/rcjoin <channel> [auto|noauto]

#leave
/rcleave <channel> [auto|noauto]

#use
/rc [+self] <channel> <message>
/rc <channel> <name> <message>
```

### Configuration File
A configuration file for storing custom channels that should be automatically joined has the following setup:

```ini
[Winnythepoo]
honeyjar=1
bees=0

[Pigglet]
forrest=1
```

The ini is on a per serer basis: `MQRemote_SeverShortName.ini`

### Examples
Sending commands to other toons: 
```
/rc server ToonName /sit
/rc server ToonName /stand
/rc server ToonName /macro ninjalooter
/rc server ToonName /endmacro
```

Make a channel called "clerics". On each character you want in the channel, type:
```
/rcjoin clerics noauto
```

To make all characters in the channel "clerics" say "I am a cleric",
```
/rc clerics /say I am a cleric
```

Sending commands to all other connects clients: 
```
/rc global /target id ${Me.ID}
```

To have the recieving client parse MQ data use `noparse`
```
/noparse /rc +self global /echo I am ${Me.PctExp} into ${Me.Level}
```

## Authors
* PeonMQ - *Initial work*

See also the list of [contributors](https://github.com/peonmq/mqremote/contributors.md) who participated in this project.

## Acknowledgments
* Inspiration from EQBC and MQ2DanNet
