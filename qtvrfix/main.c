//
//  main.c
//  qtvrfix
//
//  Created by Michael Rondinelli on 5/8/11.
//  Copyright 2011 EyeSee360. All rights reserved.
//
/*
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include "qtvrfix.h"


int main (int argc, const char * argv[])
{

    if (argc == 1) {
        printf("usage: qtvrfix [qtvr.mov ...]\n");
        printf("       Modifies the specified QTVR movie files in-place to fix a crashing bug which \n");
        printf("       occurs when the movie is played using QuickTime 7.6.9 or later.\n");
        printf("       The modifications are backwards-compatible. Non-QTVR movies will not be affected.\n");
    } else {
        for (int i = 1; i < argc; i++) {
            qtvrfix(argv[i]);
        }
    }
    
    return 0;
}

