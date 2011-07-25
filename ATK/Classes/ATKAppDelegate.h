//
//  ATKAppDelegate.h
//  ATK
//
//  Created by Daniel Heffernan on 11/07/13.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import <UIKit/UIKit.h>

@class ATKViewController;

@interface ATKAppDelegate : NSObject <UIApplicationDelegate> {
    UIWindow *window;
    ATKViewController *viewController;
}

@property (nonatomic, retain) IBOutlet UIWindow *window;
@property (nonatomic, retain) IBOutlet ATKViewController *viewController;

@end

