//
//  ATKViewController.h
//  ATK
//
//  Created by Daniel Heffernan on 11/07/13.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface ATKViewController : UIViewController {
	NSString *currentPrompt;
	
	IBOutlet UILabel *promptLabel;
	IBOutlet UILabel *outLabel;
	IBOutlet UILabel *quantityLabel;
	IBOutlet UIImageView *plate;
	IBOutlet UIImageView *base;
	IBOutlet UIImageView *pepperoni;
	IBOutlet UIImageView *tomato;
	IBOutlet UIImageView *ham;
	IBOutlet UIImageView *cheese;
}

@property (nonatomic, readwrite, copy) NSString *currentPrompt;

@end

