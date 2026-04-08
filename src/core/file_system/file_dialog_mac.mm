#include "file_dialog.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_MAC

#ifdef __OBJC__
#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#endif

namespace fem {

static NSArray<UTType*>* build_allowed_types(const DialogFilter& filter) {
    if (filter.extensions.empty()) return nil;
    
    NSMutableArray<UTType*>* types = [NSMutableArray array];
    for (const auto& extCpp : filter.extensions) {
        NSString* ext = [NSString stringWithUTF8String:extCpp.c_str()];
        if (!ext || [ext length] == 0) continue;
        
        UTType* t = [UTType typeWithFilenameExtension:ext];
        if (t) [types addObject:t];
    }
    return [types count] ? types : nil;
}

std::optional<std::filesystem::path> open_file_dialog(const DialogFilter& filter) {
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        panel.canChooseFiles = YES;
        panel.canChooseDirectories = NO;
        panel.allowsMultipleSelection = NO;
        panel.title = [NSString stringWithUTF8String:filter.description.c_str()];
        
        if (NSArray<UTType*>* types = build_allowed_types(filter)) {
            panel.allowedContentTypes = types;
        }
        
        [NSApp activateIgnoringOtherApps:YES];
        
        if ([panel runModal] == NSModalResponseOK) {
            NSURL* url = panel.URL;
            if (!url) return std::nullopt;
            return std::filesystem::path([[url path] UTF8String]);
        }
        return std::nullopt;
    }
}

std::vector<std::filesystem::path> open_files_dialog(const DialogFilter& filter) {
    std::vector<std::filesystem::path> out;
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        panel.canChooseFiles = YES;
        panel.canChooseDirectories = NO;
        panel.allowsMultipleSelection = YES;
        panel.title = [NSString stringWithUTF8String:filter.description.c_str()];
        
        if (NSArray<UTType*>* types = build_allowed_types(filter)) {
            panel.allowedContentTypes = types;
        }
        
        [NSApp activateIgnoringOtherApps:YES];
        
        if ([panel runModal] == NSModalResponseOK) {
            for (NSURL* url in panel.URLs) {
                if (url) out.emplace_back([[url path] UTF8String]);
            }
        }
    }
    return out;
}

std::optional<std::filesystem::path> save_file_dialog(const DialogFilter& filter) {
    @autoreleasepool {
        NSSavePanel* panel = [NSSavePanel savePanel];
        panel.title = @"Save File";
        
        if (NSArray<UTType*>* types = build_allowed_types(filter)) {
            panel.allowedContentTypes = types;
        }
        
        [NSApp activateIgnoringOtherApps:YES];
        
        if ([panel runModal] == NSModalResponseOK) {
            NSURL* url = panel.URL;
            if (!url) return std::nullopt;
            return std::filesystem::path([[url path] UTF8String]);
        }
        return std::nullopt;
    }
}

std::optional<std::filesystem::path> open_directory_dialog(const std::string& title) {
    @autoreleasepool {
        if (!NSApp) {
            [NSApplication sharedApplication];
        }
        
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        panel.canChooseFiles = NO;
        panel.canChooseDirectories = YES;
        panel.allowsMultipleSelection = NO;
        panel.title = [NSString stringWithUTF8String:title.c_str()];
        panel.canCreateDirectories = YES;
        panel.prompt = @"Select";
        
        [NSApp activateIgnoringOtherApps:YES];
        
        NSModalResponse response = [panel runModal];
        
        if (response == NSModalResponseOK) {
            NSURL* url = panel.URL;
            if (url) {
                const char* path_cstr = [[url path] UTF8String];
                if (path_cstr) {
                    return std::filesystem::path(path_cstr);
                }
            } else {
                // add error log that url is null
            }
        } else {
            // add error log that dialog was cancelled
        }
        
        return std::nullopt;
    }
}

} // namespace fem

#endif // TARGET_OS_MAC
#endif // __APPLE__