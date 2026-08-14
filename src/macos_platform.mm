#include "macos_platform.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

namespace tsw {

bool OpenPathInFinder(const std::string& path, bool is_directory) {
    @autoreleasepool {
        NSString* ns_path = [NSString stringWithUTF8String:path.c_str()];
        if (ns_path == nil) {
            return false;
        }

        NSURL* url = [NSURL fileURLWithPath:ns_path];
        if (url == nil) {
            return false;
        }

        NSWorkspace* workspace = [NSWorkspace sharedWorkspace];
        if (is_directory) {
            return [workspace openURL:url];
        }

        NSArray* urls = @[url];
        [workspace activateFileViewerSelectingURLs:urls];
        return true;
    }
}

}  // namespace tsw

