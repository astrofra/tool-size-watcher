#include "macos_platform.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

namespace tsw {
namespace {

NSString* const kSettingsDomain = @"com.fra.toolsizewatcher";
NSString* const kExcludeNetworkVolumesKey = @"exclude_network_volumes";
NSString* const kIncludeProtectedFoldersKey = @"include_protected_folders";

NSDictionary* ReadSettingsDomain() {
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    NSDictionary* domain = [defaults persistentDomainForName:kSettingsDomain];
    if (domain == nil) {
        return [NSDictionary dictionary];
    }
    return domain;
}

}  // namespace

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

UserSettings LoadUserSettings() {
    @autoreleasepool {
        UserSettings settings;
        NSDictionary* domain = ReadSettingsDomain();

        id exclude_network_value = [domain objectForKey:kExcludeNetworkVolumesKey];
        if (exclude_network_value != nil) {
            settings.exclude_network_volumes = [exclude_network_value boolValue];
        }

        id include_protected_value = [domain objectForKey:kIncludeProtectedFoldersKey];
        if (include_protected_value != nil) {
            settings.include_protected_folders = [include_protected_value boolValue];
        }

        return settings;
    }
}

void SaveUserSettings(const UserSettings& settings) {
    @autoreleasepool {
        NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
        NSMutableDictionary* domain = [NSMutableDictionary dictionaryWithDictionary:ReadSettingsDomain()];
        [domain setObject:[NSNumber numberWithBool:settings.exclude_network_volumes]
                   forKey:kExcludeNetworkVolumesKey];
        [domain setObject:[NSNumber numberWithBool:settings.include_protected_folders]
                   forKey:kIncludeProtectedFoldersKey];
        [defaults setPersistentDomain:domain forName:kSettingsDomain];
    }
}

}  // namespace tsw
