//
//  QTVR_FixAppDelegate.h
//  QTVR Fix
//
//  Created by Michael Rondinelli on 5/10/11.
//  Copyright 2011 EyeSee360. All rights reserved.
//

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
