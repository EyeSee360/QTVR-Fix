//
//  QTVR_FixAppDelegate.h
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

#import <Cocoa/Cocoa.h>

@interface QTVR_FixAppDelegate : NSObject <NSApplicationDelegate> {
@private
    NSWindow *window;
    NSTextField *title;
    NSMutableArray *results;
    NSString *lastError;
    NSArrayController *resultsController;
    
    NSFileHandle *_pipeReadHandle;
}

@property (assign) IBOutlet NSWindow *window;
@property (assign) IBOutlet NSTextField *title;
@property (nonatomic, retain) NSMutableArray *results;
@property (nonatomic, copy) NSString *lastError;
@property (nonatomic, retain) IBOutlet NSArrayController *resultsController;

- (IBAction) open:(id)sender;

@end
