//
//  ATKViewController.m
//  ATK
//
//  Created by Daniel Heffernan on 11/07/13.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import "ATKViewController.h"

#include <pthread.h>
#include "APacket.h"

extern void *startSSDS(void *);


@implementation ATKViewController

@synthesize currentPrompt;


/*
// The designated initializer. Override to perform setup that is required before the view is loaded.
- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil {
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        // Custom initialization
    }
    return self;
}
*/

/*
// Implement loadView to create a view hierarchy programmatically, without using a nib.
- (void)loadView {
}
*/

- (void)setOutLabelAlpha:(NSNumber *)value {
	outLabel.alpha = [value floatValue];
}

- (void)clearPlate {
	[UIView animateWithDuration:0.3f animations:^{
		base.alpha = 0.0f;
		pepperoni.alpha = 0.0f;
		tomato.alpha = 0.0f;
		ham.alpha = 0.0f;
		cheese.alpha = 0.0f;
	}];
}

- (void)processWord:(NSString *)word {
	[UIView animateWithDuration:0.3f animations:^{	
		if ([self.currentPrompt isEqualToString:@"quantity"]) {
			if ([word isEqualToString:@"ONE"]) quantityLabel.text = @"x1";
			if ([word isEqualToString:@"TWO"]) quantityLabel.text = @"x2";
			if ([word isEqualToString:@"THREE"]) quantityLabel.text = @"x3";
		} else if ([self.currentPrompt isEqualToString:@"topping"]) {
			base.alpha = 1.0f;
			if ([word isEqualToString:@"PEPPERONI"])	pepperoni.alpha = 1.0f;
			if ([word isEqualToString:@"TOMATO"])		tomato.alpha = 1.0f;
			if ([word isEqualToString:@"HAM"])			ham.alpha = 1.0f;
			if ([word isEqualToString:@"CHEESE"])		cheese.alpha = 1.0f;
		}
	}];
}

// Implement viewDidLoad to do additional setup after loading the view, typically from a nib.
- (void)viewDidLoad {
    [super viewDidLoad];
	
	[[NSNotificationCenter defaultCenter] addObserverForName:@"ARec::OutPacket" object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *notification){
		NSDictionary *d = [notification userInfo];
		NSNumber *type = [d objectForKey:@"PhraseType"];
		
		NSString *word = [d objectForKey:@"Word"];
		NSString *tag = [d objectForKey:@"Tag"];
		
		PhraseType k = PhraseType([type intValue]);
		NSMutableString *rdline = [NSMutableString stringWithString:outLabel.text];
		
		BOOL complete = NO;
		switch(k){
			case Start_PT:
				[rdline deleteCharactersInRange:NSMakeRange(0, [rdline length])];
				if ([self.currentPrompt isEqualToString:@"topping"])
					[self clearPlate];
				quantityLabel.text = @"";
				break;
			case OpenTag_PT:
				[rdline appendString:@"("];
				break;
			case CloseTag_PT: 
				[rdline appendString:@")"];
				if ([tag length] != 0) [rdline appendString:tag];
				break;
			case Null_PT:
				if ([tag length] != 0) [rdline appendFormat:@" <%@>", tag];
				break;
			case Word_PT:
				[rdline appendFormat:@" %@", word];
				if ([tag length] != 0) [rdline appendFormat:@"<%@>", tag];
				[self processWord:word];
				break;
			case End_PT:
				complete = YES;
				break;
		}
		
		outLabel.text = rdline;
		outLabel.alpha = complete ? 1.0f : 0.6f;
//		[self performSelectorOnMainThread:@selector(processPacket:) withObject:[notification userInfo] waitUntilDone:YES];
	}];
	
	[[NSNotificationCenter defaultCenter] addObserverForName:@"Talk" object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *notification) {
		NSString *text = [[notification userInfo] objectForKey:@"Text"];
		NSString *tag = [[notification userInfo] objectForKey:@"Tag"];
		self.currentPrompt = tag;
		promptLabel.text = text;
		
		if ([self.currentPrompt isEqualToString:@"completed"]) {
			[[[[UIAlertView alloc] initWithTitle:@"Completed!"
										 message:@"Your order is on its way!"
										delegate:nil
							   cancelButtonTitle:@"Thanks!"
							   otherButtonTitles:nil] autorelease] show];
			
			[self performSelector:@selector(clearPlate) withObject:nil afterDelay:2.0f];
			[quantityLabel performSelector:@selector(setText:) withObject:@"" afterDelay:2.0f];
		}
	}];
	
	pthread_t thread;
	pthread_create(&thread, NULL, startSSDS, NULL);
}

- (void)viewWillAppear:(BOOL)animated {
	base.alpha = 0;
	pepperoni.alpha = 0;
	tomato.alpha = 0;
	ham.alpha = 0;
	cheese.alpha = 0;
	promptLabel.text = @"";
	outLabel.text = @"";
	quantityLabel.text = @"";
}

// Override to allow orientations other than the default portrait orientation.
- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation {
    // Return YES for supported orientations
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
}


- (void)didReceiveMemoryWarning {
	// Releases the view if it doesn't have a superview.
    [super didReceiveMemoryWarning];
	
	// Release any cached data, images, etc that aren't in use.
}

- (void)viewDidUnload {
	// Release any retained subviews of the main view.
	// e.g. self.myOutlet = nil;
}


- (void)dealloc {
    [super dealloc];
}

@end
