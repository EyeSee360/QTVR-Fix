//
//  main.c
//  qtvrfix
//
//  Created by Michael Rondinelli on 5/8/11.
//  Copyright 2011 EyeSee360. All rights reserved.
//

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

