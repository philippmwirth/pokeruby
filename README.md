(This is a fork from https://github.com/pret/)

# Planned Changes

## Version 1.0

### Advanced Rules
1) No heal/boost items during battle.
2) Battle mode to "BattleModeSet".

### No Trade Evolutions
All trade evolutions are replaced by friendship evolutions.

### Stronger Trainers
Improve IVs of trainer Pokemon and change AI Flags to 0x7 for all trainers.

### Complete Hoenn Dex
All Pokemon from the Hoenn Dex can be caught in both games (no trades necessary).

### Access Mystery Island
Mystery Island appears more often.

### Improved Battle AI
Improve AI such that:
1) Trainers can make intelligent switches if they are at a disadvantage.
2) Trainers send in the Pokemon which pressures their opponents the most.

TODO: Add diagram explaining the decision flow.

### Weather in Gyms
The following gyms have weather effects:
- Rustboro City Gym (sandstorm)
- Mauville City Gym (heavy rain)
- Lavaridge Town Gym (drought)
- Sootopolis City Gym (light rain)

### No Overleveling
When a Pokemon has a higher level than the strongest Pokemon of the next gym leader (or elite four member), it's disobedient.

## Version 2.0

### Help the Weak
The following Pokemon have higher base stats:

**Slugma**

Stat | Before | After
-----|--------|------
HP | 40 | 80
Atk | 40 | 40
Def | 40 | 70
SpAtk | 70 | 70
SpDef | 40 | 70
Speed | 20 | 20
Total | 250 | 350

Slugma also receives the secondary type "Stone".

**Magcargo**

Stat | Before | After
-----|--------|------
HP | 60 | 100
Atk | 50 | 50
Def | 120 | 120
SpAtk | 90 | 100
SpDef | 80 | 90
Speed | 30 | 20
Total | 430 | 480

**Corsola**

Stat | Before | After
-----|--------|------
HP | 65 | 105
Atk | 55 | 55
Def | 95 | 105
SpAtk | 65 | 75
SpDef | 95 | 105
Speed | 35 | 35
Total | 410 | 480

**Masquerain**
Stat | Before | After
-----|--------|------
HP | 70 | 70
Atk | 60 | 80
Def | 62 | 60
SpAtk | 100 | 120
SpDef | 82 | 80
Speed | 80 | 90
Total | 454 | 500

Masquerains typing is changed to "Bug/Water".   

### Improved Trainer Teams
Gym leaders, elite four members, and team aqua/magma have more balanced and stronger teams.


# Pokémon Ruby and Sapphire [![Build Status][travis-badge]][travis]

This is a disassembly of Pokémon Ruby and Sapphire.

It builds the following roms:

* pokeruby.gba `sha1: f28b6ffc97847e94a6c21a63cacf633ee5c8df1e`
* pokesapphire.gba `sha1: 3ccbbd45f8553c36463f13b938e833f652b793e4`

To set up the repository, see [INSTALL.md](INSTALL.md).

## See also

* Disassembly of [**Pokémon Red/Blue**][pokered]
* Disassembly of [**Pokémon Yellow**][pokeyellow]
* Disassembly of [**Pokémon Gold**][pokegold]
* Disassembly of [**Pokémon Crystal**][pokecrystal]
* Disassembly of [**Pokémon Pinball**][pokepinball]
* Disassembly of [**Pokémon TCG**][poketcg]
* Disassembly of [**Pokémon Fire Red**][pokefirered]
* Disassembly of [**Pokémon Emerald**][pokeemerald]
* Discord: [**pret**][Discord]
* irc: **irc.freenode.net** [**#pret**][irc]

[pokered]: https://github.com/pret/pokered
[pokeyellow]: https://github.com/pret/pokeyellow
[pokegold]: https://github.com/pret/pokegold
[pokecrystal]: https://github.com/pret/pokecrystal
[pokepinball]: https://github.com/pret/pokepinball
[poketcg]: https://github.com/pret/poketcg
[pokefirered]: https://github.com/pret/pokefirered
[pokeemerald]: https://github.com/pret/pokeemerald
[Discord]: https://discord.gg/d5dubZ3
[irc]: https://kiwiirc.com/client/irc.freenode.net/?#pret
[travis]: https://travis-ci.org/pret/pokeruby
[travis-badge]: https://travis-ci.org/pret/pokeruby.svg?branch=master
