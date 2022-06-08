# Minetest Major Breakages List

This document contains a list of breaking changes to be made in the next major version.

* Remove attachment space multiplier (*10)
* `get_sky()` returns a table (without arg)
* `game.conf` name/id mess
* remove `depends.txt` / `description.txt` (would simplify ContentDB and Minetest code a little)
* flip moon texture by 180°, make it coherent with the sun (see https://github.com/minetest/minetest/pull/11902)
