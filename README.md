# Cypress
Cypress is an Open-Source reimplementation of Dedicated Servers for PvZ: Garden Warfare 1 and 2, based off of [KYBER V1 (a Private Server project for Star Wars: Battlefront 2).](https://github.com/ArmchairDevelopers/Kyber)

# Credits
* [KYBER](https://github.com/ArmchairDevelopers/Kyber) - Without their reimplementation of Frostbite's Socket Manager, this project would likely not exist.
* [Andersson799](https://github.com/Andersson799) - He figured out how to reimplement Dedicated Server functionality in Frostbite games, and without his amazing work, this project would not be able to host dedicated servers.

# License
See [LICENSE.txt](LICENSE.txt)

# Building
Cypress should build out out of the box as long as you're using Visual Studio 2026.

# Usage
TODO - more in depth instructions here

Cypress is only compatible with v1.0.12 of Garden Warfare 2, and the latest version of Garden Warfare 1 (v1.0.3.0)
After compiling the project, place the compiled dll in the game's directory and rename it to dinput8.dll

An example batch file that launches a dedicated server can be found at Examples/Start_Dedicated.bat
When run from the game directory, this will launch a Team Vanquish server on Zomboss Factory, with a max player count of 48.

For joining, see Examples/Start_Join.bat
