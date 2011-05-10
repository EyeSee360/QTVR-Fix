//
//  QTVR_FixAppDelegate.m
//  QTVR Fix
//
//  Created by Michael Rondinelli on 5/10/11.
//  Copyright 2011 EyeSee360. All rights reserved.
//

#import "QTVR_FixAppDelegate.h"
#import "qtvrfix.h"

@implementation QTVR_FixAppDelegate

@synthesize window;
@synthesize title;
@synthesize progressBar;
@synthesize status;

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    // Insert code here to initialize your application
    [window makeKeyAndOrderFront:self];
    [window center];
}

- (IBAction)open:(id)sender
{
    NSOpenPanel *op = [NSOpenPanel openPanel];
    [op runModalForTypes:[NSArray arrayWithObjects:@"mov", nil]];
    
    [self application:NSApp openFiles:[op filenames]];
}

- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames
{
    [progressBar setMaxValue:[filenames count]];
    
    int result = 0;
    
    [title setStringValue:NSLocalizedString(@"Fixing movies...", @"")];
    
    for (NSString *filename in filenames) {
        [status setStringValue:[filename lastPathComponent]];

        result = qtvrfix([filename UTF8String]);
        if (result == 0) {
            [progressBar incrementBy:1.0];
        } else {
            [status setStringValue:[NSString stringWithFormat:NSLocalizedString(@"An error occurred: %d", @""), result]];
            NSBeep();
            break;
        }
    }
    [title setStringValue:NSLocalizedString(@"Finished!", @"")];
    
    NSBeep();
    [NSApp performSelector:@selector(terminate:) withObject:self afterDelay:2.0];
}

@end
