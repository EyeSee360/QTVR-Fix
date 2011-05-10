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
    NSProgressIndicator *progressBar;
    NSTextField *status;
}

@property (assign) IBOutlet NSWindow *window;
@property (assign) IBOutlet NSTextField *title;
@property (assign) IBOutlet NSProgressIndicator *progressBar;
@property (assign) IBOutlet NSTextField *status;

- (IBAction) open:(id)sender;

@end
