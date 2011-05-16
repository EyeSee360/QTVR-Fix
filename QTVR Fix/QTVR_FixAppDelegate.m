//
//  QTVR_FixAppDelegate.m
//  QTVR Fix
//
//  Created by Michael Rondinelli on 5/10/11.
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

#import "QTVR_FixAppDelegate.h"
#import "qtvrfix.h"

@implementation QTVR_FixAppDelegate

@synthesize window;
@synthesize title;
@synthesize results;
@synthesize lastError;

- (void)dealloc
{
    if (_pipeReadHandle) {
        [_pipeReadHandle release];
    }
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    // Insert code here to initialize your application
    [window makeKeyAndOrderFront:self];
    [window center];
    
    [self setResults:[NSMutableArray arrayWithCapacity:0]];
}

- (IBAction)open:(id)sender
{
    NSOpenPanel *op = [NSOpenPanel openPanel];
    [op setAllowsMultipleSelection:YES];
    [op runModalForTypes:[NSArray arrayWithObjects:@"mov", nil]];
    
    [self application:NSApp openFiles:[op filenames]];
}

- (void)handleStderr:(NSNotification *)notification
{
    [_pipeReadHandle readInBackgroundAndNotify];
    
    NSData *data = [[notification userInfo] objectForKey: NSFileHandleNotificationDataItem];
    NSString *str = [[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] autorelease];

    [self setLastError:str];
}

- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames
{
    int result = 0;
    
    [title setStringValue:NSLocalizedString(@"Fixing movies...", @"")];
    
    NSPipe *pipe = [NSPipe pipe];
    _pipeReadHandle = [pipe fileHandleForReading];
    dup2([[pipe fileHandleForWriting] fileDescriptor], fileno(stderr)) ;
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(handleStderr:) 
                                                 name:NSFileHandleReadCompletionNotification 
                                               object:_pipeReadHandle];
    [_pipeReadHandle readInBackgroundAndNotify];
    
    for (NSString *filename in filenames) {
        result = qtvrfix([filename UTF8String]);
        NSString *error = [self lastError];
        if (!error) error = @"";
        NSString *resultString = (result == 0) ? @"Fixed" : @"Error";
        
        NSDictionary *fileDict = [NSDictionary dictionaryWithObjectsAndKeys:
                                  [filename lastPathComponent], @"filename", 
                                  error, @"error",
                                  resultString, @"status",
                                  [NSNumber numberWithInt:result], @"resultCode",
                                  nil];
        [resultsController addObject:fileDict];
    }
    [title setStringValue:NSLocalizedString(@"Finished!", @"")];
    
    NSBeep();
}

@end
