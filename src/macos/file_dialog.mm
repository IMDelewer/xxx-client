#include <game/client/components/menu_services/file_dialog.h>

#import <Cocoa/Cocoa.h>

SImageFileSelection SelectBackgroundImage()
{
	@autoreleasepool
	{
		NSOpenPanel *pPanel = [NSOpenPanel openPanel];
		[pPanel setTitle:@"XUI background"];
		[pPanel setCanChooseDirectories:NO];
		[pPanel setAllowsMultipleSelection:NO];
		[pPanel setAllowedFileTypes:@[@"png"]];
		if([pPanel runModal] != NSModalResponseOK)
			return {};
		const char *pPath = [[[pPanel URL] path] UTF8String];
		return pPath ? SImageFileSelection{pPath, ""} : SImageFileSelection{"", "Could not read the selected path"};
	}
}
