I have yet to actually implement proper docs, until I can get the program to a reasonable state. **Feel free to help contribute to the documentation though!**

## Commands

`nav create` - Creates datum.
`nav edit` - Edit datum.
`nav delete` - Deletes datum.
`nav info` - Displays info about a NAV datum.

### Data Types
The type of NAV datum you want to modify can be specified.

`file <filepath>` - The file. It is required to specify the NAV file for all commands, except test.
`area <ID / index>` - Nav area.
`ladder <ID / index>` - Ladder. Haven't actually set this type up yet.

**The below datum types require the area to be specified!**

* `connection <direction> <index>` - Connection.

* `encounter-path <index>` - Encounter path.
* `encounter-spot <index>` - Encounter spot. Encounter Path must be specified.

* `hide-spot <index>` - Hiding \[sic\] spot.

Some data types can also take in an ID number, which is a value prefixed with a hash tag '#'.