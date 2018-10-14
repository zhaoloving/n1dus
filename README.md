# n1dus (ex dOPUS)
[![License: GPL v3](https://img.shields.io/badge/License-GPL%20v3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

dOPUS has been renamed n1dus.
n1dus is an installer and converter of NSP and XCI files for the Nintendo Switch. It is Based on Tinfoil and 4NXCI. 

DISCLAIMER: Use n1dus at your own risk. I'm not responsible for any damage, data loss, or anything else n1dus may cause.

## Features
* File browser like interface
* Install or "Install & Delete" NSP, XCI
* Install an extracted NSP or XCI (NCA folder)
* Extract NSP, XCI to a folder
* Convert XCI to NSP
* Can install NSP and XCI (split) files larger than 4GB on a FAT32 SD Card. No more need for exFAT :)


## Installation
* Place n1dus.nro in /switch folder on your sd card.
* Requires `sdmc:/keys.dat` in order to extract/install XCI files. 

Keys can be extracted using [tesnos' kezplez](https://github.com/tesnos/kezplez-nx/tree/v1.0) or [shchmue's fork of kezplez](https://github.com/shchmue/kezplez-nx/releases). Hers supports the fuses and TSEC being dumped in multiple locations as well as also supporting firmware 6.0.


## Usage
* Launch n1dus via hbmenu.
* Follow the on screen instructions.


## Installing files over 4GB
* Split files using either AnalogMan151 [splitNSP](https://github.com/AnalogMan151/splitNSP), or my slightly modified version in tools/splitFile.py (will handle both XCI and NSP file extensions, and set the archive bit for you under windows). 
* Make sure the generated folder has the right extension (XCI or NSP), and has the archive bit set ("Folder is ready for archiving" under windows). 
* The generated folder should show up under n1dus as regular NSP or XCI file.
* Install as you would normally do with n1dus!


## Screenshots
TODO

## Thanks to...
* SciresM
* Adubbz
* The-4n
* AnalogMan151
* mbedtls
* Team Reswitched
* Team Switchbrew
* The switch community

## Licence
GNU General Public License v3.0
